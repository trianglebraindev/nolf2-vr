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
//   ADJUST GUN (click the left stick while playing; click again when done)
//     Left stick              slide the gun in your hand
//     Right stick             tilt (up/down) and turn (left/right) it
//     Right grip + right stick up/down   raise / lower it
//     A / B                   bigger / smaller
//     Right trigger           fire, to check where shots go
//   Aiming always follows the controller's pointing direction (the
//   crosshair shows it); the adjusting moves the gun model so its sights
//   line up with that. Saved per weapon to vr_guns.txt in the run folder.
//
//   The gun in your hand (see GetGunPlacement), starting values for every
//   weapon before any adjusting:
//   VRGunScale      how much bigger than the game's miniature gun model
//                   (bigger number = bigger gun)
//   VRGunHandX/Y/Z  where the game's normal view holds the gun, in cm
//                   from your eye (right, up, forward). Changing these
//                   slides the gun in your hand: e.g. if the grip sits
//                   too far forward of your hand, raise VRGunHandZ.
//   VRGunPitch      tilts the gun relative to the controller, degrees
//                   (positive = barrel down)
//   VRArms          0 = just hands (default), 1 = the game's full arms
//
// HANDS (see VRMgr.h): the left hand follows the left controller. Squeeze
// the left grip with your left hand near where Cate's left hand sits on
// the weapon to hold it with both hands (steadier aim); let go to free it.
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
#include "PlayerMgr.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

CVRMgr g_VRMgr;

static VarTrack g_vtVRSnapTurn;
static VarTrack g_vtVRTurnSpeed;
static VarTrack g_vtVRSnapAngle;
static VarTrack g_vtVRMoveWithHand;
static VarTrack g_vtVRGunScale;
static VarTrack g_vtVRGunHandX;
static VarTrack g_vtVRGunHandY;
static VarTrack g_vtVRGunHandZ;
static VarTrack g_vtVRGunPitch;
static VarTrack g_vtVRArms;

