// ----------------------------------------------------------------------- //
//
// MODULE  : VRMgr.cpp
//
// PURPOSE : The game's side of VR. See VRMgr.h.
//
// DEFAULT CONTROLS (right-handed)
//
//   Left stick      walk/run (light push walks quietly, full push runs)
//   Left trigger    use / activate
//   Left X          mission objectives
//   Left Y / menu   pause menu
//   Right stick     turn (left/right); up = jump, down = crouch toggle
//   Right trigger   fire
//   Right A         next ammo type
//   Right B         holster
//   Right stick     click = reload
//   Right grip      next weapon (until the weapon ring exists)
//
//   In menus: either stick moves the selection, trigger or A selects,
//   B goes back.
//
//   Conversation choices (e.g. the crow robot): right stick up/down
//   moves the highlight, right trigger or A picks it, B says nothing.
//   Fire, jump and crouch are off while the choices are up; you can
//   still walk and turn.
//
// INTERFACE
//
//   While the world is in the headset, the game's interface is shown
//   over it: the HUD fixed in front of your eyes while playing, and
//   in-game menus, popups and message boxes on a panel that appears
//   where you're looking and stays put. Screen fades darken the whole
//   view. The cutscene letterbox bars are left out in VR.
//
// SETTINGS (console variables; add to run_nolf2_vr.bat as +Name value)
//
//   VRSnapTurn      0 = smooth turning (default), 1 = snap turning
//   VRTurnSpeed     smooth turn speed, degrees per second (default 150)
//   VRSnapAngle     snap turn step, degrees (default 45)
//   VRMoveWithHand  0 = walk where you look (default),
//                   1 = walk where the left controller points
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "VRMgr.h"
#include "CommandIDs.h"
#include "GameClientShell.h"
#include "InterfaceMgr.h"
#include "VarTrack.h"
#include "HUDMgr.h"
#include "HUDDecision.h"

CVRMgr g_VRMgr;

static VarTrack g_vtVRSnapTurn;
static VarTrack g_vtVRTurnSpeed;
static VarTrack g_vtVRSnapAngle;
static VarTrack g_vtVRMoveWithHand;

static const float kStickDeadZone = 0.15f;

// Applies a radial dead zone and rescales so motion starts smoothly at
// the edge of it. Returns the stick length after rescaling (0..1).
static float ApplyDeadZone(float& x, float& y)
{
	float fLen = (float)sqrt(x * x + y * y);
	if (fLen < kStickDeadZone)
	{
		x = y = 0.0f;
		return 0.0f;
	}
	float fScaled = (fLen - kStickDeadZone) / (1.0f - kStickDeadZone);
	if (fScaled > 1.0f)
		fScaled = 1.0f;
	x = x / fLen * fScaled;
	y = y / fLen * fScaled;
	return fScaled;
}

// Heading of a rotation (LithTech: 0 = +Z, positive turns toward +X).
static float YawOf(const LTRotation& rRot)
{
	LTVector vForward = rRot.Forward();
	return (float)atan2(vForward.x, vForward.z);
}

CVRMgr::CVRMgr()
{
	m_bInitialized		= false;
	m_pfnGetState		= NULL;
	m_pfnSetOrigin		= NULL;
	m_pfnSetBinding		= NULL;
	m_pfnClearBindings	= NULL;
	m_pfnRecenter		= NULL;
	m_pfnSetHudMode		= NULL;
	m_pfnSetFade		= NULL;
	m_pfnIsHudFrame		= NULL;
	memset(&m_State, 0, sizeof(m_State));
	m_aPrevButtons[0] = m_aPrevButtons[1] = 0;
	m_bSnapLatched		= false;
	m_nRepeatKey		= 0;
	m_fRepeatTimer		= 0.0f;
}

void CVRMgr::Init()
{
	m_bInitialized = true;

	// The engine exports these (Lithtech.exe). Missing = an engine without
	// VR support; everything here then stays off.
	HMODULE hEngine = GetModuleHandle(NULL);
	m_pfnGetState		= (LTVR_GetStateFn)GetProcAddress(hEngine, "LTVR_GetState");
	m_pfnSetOrigin		= (LTVR_SetOriginFn)GetProcAddress(hEngine, "LTVR_SetOrigin");
	m_pfnSetBinding		= (LTVR_SetBindingFn)GetProcAddress(hEngine, "LTVR_SetBinding");
	m_pfnClearBindings	= (LTVR_ClearBindingsFn)GetProcAddress(hEngine, "LTVR_ClearBindings");
	m_pfnRecenter		= (LTVR_RecenterFn)GetProcAddress(hEngine, "LTVR_Recenter");
	m_pfnSetHudMode		= (LTVR_SetHudModeFn)GetProcAddress(hEngine, "LTVR_SetHudMode");
	m_pfnSetFade		= (LTVR_SetFadeFn)GetProcAddress(hEngine, "LTVR_SetFade");
	m_pfnIsHudFrame		= (LTVR_IsHudFrameFn)GetProcAddress(hEngine, "LTVR_IsHudFrame");

	g_vtVRSnapTurn.Init(g_pLTClient, "VRSnapTurn", NULL, 0.0f);
	g_vtVRTurnSpeed.Init(g_pLTClient, "VRTurnSpeed", NULL, 150.0f);
	g_vtVRSnapAngle.Init(g_pLTClient, "VRSnapAngle", NULL, 45.0f);
	g_vtVRMoveWithHand.Init(g_pLTClient, "VRMoveWithHand", NULL, 0.0f);
}

void CVRMgr::SetDefaultBindings()
{
	if (!m_pfnSetBinding || !m_pfnClearBindings)
		return;

	m_pfnClearBindings();

	m_pfnSetBinding(LTVR_LEFT,  LTVR_BTN_TRIGGER,		COMMAND_ID_ACTIVATE);
	m_pfnSetBinding(LTVR_LEFT,  LTVR_BTN_PRIMARY,		COMMAND_ID_MISSION);

	m_pfnSetBinding(LTVR_RIGHT, LTVR_BTN_TRIGGER,		COMMAND_ID_FIRING);
	m_pfnSetBinding(LTVR_RIGHT, LTVR_BTN_PRIMARY,		COMMAND_ID_NEXT_AMMO);
	m_pfnSetBinding(LTVR_RIGHT, LTVR_BTN_SECONDARY,		COMMAND_ID_HOLSTER);
	m_pfnSetBinding(LTVR_RIGHT, LTVR_BTN_STICK,			COMMAND_ID_RELOAD);
	m_pfnSetBinding(LTVR_RIGHT, LTVR_BTN_STICK_UP,		COMMAND_ID_JUMP);
	m_pfnSetBinding(LTVR_RIGHT, LTVR_BTN_STICK_DOWN,	COMMAND_ID_DUCKLOCK);
	m_pfnSetBinding(LTVR_RIGHT, LTVR_BTN_GRIP,			COMMAND_ID_NEXT_WEAPON);
}

void CVRMgr::Update(float fFrameTime)
{
	if (!m_bInitialized)
		Init();

	m_aPrevButtons[0] = m_State.hand[0].buttons;
	m_aPrevButtons[1] = m_State.hand[1].buttons;

	if (!m_pfnGetState || !m_pfnGetState(&m_State))
	{
		memset(&m_State, 0, sizeof(m_State));
		return;
	}

	// Gameplay bindings only while playing; in menus the controllers drive
	// the menu instead (and e.g. the trigger mustn't also count as "fire").
	static bool s_bBindingsOn = false;
	bool bPlaying = g_pInterfaceMgr && g_pInterfaceMgr->GetGameState() == GS_PLAYING && !IsDecisionOpen();

	// Coming back to play (closing a menu or picking a conversation answer
	// with the trigger): wait until the buttons are let go, or the same
	// press would also fire or jump.
	// (The left stick's directions don't count: that stick walks.)
	const unsigned kButtons = LTVR_BTN_TRIGGER | LTVR_BTN_GRIP | LTVR_BTN_PRIMARY | LTVR_BTN_SECONDARY | LTVR_BTN_STICK;
	const unsigned kStickDirs = LTVR_BTN_STICK_UP | LTVR_BTN_STICK_DOWN;
	if (bPlaying && !s_bBindingsOn &&
		((m_State.hand[LTVR_LEFT].buttons & kButtons) || (m_State.hand[LTVR_RIGHT].buttons & (kButtons | kStickDirs))))
	{
		bPlaying = false;
	}

	if (bPlaying != s_bBindingsOn)
	{
		s_bBindingsOn = bPlaying;
		if (bPlaying)
			SetDefaultBindings();
		else if (m_pfnClearBindings)
			m_pfnClearBindings();
	}

	UpdateMenuControls(fFrameTime);
	UpdateHudMode();
}