static const float kGrabDistanceMeters = 0.2f;	// how close the left hand must be to grab the gun
static const float kArmLength = 0.12f;			// how much of the forearm's length is left when hiding arms
static const char* kHandFitFile = "vr_hands.txt";

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
	m_pfnSetCloseScale	= NULL;
	m_pfnBeginCrosshair	= NULL;
	m_pfnEndCrosshair	= NULL;
	m_pfnSetCrosshair	= NULL;
	m_pfnLogModel		= NULL;
	memset(m_aGunFits, 0, sizeof(m_aGunFits));
	m_nGunFits			= 0;
	m_nCurrentFit		= -1;
	m_szCurrentGun[0]	= '\0';
	m_bAdjusting		= false;
	m_hGunModel			= LTNULL;
	m_hNodeArmL = m_hNodeWristL = m_hNodeArmR = m_hNodeWristR = INVALID_MODEL_NODE;
	m_pArmLMatrix		= NULL;
	m_pArmRMatrix		= NULL;
	m_bGunPlaced		= false;
	m_bLeftAnimValid	= false;
	m_bTwoHand			= false;
	m_vTwoHandDir.Init(0.0f, 0.0f, 1.0f);
	m_bLeftOffsetValid	= false;
	m_mLeftOffset.Identity();
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
	m_pfnSetCloseScale	= (LTVR_SetCloseScaleFn)GetProcAddress(hEngine, "LTVR_SetCloseScale");
	m_pfnBeginCrosshair	= (LTVR_BeginCrosshairFn)GetProcAddress(hEngine, "LTVR_BeginCrosshair");
	m_pfnEndCrosshair	= (LTVR_EndCrosshairFn)GetProcAddress(hEngine, "LTVR_EndCrosshair");
	m_pfnSetCrosshair	= (LTVR_SetCrosshairFn)GetProcAddress(hEngine, "LTVR_SetCrosshair");
	m_pfnLogModel		= (LTVR_LogModelFn)GetProcAddress(hEngine, "LTVR_LogModel");

	g_vtVRSnapTurn.Init(g_pLTClient, "VRSnapTurn", NULL, 0.0f);
	g_vtVRTurnSpeed.Init(g_pLTClient, "VRTurnSpeed", NULL, 150.0f);
	g_vtVRSnapAngle.Init(g_pLTClient, "VRSnapAngle", NULL, 45.0f);
	g_vtVRMoveWithHand.Init(g_pLTClient, "VRMoveWithHand", NULL, 0.0f);
	g_vtVRGunScale.Init(g_pLTClient, "VRGunScale", NULL, 12.0f);
	g_vtVRGunHandX.Init(g_pLTClient, "VRGunHandX", NULL, 12.0f);
	g_vtVRGunHandY.Init(g_pLTClient, "VRGunHandY", NULL, -18.0f);
	g_vtVRGunHandZ.Init(g_pLTClient, "VRGunHandZ", NULL, 30.0f);
	g_vtVRGunPitch.Init(g_pLTClient, "VRGunPitch", NULL, 0.0f);
	g_vtVRArms.Init(g_pLTClient, "VRArms", NULL, 0.0f);

	LoadGunFits();
	LoadHandFit();
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

	if (!bPlaying && m_bAdjusting)
	{
		m_bAdjusting = false;		// left play (menu, death, level change): stop adjusting
		SaveGunFits();
	}

	// 0 = none, 1 = normal play, 2 = adjusting the gun (fire only).
	static int s_nBoundSet = 0;
	int nWantSet = bPlaying ? (m_bAdjusting ? 2 : 1) : 0;
	s_bBindingsOn = bPlaying;
	if (nWantSet != s_nBoundSet)
	{
		s_nBoundSet = nWantSet;
		if (nWantSet == 1)
		{
			SetDefaultBindings();
		}
		else if (m_pfnClearBindings)
		{
			m_pfnClearBindings();
			if (nWantSet == 2 && m_pfnSetBinding)
				m_pfnSetBinding(LTVR_RIGHT, LTVR_BTN_TRIGGER, COMMAND_ID_FIRING);
		}
	}

	UpdateMenuControls(fFrameTime);
	UpdateHudMode();
	if (bPlaying)
	{
		UpdateGunAdjust(fFrameTime);
		UpdateTwoHand();
	}
	else
	{
		m_bTwoHand = false;
	}

	if (m_pfnSetCloseScale)
		m_pfnSetCloseScale(GetGunScale());
}

void CVRMgr::Log(const char* pszFormat, ...)
{
	FILE* pFile = fopen("vr_game.log", "a");
	if (!pFile)
		return;
	va_list args;
	va_start(args, pszFormat);
	vfprintf(pFile, pszFormat, args);
	va_end(args);
	fputc('\n', pFile);
	fclose(pFile);
}

bool CVRMgr::GetHandInCamera(int nHand, LTVector& vPos, LTRotation& rRot, bool bGunPitch) const
{
	if (!IsActive() || nHand < 0 || nHand > 1)
		return false;
	const LTVR_HandState& hand = m_State.hand[nHand];
	if (!hand.active || !hand.grip.valid || !hand.aim.valid)
		return false;

	// Everything is relative to the head, so the body's position and
	// heading drop out: the camera is the head.
	LTRotation rHead = GetHeadRotation();
	LTVector vGrip(hand.grip.pos[0], hand.grip.pos[1], hand.grip.pos[2]);
	vPos = VR_Unrotate(rHead, vGrip - GetHeadOffset());

	LTRotation rAim(hand.aim.rot[0], hand.aim.rot[1], hand.aim.rot[2], hand.aim.rot[3]);
	rRot = (~rHead) * rAim;

	float fPitch = bGunPitch ? g_vtVRGunPitch.GetFloat() : 0.0f;
	if (fPitch != 0.0f)
		rRot = rRot * LTRotation(LTVector(1.0f, 0.0f, 0.0f), DEG2RAD(fPitch));
	return true;
}

float CVRMgr::GetGunScale() const
{
	const GunFit* pFit = GetCurrentFit();
	if (pFit && pFit->fScale > 0.05f)
		return pFit->fScale;
	float fScale = g_vtVRGunScale.GetFloat(12.0f);
	return (fScale > 0.05f) ? fScale : 12.0f;
}

bool CVRMgr::GetGunPlacement(const LTVector& vModelPos, LTVector& vObjPos, LTRotation& rObjRot) const
{
	LTVector vHand;
	LTRotation rHand, rGun;
	if (!ComputeGun(vHand, rHand, rGun))
		return false;

	// Positions here are in really-close units: game units / scale.
	const float fScale = GetGunScale();
	const float fUnitsPerCm = m_State.unitsPerMeter / 100.0f;
	LTVector vNatural(g_vtVRGunHandX.GetFloat() * fUnitsPerCm, g_vtVRGunHandY.GetFloat() * fUnitsPerCm,
		g_vtVRGunHandZ.GetFloat() * fUnitsPerCm);

	// This weapon's own fit: slide, along the controller's axes (the turn
	// and tilt are in rGun already, see ComputeGun).
	const GunFit* pFit = GetCurrentFit();
	if (pFit)
		vHand += VR_Rotate(rHand, LTVector(pFit->fX, pFit->fY, pFit->fZ) * fUnitsPerCm);

	// A point X of the normal view moves to: hand + gunRot * (X - natural).
	vObjPos = vHand / fScale + VR_Rotate(rGun, vModelPos - vNatural / fScale);
	rObjRot = rGun;
	return true;
}

// ----------------------------------------------------------------------- //
// Gun fit per weapon, and the "adjust gun" mode
// ----------------------------------------------------------------------- //

static const char* kGunFitFile = "vr_guns.txt";

const CVRMgr::GunFit* CVRMgr::GetCurrentFit() const
{
	return (m_nCurrentFit >= 0 && m_nCurrentFit < m_nGunFits) ? &m_aGunFits[m_nCurrentFit] : NULL;
}

CVRMgr::GunFit* CVRMgr::GetOrAddCurrentFit()
{
	if (m_nCurrentFit >= 0 && m_nCurrentFit < m_nGunFits)
		return &m_aGunFits[m_nCurrentFit];
	if (!m_szCurrentGun[0] || m_nGunFits >= kMaxGunFits)
		return NULL;
	GunFit& fit = m_aGunFits[m_nGunFits];
	memset(&fit, 0, sizeof(fit));
	strncpy(fit.szName, m_szCurrentGun, sizeof(fit.szName) - 1);
	fit.fScale = GetGunScale();
	m_nCurrentFit = m_nGunFits++;
	return &fit;
}

void CVRMgr::SetCurrentGun(const char* pszName, HOBJECT hModel)
{
	if (!pszName)
		pszName = "";
	if (strcmp(pszName, m_szCurrentGun) != 0)
	{
		if (m_bAdjusting)
		{
			m_bAdjusting = false;
			SaveGunFits();
		}
		strncpy(m_szCurrentGun, pszName, sizeof(m_szCurrentGun) - 1);
		m_szCurrentGun[sizeof(m_szCurrentGun) - 1] = '\0';
		if (IsActive())
			Log("Now holding: %s", m_szCurrentGun[0] ? m_szCurrentGun : "(nothing)");
		m_bTwoHand = false;
		m_bLeftAnimValid = false;
		m_nCurrentFit = -1;
		for (int i = 0; i < m_nGunFits; ++i)
		{
			if (!stricmp(m_aGunFits[i].szName, m_szCurrentGun))
			{
				m_nCurrentFit = i;
				break;
			}
		}
	}

	// The model's make-up, once per weapon per session.
	static char s_aLogged[kMaxGunFits][64];
	static int s_nLogged = 0;
	if (hModel && m_pfnLogModel && pszName[0])
	{
		for (int i = 0; i < s_nLogged; ++i)
		{
			if (!stricmp(s_aLogged[i], pszName))
				return;
		}
		if (s_nLogged < kMaxGunFits)
		{
			strncpy(s_aLogged[s_nLogged], pszName, 63);
			s_aLogged[s_nLogged][63] = '\0';
			++s_nLogged;
			m_pfnLogModel(hModel, pszName);
		}
	}
}