void CVRMgr::UpdateHudMode()
{
	if (!m_pfnSetHudMode || !g_pInterfaceMgr)
		return;

	// Menus and messages you read and click go on a panel that stays put;
	// everything else (the HUD) stays in front of your eyes.
	GameState eState = g_pInterfaceMgr->GetGameState();
	bool bPanel = (eState == GS_MENU || eState == GS_POPUP || g_pInterfaceMgr->IsMessageBoxVisible());
	m_pfnSetHudMode(bPanel ? LTVR_HUD_PANEL : LTVR_HUD_HEAD);
}

bool CVRMgr::IsHudFrame() const
{
	return m_pfnIsHudFrame && m_pfnIsHudFrame() != 0;
}

void CVRMgr::SetFade(float fAlpha)
{
	if (m_pfnSetFade)
		m_pfnSetFade(fAlpha);
}

bool CVRMgr::Pressed(int nHand, unsigned nButton) const
{
	return (m_State.hand[nHand].buttons & nButton) && !(m_aPrevButtons[nHand] & nButton);
}

void CVRMgr::SendKey(int nKey)
{
	if (!g_pGameClientShell)
		return;
	g_pGameClientShell->VR_SendKey(nKey);
}

void CVRMgr::UpdateMenuControls(float fFrameTime)
{
	if (!g_pInterfaceMgr)
		return;

	GameState eState = g_pInterfaceMgr->GetGameState();

	if (eState == GS_PLAYING)
	{
		// Pause menu.
		if (Pressed(LTVR_LEFT, LTVR_BTN_SECONDARY) || Pressed(LTVR_LEFT, LTVR_BTN_MENU))
			SendKey(VK_ESCAPE);
		if (IsDecisionOpen())
			UpdateDecisionControls(fFrameTime);
		else
			m_nRepeatKey = 0;
		return;
	}

	if (eState == GS_LOADINGLEVEL || eState == GS_EXITINGLEVEL)
		return;

	// Select / back.
	if (Pressed(LTVR_RIGHT, LTVR_BTN_TRIGGER) || Pressed(LTVR_LEFT, LTVR_BTN_TRIGGER) ||
		Pressed(LTVR_RIGHT, LTVR_BTN_PRIMARY))
		SendKey(VK_RETURN);
	if (Pressed(LTVR_RIGHT, LTVR_BTN_SECONDARY) || Pressed(LTVR_LEFT, LTVR_BTN_SECONDARY) ||
		Pressed(LTVR_LEFT, LTVR_BTN_MENU))
		SendKey(VK_ESCAPE);

	// Either stick moves the selection, repeating while held.
	int nKey = StickRepeat(m_State.hand[0].buttons | m_State.hand[1].buttons, fFrameTime);
	if (nKey)
		SendKey(nKey);
}

// Turns stick directions into arrow keys: one on the push, then repeating
// while held. Returns the key to send this frame (0 for none).
int CVRMgr::StickRepeat(unsigned nDirs, float fFrameTime)
{
	int nKey = 0;
	if (nDirs & LTVR_BTN_STICK_UP)			nKey = VK_UP;
	else if (nDirs & LTVR_BTN_STICK_DOWN)	nKey = VK_DOWN;
	else if (nDirs & LTVR_BTN_STICK_LEFT)	nKey = VK_LEFT;
	else if (nDirs & LTVR_BTN_STICK_RIGHT)	nKey = VK_RIGHT;

	if (nKey != m_nRepeatKey)
	{
		m_nRepeatKey = nKey;
		m_fRepeatTimer = 0.4f;		// delay before repeating
		return nKey;
	}
	if (nKey)
	{
		m_fRepeatTimer -= fFrameTime;
		if (m_fRepeatTimer <= 0.0f)
		{
			m_fRepeatTimer = 0.12f;	// repeat rate
			return nKey;
		}
	}
	return 0;
}

bool CVRMgr::IsDecisionOpen() const
{
	return g_pDecision && g_pDecision->IsVisible();
}