// Fits tested in the headset, used for any weapon vr_guns.txt doesn't
// mention. (Tre's, from the in-game adjust mode.)
static const struct { const char* pszName; float x, y, z, yaw, pitch, scale; } kDefaultGunFits[] =
{
	{ "Crossbow",	0.10f, 0.00f, 0.25f,	5.62f, 1.39f,	12.0f },
};

// vr_guns.txt: one line per weapon:  "Name" x y z yaw pitch scale
void CVRMgr::LoadGunFits()
{
	m_nGunFits = 0;
	FILE* pFile = fopen(kGunFitFile, "r");
	if (pFile)
	{
		char szLine[256];
		while (m_nGunFits < kMaxGunFits && fgets(szLine, sizeof(szLine), pFile))
		{
			char* pStart = strchr(szLine, '"');
			char* pEnd = pStart ? strchr(pStart + 1, '"') : NULL;
			if (!pStart || !pEnd || pEnd == pStart + 1)
				continue;
			GunFit fit;
			memset(&fit, 0, sizeof(fit));
			int nLen = (int)(pEnd - pStart - 1);
			if (nLen > (int)sizeof(fit.szName) - 1)
				nLen = (int)sizeof(fit.szName) - 1;
			strncpy(fit.szName, pStart + 1, nLen);
			fit.szName[nLen] = '\0';
			if (sscanf(pEnd + 1, "%f %f %f %f %f %f", &fit.fX, &fit.fY, &fit.fZ, &fit.fYaw, &fit.fPitch, &fit.fScale) >= 5)
				m_aGunFits[m_nGunFits++] = fit;
		}
		fclose(pFile);
	}

	// Built-in fits for weapons the file doesn't have.
	for (int d = 0; d < (int)(sizeof(kDefaultGunFits) / sizeof(kDefaultGunFits[0])) && m_nGunFits < kMaxGunFits; ++d)
	{
		bool bHave = false;
		for (int i = 0; i < m_nGunFits; ++i)
		{
			if (!stricmp(m_aGunFits[i].szName, kDefaultGunFits[d].pszName))
				bHave = true;
		}
		if (bHave)
			continue;
		GunFit& fit = m_aGunFits[m_nGunFits++];
		memset(&fit, 0, sizeof(fit));
		strncpy(fit.szName, kDefaultGunFits[d].pszName, sizeof(fit.szName) - 1);
		fit.fX = kDefaultGunFits[d].x;
		fit.fY = kDefaultGunFits[d].y;
		fit.fZ = kDefaultGunFits[d].z;
		fit.fYaw = kDefaultGunFits[d].yaw;
		fit.fPitch = kDefaultGunFits[d].pitch;
		fit.fScale = kDefaultGunFits[d].scale;
	}
}

void CVRMgr::SaveGunFits()
{
	FILE* pFile = fopen(kGunFitFile, "w");
	if (!pFile)
		return;
	fprintf(pFile, "# NOLF2 VR: how each gun sits in your hand. Made by the in-game\n");
	fprintf(pFile, "# \"adjust gun\" mode (click the left stick while playing).\n");
	fprintf(pFile, "# \"weapon\"  slide right / up / forward (cm)  turn  tilt (degrees)  size\n");
	for (int i = 0; i < m_nGunFits; ++i)
	{
		const GunFit& fit = m_aGunFits[i];
		fprintf(pFile, "\"%s\"  %.2f %.2f %.2f  %.2f %.2f  %.2f\n", fit.szName, fit.fX, fit.fY, fit.fZ,
			fit.fYaw, fit.fPitch, fit.fScale);
	}
	fclose(pFile);
}

void CVRMgr::Message(const char* pszText)
{
	if (g_pChatMsgs && pszText)
		g_pChatMsgs->AddMessage(pszText);
}

void CVRMgr::UpdateGunAdjust(float fFrameTime)
{
	// Click the left stick: start / finish adjusting.
	if (Pressed(LTVR_LEFT, LTVR_BTN_STICK) && !IsDecisionOpen())
	{
		if (!m_bAdjusting)
		{
			if (!m_szCurrentGun[0] || !GetOrAddCurrentFit())
				return;
			m_bAdjusting = true;
			Message("ADJUST GUN: left stick slides it, right stick tilts and turns it.");
			Message("Right grip + right stick up/down raises it. A / B: bigger / smaller.");
			Message("Line the sights up with the crosshair. Click the left stick when done.");
		}
		else
		{
			m_bAdjusting = false;
			SaveGunFits();
			char szMsg[128];
			_snprintf(szMsg, sizeof(szMsg), "Gun fit saved (%s).", m_szCurrentGun);
			szMsg[sizeof(szMsg) - 1] = '\0';
			Message(szMsg);
		}
		return;
	}
	if (!m_bAdjusting)
		return;
	GunFit* pFit = GetOrAddCurrentFit();
	if (!pFit)
	{
		m_bAdjusting = false;
		return;
	}

	const float kSlideCmPerSec = 6.0f;
	const float kTurnDegPerSec = 15.0f;

	float lx = m_State.hand[LTVR_LEFT].stick[0], ly = m_State.hand[LTVR_LEFT].stick[1];
	float rx = m_State.hand[LTVR_RIGHT].stick[0], ry = m_State.hand[LTVR_RIGHT].stick[1];
	ApplyDeadZone(lx, ly);
	ApplyDeadZone(rx, ry);

	pFit->fX += lx * kSlideCmPerSec * fFrameTime;
	pFit->fZ += ly * kSlideCmPerSec * fFrameTime;
	if (m_State.hand[LTVR_RIGHT].buttons & LTVR_BTN_GRIP)
	{
		pFit->fY += ry * kSlideCmPerSec * fFrameTime;
	}
	else
	{
		pFit->fYaw += rx * kTurnDegPerSec * fFrameTime;
		pFit->fPitch -= ry * kTurnDegPerSec * fFrameTime;	// stick up = barrel up
	}

	if (pFit->fScale <= 0.05f)
		pFit->fScale = GetGunScale();
	if (m_State.hand[LTVR_RIGHT].buttons & LTVR_BTN_PRIMARY)
		pFit->fScale *= 1.0f + 0.4f * fFrameTime;
	if (m_State.hand[LTVR_RIGHT].buttons & LTVR_BTN_SECONDARY)
		pFit->fScale /= 1.0f + 0.4f * fFrameTime;
	if (pFit->fScale < 1.0f)	pFit->fScale = 1.0f;
	if (pFit->fScale > 60.0f)	pFit->fScale = 60.0f;
}

// ----------------------------------------------------------------------- //
// Hands
// ----------------------------------------------------------------------- //

bool CVRMgr::ComputeGun(LTVector& vHand, LTRotation& rAim, LTRotation& rGun) const
{
	if (!GetHandInCamera(LTVR_RIGHT, vHand, rAim))
		return false;

	// This weapon's fit: turn and tilt about the hand.
	rGun = rAim;
	const GunFit* pFit = GetCurrentFit();
	if (pFit)
	{
		rGun = rAim * LTRotation(LTVector(0.0f, 1.0f, 0.0f), DEG2RAD(pFit->fYaw)) *
			LTRotation(LTVector(1.0f, 0.0f, 0.0f), DEG2RAD(pFit->fPitch));
	}

	// Two hands: turn the gun (and the aim with it) about the right hand so
	// the line from the right hand to the left keeps the direction, relative
	// to the gun, that it had when the left hand grabbed on.
	if (m_bTwoHand)
	{
		LTVector vLeft;
		LTRotation rLeft;
		if (GetHandInCamera(LTVR_LEFT, vLeft, rLeft, false))
		{
			LTVector vWant = vLeft - vHand;
			float fLen = vWant.Mag();
			if (fLen > 0.001f)
			{
				vWant /= fLen;
				LTRotation rTurn = VR_Arc(VR_Rotate(rGun, m_vTwoHandDir), vWant);
				rGun = rTurn * rGun;
				rAim = rTurn * rAim;
			}
		}
	}
	return true;
}