void CVRMgr::UpdateDecisionControls(float fFrameTime)
{
	// Right stick up/down only (left/right still turns you).
	unsigned nDirs = m_State.hand[LTVR_RIGHT].buttons & (LTVR_BTN_STICK_UP | LTVR_BTN_STICK_DOWN);
	int nKey = StickRepeat(nDirs, fFrameTime);
	if (nKey == VK_UP)
		g_pDecision->VR_MoveSelection(-1);
	else if (nKey == VK_DOWN)
		g_pDecision->VR_MoveSelection(1);

	if (Pressed(LTVR_RIGHT, LTVR_BTN_TRIGGER) || Pressed(LTVR_RIGHT, LTVR_BTN_PRIMARY))
		g_pDecision->VR_ChooseSelection();
	else if (Pressed(LTVR_RIGHT, LTVR_BTN_SECONDARY))
		g_pDecision->Choose(MAX_DECISION_CHOICES);	// say nothing, like Escape
}

LTVector CVRMgr::GetHeadOffset() const
{
	return LTVector(m_State.head.pos[0], m_State.head.pos[1], m_State.head.pos[2]);
}

LTRotation CVRMgr::GetHeadRotation() const
{
	return LTRotation(m_State.head.rot[0], m_State.head.rot[1], m_State.head.rot[2], m_State.head.rot[3]);
}

float CVRMgr::GetHeadYaw() const
{
	return YawOf(GetHeadRotation());
}

float CVRMgr::GetHeadPitch() const
{
	LTVector vForward = GetHeadRotation().Forward();
	float fY = vForward.y;
	if (fY > 1.0f) fY = 1.0f;
	if (fY < -1.0f) fY = -1.0f;
	return -(float)asin(fY);
}

void CVRMgr::SetOrigin(const LTVector& vEyePos, float fBodyYaw)
{
	if (!m_pfnSetOrigin)
		return;
	float aPos[3] = { vEyePos.x, vEyePos.y, vEyePos.z };
	m_pfnSetOrigin(aPos, fBodyYaw);
}

float CVRMgr::GetTurnDelta(float fFrameTime)
{
	if (!IsActive())
		return 0.0f;

	float fX = m_State.hand[LTVR_RIGHT].stick[0];

	if (g_vtVRSnapTurn.GetFloat() > 0.5f)
	{
		// One step per push; the stick has to come back before the next.
		if (m_bSnapLatched)
		{
			if (fabs(fX) < 0.3f)
				m_bSnapLatched = false;
			return 0.0f;
		}
		if (fabs(fX) > 0.7f)
		{
			m_bSnapLatched = true;
			float fStep = DEG2RAD(g_vtVRSnapAngle.GetFloat(45.0f));
			return fX > 0.0f ? fStep : -fStep;
		}
		return 0.0f;
	}

	// Smooth: only the sideways part of the stick turns, with its own dead
	// zone so pushing up/down (jump/crouch) doesn't turn you.
	if (fabs(fX) < kStickDeadZone)
		return 0.0f;
	float fAmount = (fabs(fX) - kStickDeadZone) / (1.0f - kStickDeadZone);
	if (fAmount > 1.0f)
		fAmount = 1.0f;
	fAmount *= fAmount;	// finer control near the centre
	float fSpeed = DEG2RAD(g_vtVRTurnSpeed.GetFloat(150.0f));
	return (fX > 0.0f ? fAmount : -fAmount) * fSpeed * fFrameTime;
}

LTVector CVRMgr::GetMoveVector(float fBodyYaw) const
{
	if (!IsActive())
		return LTVector(0.0f, 0.0f, 0.0f);

	float fX = m_State.hand[LTVR_LEFT].stick[0];
	float fY = m_State.hand[LTVR_LEFT].stick[1];
	if (ApplyDeadZone(fX, fY) <= 0.0f)
		return LTVector(0.0f, 0.0f, 0.0f);

	// Which way is "forward" for the stick.
	float fYaw = fBodyYaw;
	const LTVR_HandState& left = m_State.hand[LTVR_LEFT];
	if (g_vtVRMoveWithHand.GetFloat() > 0.5f && left.aim.valid)
		fYaw += YawOf(LTRotation(left.aim.rot[0], left.aim.rot[1], left.aim.rot[2], left.aim.rot[3]));
	else
		fYaw += GetHeadYaw();

	LTVector vForward((float)sin(fYaw), 0.0f, (float)cos(fYaw));
	LTVector vRight((float)cos(fYaw), 0.0f, -(float)sin(fYaw));
	return vForward * fY + vRight * fX;
}

void CVRMgr::Recenter()
{
	if (m_pfnRecenter)
		m_pfnRecenter();
}