bool CVRMgr::HideArms() const
{
	return IsActive() && g_vtVRArms.GetFloat() < 0.5f;
}

void CVRMgr::AttachHands(HOBJECT hGunModel)
{
	m_hGunModel = hGunModel;
	m_hNodeArmL = m_hNodeWristL = m_hNodeArmR = m_hNodeWristR = INVALID_MODEL_NODE;
	m_pArmLMatrix = m_pArmRMatrix = NULL;
	m_bLeftAnimValid = false;
	m_bGunPlaced = false;
	if (!hGunModel)
		return;

	// The bones every first-person model in NOLF2 shares (see vr_models.log).
	struct { const char* pszName; HMODELNODE* phNode; } aNodes[] = {
		{ "armL",		&m_hNodeArmL },
		{ "wristL",		&m_hNodeWristL },
		{ "forearmR",	&m_hNodeArmR },
		{ "wristR",		&m_hNodeWristR } };

	// Remove-then-add, so a model is never hooked twice (weapon models are
	// made afresh on every switch, and this can be called more than once).
	for (int i = 0; i < 4; ++i)
	{
		HMODELNODE hNode = INVALID_MODEL_NODE;
		if (LT_OK == g_pModelLT->GetNode(hGunModel, aNodes[i].pszName, hNode))
		{
			*aNodes[i].phNode = hNode;
			g_pModelLT->RemoveNodeControlFn(hGunModel, hNode, HandsNodeControl, this);
			g_pModelLT->AddNodeControlFn(hGunModel, hNode, HandsNodeControl, this);
		}
	}
}

void CVRMgr::SetGunModelTransform(const LTVector& vPos, const LTRotation& rRot)
{
	m_vGunObjPos = vPos;
	m_rGunObjRot = rRot;
	m_bGunPlaced = true;
}

// A controller's pose in the gun model's own space, as a matrix.
static void VR_PoseInModel(const LTVector& vCam, const LTRotation& rCam, float fScale,
	const LTVector& vGunPos, const LTRotation& rGunRot, LTMatrix& mOut)
{
	LTVector vModel = VR_Unrotate(rGunRot, vCam / fScale - vGunPos);
	LTRotation rModel = (~rGunRot) * rCam;
	rModel.ConvertToMatrix(mOut);
	mOut.SetTranslation(vModel);
}

bool CVRMgr::GetLeftControllerInModel(LTMatrix& mCtrl) const
{
	LTVector vLeft;
	LTRotation rLeft;
	if (!m_bGunPlaced || !GetHandInCamera(LTVR_LEFT, vLeft, rLeft, false))
		return false;
	VR_PoseInModel(vLeft, rLeft, GetGunScale(), m_vGunObjPos, m_rGunObjRot, mCtrl);
	return true;
}

// Where the left wrist goes when the left hand is free (model space):
// the controller, times how the hand sits on it.
bool CVRMgr::GetLeftWristTarget(LTMatrix& mWrist) const
{
	if (!m_bGunPlaced || !m_bLeftAnimValid)
		return false;
	LTMatrix mCtrl;
	if (!GetLeftControllerInModel(mCtrl))
		return false;

	LTMatrix mOffset = m_mLeftOffset;
	if (!m_bLeftOffsetValid)
	{
		// Until the first two-hand grab teaches it: the hand turned the way
		// Cate's left hand is on the weapon, as if the left controller
		// pointed the same way as the right one.
		LTVector vRight;
		LTRotation rRight;
		if (!GetHandInCamera(LTVR_RIGHT, vRight, rRight, false))
			return false;
		LTMatrix mRight;
		VR_PoseInModel(vRight, rRight, GetGunScale(), m_vGunObjPos, m_rGunObjRot, mRight);
		mRight.SetTranslation(0.0f, 0.0f, 0.0f);
		mOffset = mRight.MakeInverse() * m_mLeftWristAnim;
		mOffset.SetTranslation(0.0f, 0.0f, 0.0f);
	}

	mWrist = mCtrl * mOffset;
	return true;
}

// vr_hands.txt: how the left hand sits on the left controller, learnt at
// each two-hand grab (3 rows of 4 numbers).
void CVRMgr::LoadHandFit()
{
	FILE* pFile = fopen(kHandFitFile, "r");
	if (!pFile)
		return;
	char szLine[512];
	while (fgets(szLine, sizeof(szLine), pFile))
	{
		if (szLine[0] == '#')
			continue;
		LTMatrix m;
		m.Identity();
		if (sscanf(szLine, "%f %f %f %f %f %f %f %f %f %f %f %f",
			&m.m[0][0], &m.m[0][1], &m.m[0][2], &m.m[0][3],
			&m.m[1][0], &m.m[1][1], &m.m[1][2], &m.m[1][3],
			&m.m[2][0], &m.m[2][1], &m.m[2][2], &m.m[2][3]) == 12)
		{
			m_mLeftOffset = m;
			m_bLeftOffsetValid = true;
			break;
		}
	}
	fclose(pFile);
}

void CVRMgr::SaveHandFit()
{
	FILE* pFile = fopen(kHandFitFile, "w");
	if (!pFile)
		return;
	fprintf(pFile, "# NOLF2 VR: how the left hand sits on the left controller.\n");
	fprintf(pFile, "# Learnt from your last two-hand grab (the hand on the weapon).\n");
	const LTMatrix& m = m_mLeftOffset;
	fprintf(pFile, "%f %f %f %f  %f %f %f %f  %f %f %f %f\n",
		m.m[0][0], m.m[0][1], m.m[0][2], m.m[0][3],
		m.m[1][0], m.m[1][1], m.m[1][2], m.m[1][3],
		m.m[2][0], m.m[2][1], m.m[2][2], m.m[2][3]);
	fclose(pFile);
}

// Left grip near Cate's left hand: hold the gun with both hands.
void CVRMgr::UpdateTwoHand()
{
	bool bGrip = (m_State.hand[LTVR_LEFT].buttons & LTVR_BTN_GRIP) != 0;
	if (m_bTwoHand && (!bGrip || m_bAdjusting))
		m_bTwoHand = false;
	if (m_bTwoHand || m_bAdjusting || !Pressed(LTVR_LEFT, LTVR_BTN_GRIP) || !m_bGunPlaced || !m_bLeftAnimValid)
		return;

	LTVector vLeft, vRight;
	LTRotation rLeft, rAim, rGun;
	if (!GetHandInCamera(LTVR_LEFT, vLeft, rLeft, false) || !ComputeGun(vRight, rAim, rGun))
		return;

	// Where Cate's left hand is on the weapon, in camera space.
	const float fScale = GetGunScale();
	LTVector vAnim;
	m_mLeftWristAnim.GetTranslation(vAnim);
	LTVector vAnimCam = (m_vGunObjPos + VR_Rotate(m_rGunObjRot, vAnim)) * fScale;
	float fUnitsPerMeter = m_State.unitsPerMeter > 1.0f ? m_State.unitsPerMeter : 60.0f;
	if ((vLeft - vAnimCam).Mag() / fUnitsPerMeter > kGrabDistanceMeters)
		return;

	// Each grab teaches how the hand sits on its controller (the way you
	// hold it when gripping the weapon): the free left hand uses it after
	// you let go, and it's saved for next time.
	{
		LTMatrix mCtrl;
		VR_PoseInModel(vLeft, rLeft, fScale, m_vGunObjPos, m_rGunObjRot, mCtrl);
		m_mLeftOffset = mCtrl.MakeInverse() * m_mLeftWristAnim;
		m_bLeftOffsetValid = true;
		SaveHandFit();
	}

	LTVector vDir = vLeft - vRight;
	float fLen = vDir.Mag();
	if (fLen < 0.001f)
		return;
	m_vTwoHandDir = VR_Unrotate(rGun, vDir / fLen);
	m_bTwoHand = true;
}

// Called by the engine while it works out the gun model's skeleton, once
// per hooked bone, parents before children.
void CVRMgr::HandsNodeControl(const NodeControlData& data, void* pUser)
{
	CVRMgr* p = (CVRMgr*)pUser;
	if (!p || !data.m_pNodeTransform || data.m_hModel != p->m_hGunModel)
		return;
	const HMODELNODE hNode = data.m_hNode;

	if (hNode == p->m_hNodeArmL)
	{
		p->m_pArmLMatrix = data.m_pNodeTransform;
		return;
	}
	if (hNode == p->m_hNodeArmR)
	{
		p->m_pArmRMatrix = data.m_pNodeTransform;
		return;
	}

	LTMatrix** ppArm = NULL;
	if (hNode == p->m_hNodeWristL)
	{
		ppArm = &p->m_pArmLMatrix;
		p->m_mLeftWristAnim = *data.m_pNodeTransform;
		p->m_bLeftAnimValid = true;

		// Free left hand: put it on the left controller (the fingers, which
		// hang off the wrist, come along). The forearm moves with it, keeping
		// the same hold on the hand as in the animation; otherwise its skin
		// stretches from where the animation left it to the hand.
		if (p->IsActive() && !p->m_bTwoHand)
		{
			LTMatrix mWrist;
			if (p->GetLeftWristTarget(mWrist))
			{
				if (p->m_pArmLMatrix)
				{
					LTMatrix mMove = mWrist * data.m_pNodeTransform->MakeInverse();
					*p->m_pArmLMatrix = mMove * (*p->m_pArmLMatrix);
				}
				*data.m_pNodeTransform = mWrist;
			}
		}
	}
	else if (hNode == p->m_hNodeWristR)
	{
		ppArm = &p->m_pArmRMatrix;
	}
	else
	{
		return;
	}

	// Hands only: shorten the forearm to a cuff at the wrist. Only its
	// length is squashed (along the bone, its X axis), not its thickness,
	// so the skin between forearm and hand blends into a short cuff rather
	// than a spike. Its children (the hand) have been placed already, so
	// only the forearm's own skin is affected.
	if (*ppArm && p->HideArms())
	{
		LTMatrix& mArm = **ppArm;
		LTVector vWrist;
		data.m_pNodeTransform->GetTranslation(vWrist);
		LTVector vWristInArm = mArm.MakeInverse() * vWrist;

		// mArm * T(w) * S(k,1,1) * T(-w): about the wrist, along the bone.
		LTMatrix mSquash;
		mSquash.Identity();
		mSquash.m[0][0] = kArmLength;
		mSquash.m[0][3] = vWristInArm.x * (1.0f - kArmLength);
		mArm = mArm * mSquash;
	}
	*ppArm = NULL;
}

bool CVRMgr::GetAimRay(LTVector& vFrom, LTRotation& rRot) const
{
	LTVector vHand;
	LTRotation rHand, rGun;
	if (!g_pPlayerMgr || !ComputeGun(vHand, rHand, rGun))
		return false;
	HOBJECT hCamera = g_pPlayerMgr->GetCamera();
	if (!hCamera)
		return false;

	LTVector vCamPos;
	LTRotation rCamRot;
	g_pLTClient->GetObjectPos(hCamera, &vCamPos);
	g_pLTClient->GetObjectRotation(hCamera, &rCamRot);
	vFrom = vCamPos + VR_Rotate(rCamRot, vHand);
	rRot = rCamRot * rHand;
	return true;
}

int CVRMgr::BeginCrosshair()
{
	return m_pfnBeginCrosshair ? m_pfnBeginCrosshair() : 0;
}

void CVRMgr::EndCrosshair()
{
	if (m_pfnEndCrosshair)
		m_pfnEndCrosshair();
}

void CVRMgr::SetCrosshair(bool bVisible, const LTVector& vPos)
{
	if (!m_pfnSetCrosshair)
		return;
	float aPos[3] = { vPos.x, vPos.y, vPos.z };
	m_pfnSetCrosshair(bVisible ? 1 : 0, aPos);
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
	if (!IsActive() || m_bAdjusting)
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
	if (!IsActive() || m_bAdjusting)
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
