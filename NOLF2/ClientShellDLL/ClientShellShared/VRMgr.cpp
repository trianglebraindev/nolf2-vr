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
//   Right stick     click = reload (on letting go)
//   Both stick clicks held 1 s: set your height and face forward (where
//                   your head is now becomes Cate standing up)
//   Right grip      hold: weapon ring (move your hand to one, let go)
//   Left grip       on the weapon: hold it with both hands;
//                   anywhere else: pick up / drop a body (where the
//                   crosshair is), like the game's "move body" key
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
//   VRWristBack     the free left hand: how far the wrist sits behind the
//                   middle of the controller's handle, cm (default 7)
//   VRHandPalm      ... and how far the hand sits off the handle toward
//                   the back of the hand, cm (default 3.5)
//
//   Wrist and gun HUD (health and armour on the left wrist, ammo on the
//   gun; see RenderHudItem):
//   VRWristHud      1 = health on the wrist (default), 0 = on the HUD panel
//   VRWristHudWatch 1 = like a watch: lies on the back of the wrist and
//                   shows as you turn it toward you (default);
//                   0 = always shown, facing you
//   VRWristHudX/Y/Z where it sits, cm from the left controller's grip
//                   (right, up, forward along the controller; -X is the
//                   back of the hand)
//   VRWristHudSize  its width, cm
//   VRGunHud        1 = ammo on the gun (default), 0 = on the HUD panel
//   VRGunHudX/Y/Z   where it floats, cm from the right hand (right, up,
//                   forward along the gun)
//   VRGunHudSize    its width, cm
//
//   Scopes (see UpdateScope):
//   VRScope         1 = look through a scope by raising it to your eye
//                   (default), 0 = off
//   VRScopeHeight   how far above the aim line the scope's own line runs,
//                   cm (default 8: the scope sits on top of the gun)
//   VRScopeEye      how close (cm) an eye must be to the scope's line to
//                   look through it (default 5)
//   VRScopeDist     how far in front of that eye the lens sits, cm (8)
//   VRScopeSize     how wide the lens looks, cm (5.5)
//   VRScopeZoom     magnification of a level-1 scope (4); stronger scopes
//                   are 2x and 4x that
//
//   Comfort (see UpdateVignette):
//   VRVignette      darken the edges of the view while you move or turn with
//                   the sticks: 0 off (default), 1 light, 2 medium, 3 strong
//   VRVignetteWhen  1 moving, 2 turning, 3 both (default)
//
//   Reloading (see CClientWeapon::VR_UpdateReload):
//   VRReload        1 = by hand (default): A drops the magazine, left grip
//                   over your left shoulder takes a fresh one, push it into
//                   the gun; the right stick click is "next ammo". 0 = the
//                   game's own: right stick click reloads, A is next ammo.
//
//   Height and crouching (see UpdateRealCrouch):
//   VRSeated        0 = standing (default): crouch for real and Cate
//                   crouches; 1 = seated: the stick crouches only.
//   VRCrouchDepth   standing: how far (cm) your head drops below where you
//                   set your height before Cate crouches (default 25;
//                   0 = never, the stick only).
//
//   Throwing (see CClientWeapon::VR_UpdateThrow):
//   VRThrow         1 = by hand (default): hold the trigger, swing, let go.
//                   0 = the game's own (the trigger throws along the aim).
//   VRThrowSpeed    how fast a throw (m/s, at the hand) goes the game's own
//                   distance; faster goes further (default 5).
//
//   Melee (see CClientWeapon::VR_UpdateSwing):
//   VRSwing         1 = swinging a sword hits what the blade passes
//                   through (default), 0 = only the trigger slashes
//   VRSwingSpeed    how fast the blade's tip must move to count as a
//                   swing, metres per second (2.5)
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
#include "PlayerStats.h"
#include "ClientWeaponMgr.h"
#include "CMoveMgr.h"
#include "ClientWeaponBase.h"
#include "InterfaceResMgr.h"
#include "LayoutMgr.h"
#include "HUDItem.h"
#include "ltquaduvutils.h"
#include <stdarg.h>
#include <ctype.h>
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
static VarTrack g_vtVRWristBack;
static VarTrack g_vtVRHandPalm;
static VarTrack g_vtVRWristHud;
static VarTrack g_vtVRWristHudWatch;
static VarTrack g_vtVRWristHudX;
static VarTrack g_vtVRWristHudY;
static VarTrack g_vtVRWristHudZ;
static VarTrack g_vtVRWristHudSize;
static VarTrack g_vtVRGunHud;
static VarTrack g_vtVRGunHudX;
static VarTrack g_vtVRGunHudY;
static VarTrack g_vtVRGunHudZ;
static VarTrack g_vtVRGunHudSize;
static VarTrack g_vtVRScope;
static VarTrack g_vtVRScopeDist;
static VarTrack g_vtVRScopeHeight;
static VarTrack g_vtVRScopeZoom;
static VarTrack g_vtVRScopeSize;
static VarTrack g_vtVRScopeEye;
static VarTrack g_vtVRSwing;
static VarTrack g_vtVRReload;
static VarTrack g_vtVRThrow;
static VarTrack g_vtVRSeated;
static VarTrack g_vtVRCrouchDepth;
static VarTrack g_vtVRThrowSpeed;
static VarTrack g_vtVRVignette;
static VarTrack g_vtVRVignetteWhen;
static VarTrack g_vtVRSwingSpeed;

static const float kGrabDistanceMeters = 0.2f;	// how close the left hand must be to grab the gun
static const float kRingWidthMeters = 0.36f;	// the weapon ring's size
static const float kRingDeadZone = 0.2f;		// hand this close to the middle (of the half-width): no choice
static const float kArmLength = 0.12f;			// how much of the forearm's length is left when hiding arms

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
	m_pfnBeginLayer		= NULL;
	m_pfnEndLayer		= NULL;
	m_pfnSetLayer		= NULL;
	m_pfnBeginLayerFromScreen = NULL;
	m_pfnSetLayerPose	= NULL;
	m_pfnSetScopeView	= NULL;
	m_pfnTakeSnapshot	= NULL;
	m_pfnSetMirrorEye	= NULL;
	m_pfnShowMarker		= NULL;
	m_pfnSetVignette	= NULL;
	m_fVignette			= 0.0f;
	m_bSnapTriggerOff	= false;
	m_bMouseInUse		= false;
	m_bWantsMag			= false;
	m_bWantsCock		= false;
	m_vCockPoint.Init(0.0f, 0.0f, 0.0f);
	m_bMouseKnown		= false;
	m_nLastMouseX		= 0;
	m_nLastMouseY		= 0;
	m_nSnapMsgFrames	= 0;
	m_bScoped			= false;
	m_bRingOpen			= false;
	m_bMoveBodyBound	= false;
	m_bRealCrouch		= false;
	m_bStickCombo		= false;
	m_bComboHeld		= false;
	m_bComboDone		= false;
	m_fComboStart		= 0.0f;
	m_bLeftStickClick	= false;
	m_vRingOffset.Init(0.0f, 0.0f, 0.0f);
	m_vRingRight.Init(1.0f, 0.0f, 0.0f);
	m_vRingUp.Init(0.0f, 1.0f, 0.0f);
	m_nRingItems		= 0;
	m_fRingIconSize		= 0.15f;
	m_nRingSel			= -1;
	m_pRingText			= NULL;
	memset(m_aGunFits, 0, sizeof(m_aGunFits));
	m_nGunFits			= 0;
	m_nCurrentFit		= -1;
	m_szCurrentGun[0]	= '\0';
	m_bAdjusting		= false;
	m_bAdjustScope		= false;
	m_bAdjustMag		= false;
	m_bHoldingMag		= false;
	m_hGunModel			= LTNULL;
	m_hNodeArmL = m_hNodeWristL = m_hNodeArmR = m_hNodeWristR = INVALID_MODEL_NODE;
	m_nFingerJoints		= 0;
	m_bLeftFree			= false;
	m_vPalmNormal.Init(1.0f, 0.0f, 0.0f);
	for (int f = 0; f < kNumFingers; ++f)
		m_abFingerBase[f] = false;
	m_pArmLMatrix		= NULL;
	m_pArmRMatrix		= NULL;
	m_bGunPlaced		= false;
	m_bLeftAnimValid	= false;
	m_bTwoHand			= false;
	m_vTwoHandDir.Init(0.0f, 0.0f, 1.0f);
	m_bHandRest			= false;
	m_mFreeHand.Identity();
	m_vThumbCurl.Init(0.0f, 0.0f, 1.0f);
	m_nRefBones			= 0;
	m_bRefHandTried		= false;
	m_bWatchShown		= false;
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
	m_pfnBeginLayer		= (LTVR_BeginLayerFn)GetProcAddress(hEngine, "LTVR_BeginLayer");
	m_pfnEndLayer		= (LTVR_EndLayerFn)GetProcAddress(hEngine, "LTVR_EndLayer");
	m_pfnSetLayer		= (LTVR_SetLayerFn)GetProcAddress(hEngine, "LTVR_SetLayer");
	m_pfnBeginLayerFromScreen = (LTVR_BeginLayerFromScreenFn)GetProcAddress(hEngine, "LTVR_BeginLayerFromScreen");
	m_pfnSetLayerPose	= (LTVR_SetLayerPoseFn)GetProcAddress(hEngine, "LTVR_SetLayerPose");
	m_pfnSetScopeView	= (LTVR_SetScopeViewFn)GetProcAddress(hEngine, "LTVR_SetScopeView");
	m_pfnTakeSnapshot	= (LTVR_TakeSnapshotFn)GetProcAddress(hEngine, "LTVR_TakeSnapshot");
	m_pfnSetMirrorEye	= (LTVR_SetMirrorEyeFn)GetProcAddress(hEngine, "LTVR_SetMirrorEye");
	m_pfnShowMarker		= (LTVR_ShowMarkerFn)GetProcAddress(hEngine, "LTVR_ShowMarker");
	m_pfnSetVignette	= (LTVR_SetVignetteFn)GetProcAddress(hEngine, "LTVR_SetVignette");

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
	g_vtVRWristBack.Init(g_pLTClient, "VRWristBack", NULL, 7.0f);
	g_vtVRHandPalm.Init(g_pLTClient, "VRHandPalm", NULL, 3.5f);
	g_vtVRWristHud.Init(g_pLTClient, "VRWristHud", NULL, 1.0f);
	g_vtVRWristHudWatch.Init(g_pLTClient, "VRWristHudWatch", NULL, 1.0f);
	g_vtVRWristHudX.Init(g_pLTClient, "VRWristHudX", NULL, -3.0f);
	g_vtVRWristHudY.Init(g_pLTClient, "VRWristHudY", NULL, 0.0f);
	g_vtVRWristHudZ.Init(g_pLTClient, "VRWristHudZ", NULL, -9.0f);
	g_vtVRWristHudSize.Init(g_pLTClient, "VRWristHudSize", NULL, 9.0f);
	g_vtVRGunHud.Init(g_pLTClient, "VRGunHud", NULL, 1.0f);
	g_vtVRGunHudX.Init(g_pLTClient, "VRGunHudX", NULL, -6.0f);
	g_vtVRGunHudY.Init(g_pLTClient, "VRGunHudY", NULL, -2.0f);
	g_vtVRGunHudZ.Init(g_pLTClient, "VRGunHudZ", NULL, 6.0f);
	g_vtVRGunHudSize.Init(g_pLTClient, "VRGunHudSize", NULL, 10.0f);
	g_vtVRScope.Init(g_pLTClient, "VRScope", NULL, 1.0f);
	g_vtVRScopeDist.Init(g_pLTClient, "VRScopeDist", NULL, 8.0f);
	g_vtVRScopeHeight.Init(g_pLTClient, "VRScopeHeight", NULL, 8.0f);
	g_vtVRScopeSize.Init(g_pLTClient, "VRScopeSize", NULL, 5.5f);
	g_vtVRScopeZoom.Init(g_pLTClient, "VRScopeZoom", NULL, 4.0f);
	g_vtVRScopeEye.Init(g_pLTClient, "VRScopeEye", NULL, 5.0f);
	g_vtVRSwing.Init(g_pLTClient, "VRSwing", NULL, 1.0f);
	g_vtVRReload.Init(g_pLTClient, "VRReload", NULL, 1.0f);
	g_vtVRThrow.Init(g_pLTClient, "VRThrow", NULL, 1.0f);
	g_vtVRSeated.Init(g_pLTClient, "VRSeated", NULL, 0.0f);
	g_vtVRCrouchDepth.Init(g_pLTClient, "VRCrouchDepth", NULL, 25.0f);
	g_vtVRThrowSpeed.Init(g_pLTClient, "VRThrowSpeed", NULL, 5.0f);
	g_vtVRVignette.Init(g_pLTClient, "VRVignette", NULL, 0.0f);
	g_vtVRVignetteWhen.Init(g_pLTClient, "VRVignetteWhen", NULL, 3.0f);
	g_vtVRSwingSpeed.Init(g_pLTClient, "VRSwingSpeed", NULL, 2.5f);

	LoadGunFits();
	LoadSettings();
}

// ----------------------------------------------------------------------- //
//
//	Settings file (vr_settings.txt)
//
// ----------------------------------------------------------------------- //

static const char* const kSettingsFile = "vr_settings.txt";

struct VRSettingDef
{
	const char*	pszName;
	float		fDefault;
};

// Every VR setting the file keeps, with its default. (The engine's own ones,
// VRUnitsPerMeter and VRHud* / VRScreen*, are read by the renderer by name.)
static const VRSettingDef s_aVRSettings[] =
{
	{ "VRUnitsPerMeter",	54.5454f },	// "110%" world size (100% = 60)
	{ "VRSnapTurn",			0.0f },
	{ "VRTurnSpeed",		150.0f },
	{ "VRSnapAngle",		45.0f },
	{ "VRMoveWithHand",		0.0f },
	{ "VRHudDistance",		2.0f },
	{ "VRHudAngle",			60.0f },
	{ "VRHudFollow",		1.0f },
	{ "VRHudFollowAngle",	35.0f },
	{ "VRScreenDistance",	2.0f },
	{ "VRScreenAngle",		62.0f },
	{ "VRWristHud",			1.0f },
	{ "VRWristHudWatch",	1.0f },
	{ "VRWristHudX",		-3.0f },
	{ "VRWristHudY",		0.0f },
	{ "VRWristHudZ",		-9.0f },
	{ "VRWristHudSize",		9.0f },
	{ "VRGunHud",			1.0f },
	{ "VRGunHudX",			-6.0f },
	{ "VRGunHudY",			-2.0f },
	{ "VRGunHudZ",			6.0f },
	{ "VRGunHudSize",		10.0f },
	{ "VRScope",			1.0f },
	{ "VRScopeDist",		8.0f },
	{ "VRScopeHeight",		8.0f },
	{ "VRScopeSize",		5.5f },
	{ "VRScopeZoom",		4.0f },
	{ "VRScopeEye",			5.0f },
	{ "VRSwing",			1.0f },
	{ "VRReload",			1.0f },
	{ "VRThrow",			1.0f },
	{ "VRSeated",			0.0f },
	{ "VRCrouchDepth",		25.0f },
	{ "VRThrowSpeed",		5.0f },
	{ "VRVignette",			0.0f },
	{ "VRVignetteWhen",		3.0f },
	{ "VRSwingSpeed",		2.5f },
	{ "VRArms",				0.0f },
	{ "VRWristBack",		7.0f },
	{ "VRHandPalm",			3.5f },
	{ "VRGunScale",			12.0f },
	{ "VRGunHandX",			12.0f },
	{ "VRGunHandY",			-18.0f },
	{ "VRGunHandZ",			30.0f },
	{ "VRGunPitch",			0.0f },
};
static const int kNumVRSettings = sizeof(s_aVRSettings) / sizeof(s_aVRSettings[0]);

static const VRSettingDef* VR_FindSetting(const char* pszName)
{
	for (int i = 0; i < kNumVRSettings; ++i)
	{
		if (stricmp(s_aVRSettings[i].pszName, pszName) == 0)
			return &s_aVRSettings[i];
	}
	return NULL;
}

float CVRMgr::GetSettingDefault(const char* pszName)
{
	const VRSettingDef* pDef = VR_FindSetting(pszName);
	return pDef ? pDef->fDefault : 0.0f;
}

void CVRMgr::GetMagHold(LTVector& vSlideCm, float& fYaw, float& fPitch, float& fRoll) const
{
	const GunFit* pFit = GetCurrentFit();
	vSlideCm.Init(pFit ? pFit->fMagX : 0.0f, pFit ? pFit->fMagY : 0.0f, pFit ? pFit->fMagZ : 0.0f);
	fYaw = pFit ? pFit->fMagYaw : 0.0f;
	fPitch = pFit ? pFit->fMagPitch : 0.0f;
	fRoll = pFit ? pFit->fMagRoll : 0.0f;
}

bool CVRMgr::IsManualReload() const
{
	return IsActive() && g_vtVRReload.GetFloat(1.0f) != 0.0f;
}

bool CVRMgr::IsThrowByHand() const
{
	return IsActive() && g_vtVRThrow.GetFloat(1.0f) != 0.0f;
}

float CVRMgr::GetThrowSpeed() const
{
	float f = g_vtVRThrowSpeed.GetFloat(5.0f);
	return (f > 0.5f) ? f : 0.5f;
}

bool CVRMgr::GetHandTracking(int nHand, LTVector& vPos) const
{
	if (!IsActive() || nHand < 0 || nHand > 1)
		return false;
	const LTVR_HandState& hand = m_State.hand[nHand];
	if (!hand.active || !hand.grip.valid)
		return false;
	vPos.Init(hand.grip.pos[0], hand.grip.pos[1], hand.grip.pos[2]);
	return true;
}

bool CVRMgr::GameButtonPressed(int nHand, unsigned nButton) const
{
	if (!IsActive() || m_bAdjusting || IsDecisionOpen() || !g_pInterfaceMgr ||
		g_pInterfaceMgr->GetGameState() != GS_PLAYING)
		return false;
	return Pressed(nHand, nButton);
}

bool CVRMgr::ButtonHeld(int nHand, unsigned nButton) const
{
	if (!IsActive() || nHand < 0 || nHand > 1)
		return false;
	return (m_State.hand[nHand].buttons & nButton) != 0;
}

// Over your left shoulder: the left hand left of the head's middle, no
// further forward than your face, from about chest height up, within half
// a metre. Measured in a frame that turns with your head's heading only,
// so looking up or down doesn't move the spot.
bool CVRMgr::IsLeftHandAtShoulder() const
{
	if (!IsActive())
		return false;
	const LTVR_HandState& left = m_State.hand[LTVR_LEFT];
	if (!left.active || !left.grip.valid || m_State.unitsPerMeter <= 0.0f)
		return false;
	LTVector vGrip(left.grip.pos[0], left.grip.pos[1], left.grip.pos[2]);
	LTRotation rYaw(LTVector(0.0f, 1.0f, 0.0f), GetHeadYaw());
	LTVector v = VR_Unrotate(rYaw, vGrip - GetHeadOffset()) / m_State.unitsPerMeter;	// metres: right, up, forward
	return v.x < 0.05f && v.x > -0.5f && v.y > -0.45f && v.y < 0.35f && v.z < 0.10f && v.z > -0.5f;
}

// The left hand is where the gun is gripped to cock it (12 cm, a little
// more than the weapon's own 10 cm, so a grab there is never also a grab of
// the gun).
bool CVRMgr::IsLeftHandNearCock() const
{
	if (!m_bWantsCock || m_State.unitsPerMeter <= 0.0f)
		return false;
	LTVector vLeft;
	LTRotation rLeft;
	if (!GetHandInCamera(LTVR_LEFT, vLeft, rLeft, false))
		return false;
	return (vLeft - m_vCockPoint).Mag() / m_State.unitsPerMeter < 0.12f;
}

void CVRMgr::ShowMarker(const LTVector& vWorld, float fWidthMeters)
{
	if (!m_pfnShowMarker || !IsActive())
		return;
	float aPos[3] = { vWorld.x, vWorld.y, vWorld.z };
	m_pfnShowMarker(aPos, fWidthMeters);
}

bool CVRMgr::IsSwingOn() const
{
	return IsActive() && g_vtVRSwing.GetFloat(1.0f) != 0.0f;
}

float CVRMgr::GetSwingSpeed() const
{
	float f = g_vtVRSwingSpeed.GetFloat(2.5f);
	return (f < 0.5f) ? 0.5f : f;
}

float CVRMgr::GetSetting(const char* pszName)
{
	HCONSOLEVAR hVar = g_pLTClient->GetConsoleVar((char*)pszName);
	if (hVar)
		return g_pLTClient->GetVarValueFloat(hVar);
	return GetSettingDefault(pszName);
}

void CVRMgr::SetSetting(const char* pszName, float fValue)
{
	char szCmd[128];
	LTSNPrintF(szCmd, sizeof(szCmd), "\"%s\" \"%g\"", pszName, fValue);
	g_pLTClient->RunConsoleString(szCmd);
}

void CVRMgr::LoadSettings()
{
	FILE* pFile = fopen(kSettingsFile, "r");
	if (!pFile)
		return;
	char szLine[256];
	while (fgets(szLine, sizeof(szLine), pFile))
	{
		char szName[64];
		float fValue;
		if (szLine[0] == '#' || sscanf(szLine, "%63s %f", szName, &fValue) != 2)
			continue;
		const VRSettingDef* pDef = VR_FindSetting(szName);
		if (pDef)
			SetSetting(pDef->pszName, fValue);
	}
	fclose(pFile);
}

void CVRMgr::SaveSettings()
{
	FILE* pFile = fopen(kSettingsFile, "w");
	if (!pFile)
		return;
	fprintf(pFile, "# NOLF2 VR settings, written by Options > VR. One \"Name value\" per line;\n");
	fprintf(pFile, "# see README.md for what each one does. Delete this file for the defaults.\n");
	for (int i = 0; i < kNumVRSettings; ++i)
		fprintf(pFile, "%s %g\n", s_aVRSettings[i].pszName, GetSetting(s_aVRSettings[i].pszName));
	fclose(pFile);
}

void CVRMgr::SetDefaultBindings()
{
	if (!m_pfnSetBinding || !m_pfnClearBindings)
		return;

	m_pfnClearBindings();

	m_pfnSetBinding(LTVR_LEFT,  LTVR_BTN_TRIGGER,		COMMAND_ID_ACTIVATE);
	m_pfnSetBinding(LTVR_LEFT,  LTVR_BTN_PRIMARY,		COMMAND_ID_MISSION);

	m_pfnSetBinding(LTVR_RIGHT, LTVR_BTN_TRIGGER,		COMMAND_ID_FIRING);
	m_pfnSetBinding(LTVR_RIGHT, LTVR_BTN_SECONDARY,		COMMAND_ID_HOLSTER);
	// (Reloading by hand: A drops the magazine, see CClientWeapon::
	// VR_UpdateReload, and the right stick click is next ammo; otherwise A
	// is next ammo and the click reloads. The click isn't bound: it acts
	// when let go, so holding both stick clicks to set your height doesn't
	// also reload; see UpdateStickClicks.)
	if (g_vtVRReload.GetFloat(1.0f) == 0.0f)
		m_pfnSetBinding(LTVR_RIGHT, LTVR_BTN_PRIMARY,	COMMAND_ID_NEXT_AMMO);
	m_pfnSetBinding(LTVR_RIGHT, LTVR_BTN_STICK_UP,		COMMAND_ID_JUMP);
	m_pfnSetBinding(LTVR_RIGHT, LTVR_BTN_STICK_DOWN,	COMMAND_ID_DUCKLOCK);
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
		m_bMoveBodyBound = false;
		m_bSnapTriggerOff = false;		// (the new set binds the trigger as it should be)
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
		// While the left grip is held the left trigger takes a snapshot
		// instead of "activate" (normal play only: adjusting leaves the
		// left trigger unbound anyway).
		bool bGrip = (m_State.hand[LTVR_LEFT].buttons & LTVR_BTN_GRIP) != 0;
		if (nWantSet == 1 && m_pfnSetBinding)
		{
			if (bGrip && !m_bSnapTriggerOff)
			{
				m_pfnSetBinding(LTVR_LEFT, LTVR_BTN_TRIGGER, -1);
				m_bSnapTriggerOff = true;
			}
			else if (!bGrip && m_bSnapTriggerOff)
			{
				m_pfnSetBinding(LTVR_LEFT, LTVR_BTN_TRIGGER, COMMAND_ID_ACTIVATE);
				m_bSnapTriggerOff = false;
			}
		}
		UpdateSnapshot();
		UpdateStickClicks(nWantSet == 1);
		UpdateGunAdjust(fFrameTime);
		UpdateTwoHand();
		UpdateMoveBody();
		UpdateWeaponRing(fFrameTime);
	}
	else
	{
		m_bTwoHand = false;
		if (m_bRingOpen)
			CloseWeaponRing(false);
		UpdateStickClicks(false);	// (setting your height works in menus too)
	}
	UpdateRealCrouch();

	UpdateVignette(fFrameTime, bPlaying);

	if (m_pfnSetCloseScale)
		m_pfnSetCloseScale(GetGunScale());
}

// The comfort vignette: how much you're moving (walk stick) and smooth
// turning (right stick sideways), whichever is more of the kinds chosen,
// eased in quickly and out more slowly. Moving your real body and snap
// turns never count.
void CVRMgr::UpdateVignette(float fFrameTime, bool bPlaying)
{
	// Fully dark this far out from straight ahead (degrees): light, medium, strong.
	static const float kEdgeDegrees[4] = { 0.0f, 60.0f, 45.0f, 32.0f };
	int nStrength = (int)(g_vtVRVignette.GetFloat(0.0f) + 0.5f);
	if (nStrength < 0) nStrength = 0;
	if (nStrength > 3) nStrength = 3;

	float fWant = 0.0f;
	if (bPlaying && nStrength > 0 && !m_bAdjusting && !m_bRingOpen)
	{
		int nWhen = (int)(g_vtVRVignetteWhen.GetFloat(3.0f) + 0.5f);
		if (nWhen & 1)
		{
			float fMove = GetMoveVector(0.0f).Mag();
			if (fMove > fWant)
				fWant = fMove;
		}
		if (nWhen & 2)
		{
			float fTurn = GetSmoothTurnAmount();
			if (fTurn > fWant)
				fWant = fTurn;
		}
		if (fWant > 1.0f)
			fWant = 1.0f;
	}

	// In over about 0.15 s, out over about 0.4 s.
	float fRate = (fWant > m_fVignette) ? 1.0f / 0.15f : 1.0f / 0.4f;
	float fStep = fRate * fFrameTime;
	if (fWant > m_fVignette)
		m_fVignette = (m_fVignette + fStep > fWant) ? fWant : m_fVignette + fStep;
	else
		m_fVignette = (m_fVignette - fStep < fWant) ? fWant : m_fVignette - fStep;

	if (m_pfnSetVignette && m_fVignette > 0.0f && nStrength > 0)
		m_pfnSetVignette(m_fVignette, kEdgeDegrees[nStrength]);
}

// How hard you're smooth turning (0..1, as GetTurnDelta reads the stick);
// 0 with snap turning.
float CVRMgr::GetSmoothTurnAmount() const
{
	if (!IsActive() || m_bAdjusting || g_vtVRSnapTurn.GetFloat() > 0.5f)
		return 0.0f;
	float fX = (float)fabs(m_State.hand[LTVR_RIGHT].stick[0]);
	if (fX < kStickDeadZone)
		return 0.0f;
	float fAmount = (fX - kStickDeadZone) / (1.0f - kStickDeadZone);
	return (fAmount > 1.0f) ? 1.0f : fAmount;
}

// Left grip held + left trigger: save what the headset shows (the engine
// writes run\Snapshots\vr_<date>_<time>.png as this frame is shown). The
// "saved" message waits a few frames so it isn't in the picture.
void CVRMgr::UpdateSnapshot()
{
	if (m_nSnapMsgFrames > 0 && --m_nSnapMsgFrames == 0)
		Message("Snapshot saved (run\\Snapshots).");

	if (!(m_State.hand[LTVR_LEFT].buttons & LTVR_BTN_GRIP) || !Pressed(LTVR_LEFT, LTVR_BTN_TRIGGER))
		return;
	if (!m_pfnTakeSnapshot)
	{
		Message("Snapshots need the new engine (run update.bat).");
		return;
	}
	m_pfnTakeSnapshot();
	m_nSnapMsgFrames = 4;
	Log("Snapshot taken (holding: %s%s)", m_szCurrentGun[0] ? m_szCurrentGun : "nothing", m_bScoped ? ", through the scope" : "");
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

// How much to grow the hands so they're the same size whatever a gun's
// size: the hands are part of each gun's model, so a gun made smaller (the
// Sterlings, or A / B in the adjust mode) would shrink them with it. They
// look right at VRGunScale.
float CVRMgr::GetHandScale() const
{
	float fBase = g_vtVRGunScale.GetFloat(12.0f);
	if (fBase < 0.05f)
		fBase = 12.0f;
	float fScale = GetGunScale();
	return (fScale > 0.05f) ? fBase / fScale : 1.0f;
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
// The Sterlings are modelled about half again too big next to the other
// guns (the hands are the same size in every gun model, measured from
// vr_models.log; the Sterling's breech-to-muzzle is as long as the AK-47's),
// so they start smaller. The adjust mode's A / B fine-tunes it.
static const struct { const char* pszName; float x, y, z, yaw, pitch, scale, scopeUp, scopeRight;
	float magX, magY, magZ, magYaw, magPitch, magRoll; } kDefaultGunFits[] =
{
	// Tre's, set by eye in the headset (vr_guns.txt, 2026-10-11): the aim
	// (sights on the crosshair) and the magazine holds (held upright like a
	// handle). The two Tommy guns are one model, so the plain one shares the
	// infinite one's aim. The sniper rifle's scope is part of its model, so
	// it has its own scope offset (the scope adjust mode).
	{ "Crossbow",         0.10f, 0.00f, 0.25f,	5.62f, 1.39f,	12.00f,	0.00f, 0.00f,		0.00f, 0.00f, 0.00f,	0.00f, 0.00f, 0.00f },
	{ "Tazer",            0.00f, 0.00f, 0.00f,	0.15f, 0.15f,	12.00f,	0.00f, 0.00f,		0.00f, 0.00f, 0.00f,	0.00f, 0.00f, 0.00f },
	{ "Holster",          2.29f, 0.00f, 6.43f,	0.73f, 0.65f,	12.00f,	0.00f, 0.00f,		0.00f, 0.00f, 0.00f,	0.00f, 0.00f, 0.00f },
	{ "Sterling",         -0.06f, 0.00f, 0.06f,	6.52f, 4.64f,	8.50f,	0.00f, 0.00f,		4.10f, 3.80f, 8.00f,	-78.00f, 77.00f, 0.00f },
	{ "SilencedSterling", 0.00f, 0.00f, 0.00f,	0.00f, 0.00f,	8.50f,	0.00f, 0.00f,		4.09f, 3.84f, 7.99f,	-78.00f, 77.40f, 0.00f },
	{ "AK-47",            0.00f, 0.00f, 0.00f,	4.81f, 1.28f,	12.00f,	0.00f, 0.00f,		-1.58f, 2.26f, 6.25f,	-19.00f, 79.00f, 0.00f },
	{ "Sniper Rifle",     0.00f, 0.00f, 0.00f,	1.50f, 0.60f,	12.00f,	6.40f, -4.10f,		0.28f, 2.34f, 5.35f,	-0.10f, -46.30f, 0.00f },
	{ "Beretta",          0.00f, 0.00f, 0.00f,	7.17f, 0.82f,	12.00f,	0.00f, 0.00f,		0.12f, -0.21f, 0.09f,	-2.00f, 10.30f, 0.00f },
	{ "Tommygun",         0.00f, 0.00f, 0.00f,	3.83f, -1.78f,	12.00f,	0.00f, 0.00f,		7.57f, -7.04f, -2.17f,	4.90f, 49.00f, 0.00f },
	{ "TommygunInfinite", 0.00f, 0.00f, 0.00f,	3.83f, -1.78f,	12.00f,	0.00f, 0.00f,		7.60f, -7.00f, -2.20f,	5.00f, 49.00f, 0.00f },
	{ "Utility Launcher", 0.00f, 0.00f, 0.00f,	3.10f, 5.07f,	12.00f,	0.00f, 0.00f,		0.00f, 0.00f, 0.00f,	0.00f, 0.00f, 0.00f },
	{ "Shotgun",          0.00f, 0.00f, 0.00f,	4.39f, -1.93f,	12.00f,	0.00f, 0.00f,		0.00f, 0.00f, 0.00f,	0.00f, 0.00f, 0.00f },
	{ "Purse",            0.00f, 0.00f, 0.00f,	5.40f, 2.19f,	12.00f,	0.00f, 0.00f,		0.00f, 0.00f, 0.00f,	0.00f, 0.00f, 0.00f },
};

// vr_guns.txt: one line per weapon:  "Name" x y z yaw pitch scale [scopeUp scopeRight
// [magX magY magZ magYaw magPitch magRoll]]
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
			if (sscanf(pEnd + 1, "%f %f %f %f %f %f %f %f %f %f %f %f %f %f", &fit.fX, &fit.fY, &fit.fZ, &fit.fYaw,
				&fit.fPitch, &fit.fScale, &fit.fScopeUp, &fit.fScopeRight, &fit.fMagX, &fit.fMagY, &fit.fMagZ,
				&fit.fMagYaw, &fit.fMagPitch, &fit.fMagRoll) >= 5)
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
			{
				bHave = true;
				// In the file but its magazine hold never set (all 0, e.g.
				// a file from before there were holds): the built-in hold.
				GunFit& have = m_aGunFits[i];
				if (have.fMagX == 0.0f && have.fMagY == 0.0f && have.fMagZ == 0.0f &&
					have.fMagYaw == 0.0f && have.fMagPitch == 0.0f && have.fMagRoll == 0.0f)
				{
					have.fMagX = kDefaultGunFits[d].magX;
					have.fMagY = kDefaultGunFits[d].magY;
					have.fMagZ = kDefaultGunFits[d].magZ;
					have.fMagYaw = kDefaultGunFits[d].magYaw;
					have.fMagPitch = kDefaultGunFits[d].magPitch;
					have.fMagRoll = kDefaultGunFits[d].magRoll;
				}
			}
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
		fit.fScopeUp = kDefaultGunFits[d].scopeUp;
		fit.fScopeRight = kDefaultGunFits[d].scopeRight;
		fit.fMagX = kDefaultGunFits[d].magX;
		fit.fMagY = kDefaultGunFits[d].magY;
		fit.fMagZ = kDefaultGunFits[d].magZ;
		fit.fMagYaw = kDefaultGunFits[d].magYaw;
		fit.fMagPitch = kDefaultGunFits[d].magPitch;
		fit.fMagRoll = kDefaultGunFits[d].magRoll;
	}
}

void CVRMgr::SaveGunFits()
{
	FILE* pFile = fopen(kGunFitFile, "w");
	if (!pFile)
		return;
	fprintf(pFile, "# NOLF2 VR: how each gun sits in your hand. Made by the in-game\n");
	fprintf(pFile, "# \"adjust gun\" mode (click the left stick while playing).\n");
	fprintf(pFile, "# \"weapon\"  slide right / up / forward (cm)  turn  tilt (degrees)  size  scope up / right (cm)\n");
	fprintf(pFile, "#     magazine in the left hand: slide right / up / forward (cm)  turn  tilt  roll (degrees)\n");
	for (int i = 0; i < m_nGunFits; ++i)
	{
		const GunFit& fit = m_aGunFits[i];
		fprintf(pFile, "\"%s\"  %.2f %.2f %.2f  %.2f %.2f  %.2f  %.2f %.2f  %.2f %.2f %.2f  %.1f %.1f %.1f\n", fit.szName,
			fit.fX, fit.fY, fit.fZ, fit.fYaw, fit.fPitch, fit.fScale, fit.fScopeUp, fit.fScopeRight,
			fit.fMagX, fit.fMagY, fit.fMagZ, fit.fMagYaw, fit.fMagPitch, fit.fMagRoll);
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
	// Click the left stick: start / finish adjusting (on letting go, see
	// UpdateStickClicks).
	if (m_bLeftStickClick && !IsDecisionOpen())
	{
		if (!m_bAdjusting)
		{
			if (!m_szCurrentGun[0] || !GetOrAddCurrentFit())
				return;
			m_bAdjusting = true;
			// Holding a magazine: adjust how it's held. Looking through the
			// scope: adjust where the scope is.
			m_bAdjustMag = m_bHoldingMag;
			m_bAdjustScope = !m_bAdjustMag && m_bScoped;
			if (m_bAdjustMag)
			{
				Message("ADJUST MAGAZINE: left stick slides it, right stick turns and tilts it.");
				Message("Right grip + right stick up/down raises it. A / B: roll. (You can let go of the grip.)");
				Message("Hold it like a handle. Click the left stick when done.");
			}
			else if (m_bAdjustScope)
			{
				Message("ADJUST SCOPE: left stick moves the scope picture up / down / left / right.");
				Message("Put it over the scope on the gun. Click the left stick when done.");
			}
			else
			{
				Message("ADJUST GUN: left stick slides it, right stick tilts and turns it.");
				Message("Right grip + right stick up/down raises it. A / B: bigger / smaller.");
				Message("Line the sights up with the crosshair. Click the left stick when done.");
			}
		}
		else
		{
			m_bAdjusting = false;
			SaveGunFits();
			char szMsg[160];
			if (m_bAdjustMag)
			{
				const GunFit* pDone = GetCurrentFit();
				_snprintf(szMsg, sizeof(szMsg), "Magazine hold saved (%s: slide %.1f %.1f %.1f cm, turn %.0f tilt %.0f roll %.0f).",
					m_szCurrentGun, pDone ? pDone->fMagX : 0.0f, pDone ? pDone->fMagY : 0.0f, pDone ? pDone->fMagZ : 0.0f,
					pDone ? pDone->fMagYaw : 0.0f, pDone ? pDone->fMagPitch : 0.0f, pDone ? pDone->fMagRoll : 0.0f);
				szMsg[sizeof(szMsg) - 1] = '\0';
				Log("%s", szMsg);
			}
			else if (m_bAdjustScope)
			{
				const GunFit* pDone = GetCurrentFit();
				_snprintf(szMsg, sizeof(szMsg), "Scope saved (%s: %.1f cm up, %.1f cm right).", m_szCurrentGun,
					pDone ? pDone->fScopeUp : 0.0f, pDone ? pDone->fScopeRight : 0.0f);
				szMsg[sizeof(szMsg) - 1] = '\0';
				Log("%s", szMsg);
			}
			else
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

	if (m_bAdjustMag)
	{
		const float kMagDegPerSec = 45.0f;
		pFit->fMagX += lx * kSlideCmPerSec * 0.5f * fFrameTime;
		pFit->fMagZ += ly * kSlideCmPerSec * 0.5f * fFrameTime;
		if (m_State.hand[LTVR_RIGHT].buttons & LTVR_BTN_GRIP)
		{
			pFit->fMagY += ry * kSlideCmPerSec * 0.5f * fFrameTime;
		}
		else
		{
			pFit->fMagYaw += rx * kMagDegPerSec * fFrameTime;
			pFit->fMagPitch += ry * kMagDegPerSec * fFrameTime;
		}
		if (m_State.hand[LTVR_RIGHT].buttons & LTVR_BTN_PRIMARY)
			pFit->fMagRoll += kMagDegPerSec * fFrameTime;
		if (m_State.hand[LTVR_RIGHT].buttons & LTVR_BTN_SECONDARY)
			pFit->fMagRoll -= kMagDegPerSec * fFrameTime;
		return;
	}

	if (m_bAdjustScope)
	{
		const float kScopeCmPerSec = 3.0f;
		pFit->fScopeRight += lx * kScopeCmPerSec * fFrameTime;
		pFit->fScopeUp += ly * kScopeCmPerSec * fFrameTime;
		return;
	}

	// The scope picture stays on the gun's scope as the gun moves: where the
	// scope's line met the eye before (VRScopeHeight + the scope offset
	// above the aim line, about 38 cm behind the hand), in the gun's own
	// terms, then back out with the new fit; the difference goes onto the
	// scope offset. (Tre: re-aiming the sniper rifle left its scope picture
	// off center, as the picture's line runs along the aim, not the model.)
	const GunFit kBefore = *pFit;

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

	if (CurrentGunHasScope())
		MoveScopeWithGun(kBefore, *pFit);
}

bool CVRMgr::CurrentGunHasScope() const
{
	if (!g_pPlayerStats || !g_pWeaponMgr)
		return false;
	uint8 nScopeId = g_pPlayerStats->GetScope();
	MOD const* pMod = (nScopeId != WMGR_INVALID_ID) ? g_pWeaponMgr->GetMod(nScopeId) : NULL;
	return pMod && pMod->nZoomLevel > 0;
}

// The gun's fit went from "before" to "fit": shift fit's scope offset so the
// scope's line still crosses the gun where it did, at an eye's distance
// behind the hand. All in cm, in the aim's frame (x right, y up, z forward):
// a point of the model sits at slide + turn * (point * size - natural).
void CVRMgr::MoveScopeWithGun(const GunFit& before, GunFit& fit) const
{
	const float kEyeBehindCm = 38.0f;	// (the eye at the scope: 30 - 45 cm behind the hand in the logs)
	const float fHeight = g_vtVRScopeHeight.GetFloat(8.0f);
	const LTVector vNatural(g_vtVRGunHandX.GetFloat(), g_vtVRGunHandY.GetFloat(), g_vtVRGunHandZ.GetFloat());
	const float fOldSize = (before.fScale > 0.05f) ? before.fScale : GetGunScale();
	const float fNewSize = (fit.fScale > 0.05f) ? fit.fScale : GetGunScale();

	const LTRotation rOld = LTRotation(LTVector(0.0f, 1.0f, 0.0f), DEG2RAD(before.fYaw)) *
		LTRotation(LTVector(1.0f, 0.0f, 0.0f), DEG2RAD(before.fPitch));
	const LTRotation rNew = LTRotation(LTVector(0.0f, 1.0f, 0.0f), DEG2RAD(fit.fYaw)) *
		LTRotation(LTVector(1.0f, 0.0f, 0.0f), DEG2RAD(fit.fPitch));
	const LTVector vOldSlide(before.fX, before.fY, before.fZ);
	const LTVector vNewSlide(fit.fX, fit.fY, fit.fZ);

	const LTVector vWas(before.fScopeRight, fHeight + before.fScopeUp, -kEyeBehindCm);
	const LTVector vOnModel = (VR_Rotate(~rOld, vWas - vOldSlide) + vNatural) / fOldSize;
	const LTVector vNow = vNewSlide + VR_Rotate(rNew, vOnModel * fNewSize - vNatural);

	fit.fScopeRight = before.fScopeRight + (vNow.x - vWas.x);
	fit.fScopeUp = before.fScopeUp + (vNow.y - vWas.y);
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
	m_bLeftFree = false;
	m_nFingerJoints = 0;
	m_bHandRest = false;
	m_bGunPlaced = false;
	if (!hGunModel)
		return;

	LoadReferenceHand();
	FindHandRest(hGunModel);
	FindFingers(hGunModel);

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
	UpdateScope();
}

// ----------------------------------------------------------------------- //
//
//	Scopes
//
// ----------------------------------------------------------------------- //

// How much each of the game's zoom levels magnifies, times VRScopeZoom
// (4 by default: 4x, 8x, 16x). The scope's view is then the angle the lens
// takes up in your view divided by that, so things through it look that
// many times bigger than they do around it. (The game's own zoom angles,
// 20 / 7 / 2 degrees, only came to about 1.4x on the lens.)
static const float kScopeMag[4] = { 0.0f, 1.0f, 2.0f, 4.0f };

void CVRMgr::UpdateScope()
{
	bool bWas = m_bScoped;
	m_bScoped = false;

	int nLevel = 0;
	int nScopeEye = -1;		// the eye at the scope (0 left, 1 right)
	LTVector vLens, vFrom, vCamPos;
	LTRotation rAim, rCamRot;
	if (IsActive() && g_vtVRScope.GetFloat(1.0f) != 0.0f && g_pPlayerMgr && g_pPlayerStats && g_pWeaponMgr &&
		g_pInterfaceMgr && g_pInterfaceMgr->GetGameState() == GS_PLAYING && (!m_bAdjusting || m_bAdjustScope) && !m_bRingOpen &&
		g_pPlayerMgr->GetClientWeaponMgr() && g_pPlayerMgr->GetClientWeaponMgr()->WeaponsEnabled() &&
		GetAimRay(vFrom, rAim) && GetCamera(vCamPos, rCamRot))
	{
		uint8 nScopeId = g_pPlayerStats->GetScope();
		MOD const* pMod = (nScopeId != WMGR_INVALID_ID) ? g_pWeaponMgr->GetMod(nScopeId) : NULL;

		// (Once per weapon: whether it has a scope, for the log.)
		static char s_szLogged[64] = "";
		if (strcmp(s_szLogged, m_szCurrentGun) != 0)
		{
			SAFE_STRCPY(s_szLogged, m_szCurrentGun);
			if (pMod)
				Log("Scope: %s has %s, zoom level %d", m_szCurrentGun, pMod->szName ? pMod->szName : "?", pMod->nZoomLevel);
			else
				Log("Scope: %s has no scope", m_szCurrentGun);
		}

		if (pMod && pMod->nZoomLevel > 0)
		{
			// An eye (whichever is nearer) on the scope's line, behind the
			// gun, looking the way it points. The scope's line runs along the
			// aim line (which the adjust mode lines the gun up with) but
			// VRScopeHeight cm above it: the scope sits on top of the gun.
			// (Measured on the crossbow: eye 7 - 10 cm off the aim line.)
			// Each gun's own scope offset (the scope adjust mode) on top:
			// the sniper rifle's scope is part of its model and sits
			// elsewhere, and nothing in the game data says where.
			const float fUnitsPerCm = m_State.unitsPerMeter / 100.0f;
			const LTVector vFwd = rAim.Forward();
			const GunFit* pFit = GetCurrentFit();
			const float fHeight = g_vtVRScopeHeight.GetFloat(8.0f) + (pFit ? pFit->fScopeUp : 0.0f);
			const float fSide = pFit ? pFit->fScopeRight : 0.0f;
			const LTVector vScopeFrom = vFrom + rAim.Up() * (fHeight * fUnitsPerCm) + rAim.Right() * (fSide * fUnitsPerCm);
			float fBestOff = 1e9f, fBestBehind = 0.0f, fBestUp = 0.0f, fBestRight = 0.0f;
			LTVector vBestOnLine;
			for (int e = 0; e < 2; ++e)
			{
				LTVector vEye = vCamPos + rCamRot.Right() * ((e ? 3.2f : -3.2f) * fUnitsPerCm);
				float t = (vEye - vScopeFrom).Dot(vFwd);
				LTVector vOnLine = vScopeFrom + vFwd * t;
				LTVector vOff = vEye - vOnLine;
				float fOff = vOff.Mag() / fUnitsPerCm;
				if (fOff < fBestOff)
				{
					nScopeEye = e;
					fBestOff = fOff;
					fBestBehind = -t / fUnitsPerCm;
					vBestOnLine = vOnLine;
					// (Where the eye is from the aim line, for the log.)
					fBestUp = fHeight + vOff.Dot(rAim.Up()) / fUnitsPerCm;
					fBestRight = fSide + vOff.Dot(rAim.Right()) / fUnitsPerCm;
				}
			}
			float fFacing = vFwd.Dot(rCamRot.Forward());
			float fNear = g_vtVRScopeEye.GetFloat(5.0f);
			bool bOn = fBestOff < (bWas ? fNear * 1.6f : fNear) && fBestBehind > 0.0f && fBestBehind < 80.0f &&
				fFacing > (bWas ? 0.75f : 0.85f);
			// Adjusting it: keep it up while you move it about.
			if (m_bAdjusting && m_bAdjustScope && fBestBehind > 0.0f && fFacing > 0.5f)
				bOn = true;
			if (bOn)
			{
				nLevel = pMod->nZoomLevel;
				if (nLevel > 3)
					nLevel = 3;
				// The lens: on the sight line, just in front of that eye.
				vLens = vBestOnLine + vFwd * (g_vtVRScopeDist.GetFloat(8.0f) * fUnitsPerCm);
			}

			// (Raised but not through it: say why, now and then.)
			static float s_fNextLog = 0.0f;
			float fNow = g_pLTClient->GetTime();
			if (!bOn && fFacing > 0.7f && fNow >= s_fNextLog)
			{
				s_fNextLog = fNow + 1.0f;
				Log("Scope check: eye %.1f cm off the scope's line (needs < %.1f; eye is %.1f cm up, %.1f cm right of the aim line), "
					"%.1f cm behind the hand, facing %.2f (needs > 0.85)",
					fBestOff, fNear, fBestUp, fBestRight, fBestBehind, fFacing);
			}
		}
	}

	if (!IsActive() || !g_pPlayerMgr)
		return;		// (zoom is the flat game's own then)
	g_pPlayerMgr->VR_SetZoom(nLevel);
	m_bScoped = (nLevel > 0);

	// The monitor and snapshots show the eye at the scope.
	if (m_bScoped && m_pfnSetMirrorEye)
		m_pfnSetMirrorEye(nScopeEye);

	if (!m_bScoped || !m_pfnSetScopeView || !m_pfnSetLayerPose)
	{
		if (m_pfnSetScopeView)
			m_pfnSetScopeView(0, NULL, NULL, 0.0f);
		return;
	}

	// The scope's view, from the lens toward where the shot lands (the
	// first solid thing along the aim line, as for the crosshair), so the
	// reticle is true at any range even though the scope sits above it.
	LTRotation rView = rAim;
	{
		float fRange = 10000.0f;
		IClientWeaponBase* pWeapon = g_pPlayerMgr->GetCurrentClientWeapon();
		if (pWeapon && pWeapon->GetWeapon() && pWeapon->GetWeapon()->nRange > 0)
			fRange = (float)pWeapon->GetWeapon()->nRange;
		HOBJECT ahFilter[] = { g_pLTClient->GetClientObject(),
			g_pPlayerMgr->GetMoveMgr() ? g_pPlayerMgr->GetMoveMgr()->GetObject() : LTNULL, LTNULL };
		ClientIntersectQuery query;
		ClientIntersectInfo info;
		query.m_From = vFrom;
		query.m_To = vFrom + rAim.Forward() * fRange;
		query.m_Flags = INTERSECT_OBJECTS | IGNORE_NONSOLID;
		query.m_FilterFn = ObjListFilterFn;
		query.m_pUserData = ahFilter;
		LTVector vTarget = query.m_To;
		if (g_pLTClient->IntersectSegment(&query, &info))
			vTarget = info.m_Point;
		LTVector vDir = vTarget - vLens;
		float fLen = vDir.Mag();
		if (fLen > 1.0f && (vDir / fLen).Dot(rAim.Forward()) > 0.9f)
			rView = LTRotation(vDir / fLen, rAim.Up());
	}
	float aPos[3] = { vLens.x, vLens.y, vLens.z };
	float aRot[4] = { rView.m_Quat[0], rView.m_Quat[1], rView.m_Quat[2], rView.m_Quat[3] };
	float fWidthCm = g_vtVRScopeSize.GetFloat(5.5f);
	if (fWidthCm < 1.0f)	fWidthCm = 1.0f;
	if (fWidthCm > 20.0f)	fWidthCm = 20.0f;
	float fDistCm = g_vtVRScopeDist.GetFloat(8.0f);
	if (fDistCm < 2.0f)		fDistCm = 2.0f;
	float fLensDeg = 2.0f * RAD2DEG((float)atan(fWidthCm * 0.5f / fDistCm));
	float fMag = g_vtVRScopeZoom.GetFloat(4.0f) * kScopeMag[nLevel];
	if (fMag < 1.0f)		fMag = 1.0f;
	m_pfnSetScopeView(1, aPos, aRot, fLensDeg / fMag);
	float aLensRot[4] = { rAim.m_Quat[0], rAim.m_Quat[1], rAim.m_Quat[2], rAim.m_Quat[3] };

	// The lens, at the eyepiece, facing back at you.
	m_pfnSetLayerPose(LTVR_LAYER_SCOPE, 1, aPos, aLensRot, fWidthCm / 100.0f, 1.0f);
}

// Whether a bone's matrix has collapsed (the game shrinks bones to nothing
// to hide them, e.g. the arms while holstered). Undoing such a matrix
// blows up, so collapsed bones are left alone.
static LTVector VR_MatDir(const LTMatrix& m, const LTVector& v);	// (below)

static bool VR_IsCollapsed(const LTMatrix& m)
{
	float fDet =
		m.m[0][0] * (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1]) -
		m.m[0][1] * (m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0]) +
		m.m[0][2] * (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0]);
	return fabs(fDet) < 1e-4f;
}


// Left grip that didn't take hold of the gun: the game's "move body"
// (pick up or drop a body). The grip is bound to that command while it's
// held, so the game sees an ordinary key press.
void CVRMgr::UpdateMoveBody()
{
	if (!m_pfnSetBinding)
		return;
	bool bGrip = (m_State.hand[LTVR_LEFT].buttons & LTVR_BTN_GRIP) != 0;
	if (Pressed(LTVR_LEFT, LTVR_BTN_GRIP) && !m_bTwoHand && !m_bAdjusting && !IsDecisionOpen() &&
		!(m_bWantsMag && IsLeftHandAtShoulder()) && !IsLeftHandNearCock())
	{
		m_pfnSetBinding(LTVR_LEFT, LTVR_BTN_GRIP, COMMAND_ID_MOVE_BODY);
		m_bMoveBodyBound = true;
	}
	else if (m_bMoveBodyBound && !bGrip)
	{
		m_pfnSetBinding(LTVR_LEFT, LTVR_BTN_GRIP, -1);
		m_bMoveBodyBound = false;
	}
}

// Left grip near Cate's left hand: hold the gun with both hands.
void CVRMgr::UpdateTwoHand()
{
	bool bGrip = (m_State.hand[LTVR_LEFT].buttons & LTVR_BTN_GRIP) != 0;
	if (m_bTwoHand && (!bGrip || m_bAdjusting))
		m_bTwoHand = false;
	if (m_bTwoHand || m_bAdjusting || !Pressed(LTVR_LEFT, LTVR_BTN_GRIP) || !m_bGunPlaced || !m_bLeftAnimValid)
		return;
	if (m_bWantsMag && IsLeftHandAtShoulder())
		return;		// (taking a magazine, see CClientWeapon::VR_UpdateReload)
	if (IsLeftHandNearCock())
		return;		// (cocking, see CClientWeapon::VR_UpdateCock)

	LTVector vLeft, vRight;
	LTRotation rLeft, rAim, rGun;
	if (!GetHandInCamera(LTVR_LEFT, vLeft, rLeft, false) || !ComputeGun(vRight, rAim, rGun))
		return;

	// Where Cate's left hand is on the weapon, in camera space (the bones
	// are in really-close space: camera space shrunk by the gun scale).
	const float fScale = GetGunScale();
	LTVector vAnim;
	m_mLeftWristAnim.GetTranslation(vAnim);
	LTVector vAnimCam = vAnim * fScale;
	float fUnitsPerMeter = m_State.unitsPerMeter > 1.0f ? m_State.unitsPerMeter : 54.5454f;
	if ((vLeft - vAnimCam).Mag() / fUnitsPerMeter > kGrabDistanceMeters)
		return;

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

	for (int i = 0; i < p->m_nFingerJoints; ++i)
	{
		if (p->m_aFingerJoints[i].hNode == hNode)
		{
			p->CurlFinger(p->m_aFingerJoints[i], *data.m_pNodeTransform, data.m_pParentTransform);
			return;
		}
	}

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
		p->m_bLeftFree = false;
		for (int f = 0; f < kNumFingers; ++f)
			p->m_abFingerBase[f] = false;

		// The left hand as the weapon's animation has it (for grabbing the
		// gun with both hands). Hidden by the game (shrunk to nothing) for
		// one-handed items.
		bool bHidden = VR_IsCollapsed(*data.m_pNodeTransform) || (*ppArm && VR_IsCollapsed(**ppArm));
		p->m_bLeftAnimValid = !bHidden;
		if (!bHidden)
			p->m_mLeftWristAnim = *data.m_pNodeTransform;

		// Free left hand (also for one-handed items): the hand's resting
		// shape, put on the left controller. The forearm and fingers follow
		// (see CurlFinger), so it's the same hand whatever the weapon.
		LTMatrix mT;
		LTVector vPalm;
		if (p->IsActive() && !p->m_bTwoHand && p->GetFreeHandTransform(mT, vPalm))
		{
			p->m_mFreeHand = mT;
			p->m_vPalmNormal = vPalm;
			// The thumb folds across the palm toward the little finger, a
			// little into the palm (across the knuckles = mT's S axis).
			LTVector vAcross = VR_MatDir(mT, p->m_vBindS);
			LTVector vThumb = vAcross + vPalm * 0.6f;
			float fThumbLen = vThumb.Mag();
			if (fThumbLen > 1e-4f)
				p->m_vThumbCurl = vThumb / fThumbLen;
			p->m_bLeftFree = true;
			*data.m_pNodeTransform = mT * p->m_mBindWristL;
			if (*ppArm)
				**ppArm = mT * p->m_mBindArmL;
		}
		else if (bHidden)
		{
			*ppArm = NULL;
			return;
		}
	}
	else if (hNode == p->m_hNodeWristR)
	{
		ppArm = &p->m_pArmRMatrix;
		if (VR_IsCollapsed(*data.m_pNodeTransform) || (*ppArm && VR_IsCollapsed(**ppArm)))
		{
			*ppArm = NULL;
			return;
		}
	}
	else
	{
		return;
	}

	// Same-size hands whatever the gun's size: grow the hand about its
	// wrist (in the wrist's own axes). The fingers are worked out from the
	// wrist after this, so they come with it.
	if (p->IsActive())
	{
		const float fHand = p->GetHandScale();
		if (fHand > 0.01f && (fHand < 0.999f || fHand > 1.001f))
		{
			LTMatrix mGrow;
			mGrow.Identity();
			mGrow.m[0][0] = mGrow.m[1][1] = mGrow.m[2][2] = fHand;
			*data.m_pNodeTransform = (*data.m_pNodeTransform) * mGrow;
		}
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

// ----------------------------------------------------------------------- //
//
//	Free left hand: fingers follow the left grip
//
//	While the left hand is on the left controller (not holding the gun), its
//	fingers are bent by hand instead of by the gun's animation (which has
//	them holding the gun): a slight relaxed curl with the grip let go, a
//	fist with it squeezed. Each finger bends toward the palm, about an axis
//	across the knuckles; each joint is set to an angle from the finger's
//	base bone, so the result is the same whatever the animation was doing.
//
//	In this rig a finger is <name>L1 (inside the palm, left alone), then L2
//	(knuckle), L3 and L4 (the two joints of the finger); the thumb is
//	thumbL1 (base), thumbL2, thumbL3.
//
// ----------------------------------------------------------------------- //

// How far each joint bends, degrees, open (grip let go) and closed (fist).
// Index = joint along the finger (2 = first joint that bends).
static const float kFingerOpen[5]	= { 0.0f, 0.0f, 8.0f, 12.0f, 8.0f };
static const float kFingerClosed[5]	= { 0.0f, 0.0f, 75.0f, 90.0f, 55.0f };
// (The thumb: index 1 is its base swinging over the palm.)
static const float kThumbOpen[5]	= { 0.0f, 0.0f, 5.0f, 5.0f, 0.0f };
static const float kThumbClosed[5]	= { 0.0f, 25.0f, 25.0f, 35.0f, 0.0f };

// Rotation by fAngle radians about a unit axis (written out, so it doesn't
// depend on which way round LithTech's own helpers turn): R * v turns v
// about vAxis, and when vAxis = a x b it turns a toward b.
static void VR_AxisAngleMatrix(const LTVector& k, float fAngle, float r[3][3])
{
	float c = (float)cos(fAngle), s = (float)sin(fAngle), t = 1.0f - c;
	r[0][0] = c + k.x * k.x * t;		r[0][1] = k.x * k.y * t - k.z * s;	r[0][2] = k.x * k.z * t + k.y * s;
	r[1][0] = k.y * k.x * t + k.z * s;	r[1][1] = c + k.y * k.y * t;		r[1][2] = k.y * k.z * t - k.x * s;
	r[2][0] = k.z * k.x * t - k.y * s;	r[2][1] = k.z * k.y * t + k.x * s;	r[2][2] = c + k.z * k.z * t;
}

static LTVector VR_Mul3(const float r[3][3], const LTVector& v)
{
	return LTVector(r[0][0] * v.x + r[0][1] * v.y + r[0][2] * v.z,
					r[1][0] * v.x + r[1][1] * v.y + r[1][2] * v.z,
					r[2][0] * v.x + r[2][1] * v.y + r[2][2] * v.z);
}

static LTVector VR_Cross(const LTVector& a, const LTVector& b)
{
	// (Written out: LTVector::Cross is reversed.)
	return LTVector(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

// A matrix's rotation part applied to a direction.
static LTVector VR_MatDir(const LTMatrix& m, const LTVector& v)
{
	return LTVector(m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z,
					m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z,
					m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z);
}

// The model's resting left hand: the wrist and forearm resting poses, and
// the hand's frame from where its joints sit.
void CVRMgr::FindHandRest(HOBJECT hModel)
{
	m_bHandRest = false;
	HMODELNODE hArm, hWrist, hMid, hPointer, hPinky;
	if (LT_OK != g_pModelLT->GetNode(hModel, "armL", hArm) ||
		LT_OK != g_pModelLT->GetNode(hModel, "wristL", hWrist) ||
		LT_OK != g_pModelLT->GetNode(hModel, "middleL2", hMid) ||
		LT_OK != g_pModelLT->GetNode(hModel, "pointerL2", hPointer) ||
		LT_OK != g_pModelLT->GetNode(hModel, "pinkyL2", hPinky))
		return;

	LTMatrix mMid, mPointer, mPinky;
	if (!GetRestPose(hModel, hArm, m_mBindArmL) ||
		!GetRestPose(hModel, hWrist, m_mBindWristL) ||
		!GetRestPose(hModel, hMid, mMid) ||
		!GetRestPose(hModel, hPointer, mPointer) ||
		!GetRestPose(hModel, hPinky, mPinky))
		return;

	LTVector vMid, vPointer, vPinky;
	m_hNodeWristL = hWrist;	// (needed by FindFingers; AttachHands sets it again)
	m_mBindWristL.GetTranslation(m_vBindWrist);
	mMid.GetTranslation(vMid);
	mPointer.GetTranslation(vPointer);
	mPinky.GetTranslation(vPinky);

	LTVector vF = vMid - m_vBindWrist;
	float fLen = vF.Mag();
	if (fLen < 1e-5f)
		return;
	vF /= fLen;
	LTVector vS = vPinky - vPointer;
	vS -= vF * vS.Dot(vF);
	fLen = vS.Mag();
	if (fLen < 1e-5f)
		return;
	vS /= fLen;
	m_vBindF = vF;
	m_vBindS = vS;
	m_vBindN = VR_Cross(vF, vS);
	m_bHandRest = true;
}

// Moves the resting left hand onto the left controller, in really-close
// space: the space the gun model's bones are worked out in (the engine
// starts the skeleton from the object's own position and rotation, so bone
// matrices are in the same space as the object; the resting poses are
// relative to the object). The controller's grip pose (OpenXR): the handle runs
// through the fist toward the index finger (-Z there, +Z here), +X comes
// out of the left palm, and +Y points back toward the wrist. So, in the
// hand's frame: knuckles (F) = -Up, pointer -> pinky (S) = -Forward,
// palm (N) = Right. The wrist sits behind the handle and toward the back
// of the hand (VRWristBack, VRHandPalm).
bool CVRMgr::GetFreeHandTransform(LTMatrix& mT, LTVector& vPalm) const
{
	if (!m_bHandRest || !m_bGunPlaced || !IsActive())
		return false;
	const LTVR_HandState& hand = m_State.hand[LTVR_LEFT];
	LTVector vGrip;
	LTRotation rAim;
	if (!hand.grip.valid || !GetHandInCamera(LTVR_LEFT, vGrip, rAim, false))
		return false;

	LTRotation rHead = GetHeadRotation();
	LTRotation rGrip(hand.grip.rot[0], hand.grip.rot[1], hand.grip.rot[2], hand.grip.rot[3]);
	rGrip = (~rHead) * rGrip;

	LTVector vF = -rGrip.Up();
	LTVector vS = -rGrip.Forward();
	LTVector vN = rGrip.Right();

	const float fUnitsPerCm = (m_State.unitsPerMeter > 1.0f ? m_State.unitsPerMeter : 54.5454f) / 100.0f;
	LTVector vWrist = vGrip - vF * (g_vtVRWristBack.GetFloat(7.0f) * fUnitsPerCm)
							- vN * (g_vtVRHandPalm.GetFloat(3.5f) * fUnitsPerCm);
	vWrist /= GetGunScale();	// camera space -> really-close space

	// Rotation taking the resting hand's frame onto the controller's.
	mT.Identity();
	const LTVector* aW[3] = { &vF, &vS, &vN };
	const LTVector* aB[3] = { &m_vBindF, &m_vBindS, &m_vBindN };
	for (int r = 0; r < 3; ++r)
	{
		for (int c = 0; c < 3; ++c)
		{
			float f = 0.0f;
			for (int k = 0; k < 3; ++k)
			{
				const float w[3] = { aW[k]->x, aW[k]->y, aW[k]->z };
				const float b[3] = { aB[k]->x, aB[k]->y, aB[k]->z };
				f += w[r] * b[c];
			}
			mT.m[r][c] = f;
		}
	}
	// ... and the resting wrist onto the wrist's place.
	LTVector vMoved = VR_MatDir(mT, m_vBindWrist);
	mT.SetTranslation(vWrist - vMoved);

	vPalm = vN;
	return true;
}

void CVRMgr::LoadReferenceHand()
{
	if (m_bRefHandTried)
		return;
	m_bRefHandTried = true;

	WEAPON const* pKatana = g_pWeaponMgr ? g_pWeaponMgr->GetWeapon("Katana") : NULL;
	if (!pKatana || !pKatana->szPVModel || !pKatana->szPVModel[0])
	{
		Log("Reference hand: no Katana in the weapon list; each weapon uses its own hand");
		return;
	}

	// Load its first-person model as an invisible object just long enough
	// to read its skeleton's resting pose.
	ObjectCreateStruct ocs;
	ocs.Clear();
	ocs.m_ObjectType = OT_MODEL;
	ocs.m_Flags = 0;
	SAFE_STRCPY(ocs.m_Filename, pKatana->szPVModel);
	HOBJECT hObj = g_pLTClient->CreateObject(&ocs);
	if (!hObj)
	{
		Log("Reference hand: could not load %s", pKatana->szPVModel);
		return;
	}

	uint32 nNodes = 0;
	g_pModelLT->GetNumNodes(hObj, nNodes);
	for (uint32 i = 0; i < nNodes && m_nRefBones < kMaxRefBones; ++i)
	{
		RefBone& bone = m_aRefBones[m_nRefBones];
		if (LT_OK == g_pModelLT->GetNodeName(hObj, (HMODELNODE)i, bone.szName, sizeof(bone.szName)) &&
			LT_OK == g_pModelLT->GetBindPoseNodeTransform(hObj, (HMODELNODE)i, bone.mBind))
			++m_nRefBones;
	}
	g_pLTClient->RemoveObject(hObj);
	Log("Reference hand: %d bones from %s", m_nRefBones, pKatana->szPVModel);
}

// A bone's resting pose: the reference hand's, if it has a bone of that
// name, else this model's own.
bool CVRMgr::GetRestPose(HOBJECT hModel, HMODELNODE hNode, LTMatrix& mBind) const
{
	char szName[32];
	if (m_nRefBones && LT_OK == g_pModelLT->GetNodeName(hModel, hNode, szName, sizeof(szName)))
	{
		for (int i = 0; i < m_nRefBones; ++i)
		{
			if (stricmp(m_aRefBones[i].szName, szName) == 0)
			{
				mBind = m_aRefBones[i].mBind;
				return true;
			}
		}
	}
	return LT_OK == g_pModelLT->GetBindPoseNodeTransform(hModel, hNode, mBind);
}

void CVRMgr::FindFingers(HOBJECT hModel)
{
	m_nFingerJoints = 0;
	if (!m_bHandRest)
		return;		// (needs the wrist's resting pose)
	static const char* const aszFinger[kNumFingers] = { "thumb", "pointer", "middle", "ring", "pinky" };

	for (int f = 0; f < kNumFingers; ++f)
	{
		int nSegs = (f == 0) ? 3 : 4;
		HMODELNODE ahNode[5];
		LTVector avBind[5];
		LTMatrix amBind[5];
		bool bAll = true;
		for (int n = 1; n <= nSegs && bAll; ++n)
		{
			char szName[32];
			sprintf(szName, "%sL%d", aszFinger[f], n);
			ahNode[n] = INVALID_MODEL_NODE;
			if (LT_OK != g_pModelLT->GetNode(hModel, szName, ahNode[n]))
			{
				szName[0] = (char)toupper(szName[0]);	// "PointerL4" in some models
				if (LT_OK != g_pModelLT->GetNode(hModel, szName, ahNode[n]))
					bAll = false;
			}
			if (bAll && !GetRestPose(hModel, ahNode[n], amBind[n]))
				bAll = false;
			if (bAll)
				amBind[n].GetTranslation(avBind[n]);
		}
		if (!bAll)
			continue;	// not this finger in this model

		// (The joint before the first is the wrist.)
		amBind[0] = m_mBindWristL;
		avBind[0] = m_vBindWrist;
		for (int n = 1; n <= nSegs && m_nFingerJoints < kMaxFingerJoints; ++n)
		{
			// The way this joint's bone points: toward the next joint (the
			// last one carries on the way the one before it pointed).
			LTVector vToward = (n < nSegs) ? avBind[n + 1] : avBind[n] + (avBind[n] - avBind[n - 1]);
			LTVector vLocal = amBind[n].MakeInverse() * vToward;
			float fLen = vLocal.Mag();
			if (fLen < 1e-5f)
				vLocal.Init(1.0f, 0.0f, 0.0f);
			else
				vLocal /= fLen;

			FingerJoint& j = m_aFingerJoints[m_nFingerJoints++];
			j.hNode = ahNode[n];
			j.nFinger = f;
			j.nSeg = n;
			j.vDirLocal = vLocal;
			j.mFromParent = amBind[n - 1].MakeInverse() * amBind[n];

			g_pModelLT->RemoveNodeControlFn(hModel, ahNode[n], HandsNodeControl, this);
			g_pModelLT->AddNodeControlFn(hModel, ahNode[n], HandsNodeControl, this);
		}
	}

	// Any other bones hanging off the wrist (helpers that shape the skin
	// around it): kept at rest too, so the wrist doesn't pinch.
	uint32 nChildren = 0;
	if (LT_OK == g_pModelLT->GetNumChildren(hModel, m_hNodeWristL, nChildren))
	{
		for (uint32 c = 0; c < nChildren && m_nFingerJoints < kMaxFingerJoints; ++c)
		{
			HMODELNODE hChild = INVALID_MODEL_NODE;
			LTMatrix mBind;
			if (LT_OK != g_pModelLT->GetChild(hModel, m_hNodeWristL, c, hChild) ||
				!GetRestPose(hModel, hChild, mBind))
				continue;
			bool bKnown = false;
			for (int i = 0; i < m_nFingerJoints; ++i)
				bKnown = bKnown || (m_aFingerJoints[i].hNode == hChild);
			if (bKnown)
				continue;
			FingerJoint& j = m_aFingerJoints[m_nFingerJoints++];
			j.hNode = hChild;
			j.nFinger = -1;
			j.nSeg = 0;
			j.vDirLocal.Init(1.0f, 0.0f, 0.0f);
			j.mFromParent = m_mBindWristL.MakeInverse() * mBind;
			g_pModelLT->RemoveNodeControlFn(hModel, hChild, HandsNodeControl, this);
			g_pModelLT->AddNodeControlFn(hModel, hChild, HandsNodeControl, this);
		}
	}
}

void CVRMgr::CurlFinger(const FingerJoint& joint, LTMatrix& mNode, const LTMatrix* pParent)
{
	if (!m_bLeftFree || !pParent)
		return;

	// Hang the bone off its parent as at rest (the parent has been placed
	// already: the wrist on the controller, or the joint before this one,
	// already bent), so the chain stays joined up.
	mNode = (*pParent) * joint.mFromParent;
	if (joint.nSeg == 0)
		return;

	// Where this bone points now.
	LTVector vDir = VR_MatDir(mNode, joint.vDirLocal);
	float fLen = vDir.Mag();
	if (fLen < 1e-6f)
		return;
	vDir /= fLen;

	const int f = joint.nFinger;
	if (joint.nSeg == 1)
	{
		// The base bone: remember its resting direction and the hinge the
		// finger bends about (base x palm; the thumb folds across the palm
		// instead).
		LTVector vAxis = VR_Cross(vDir, (f == 0) ? m_vThumbCurl : m_vPalmNormal);
		float fAxisLen = vAxis.Mag();
		m_abFingerBase[f] = (fAxisLen > 0.05f);
		if (m_abFingerBase[f])
		{
			m_avFingerBase[f] = vDir;
			m_avFingerAxis[f] = vAxis / fAxisLen;
		}
		if (f != 0 || !m_abFingerBase[f])
			return;
		// (The thumb's base turns too, swinging it over the palm.)
	}
	if (!m_abFingerBase[f] || joint.nSeg > 4)
		return;

	// This joint's angle from the base bone: the angles of the joints up to
	// here, from open to closed with the grip.
	float fGrip = m_State.hand[LTVR_LEFT].squeeze;
	if (fGrip < 0.0f) fGrip = 0.0f;
	if (fGrip > 1.0f) fGrip = 1.0f;
	const float* pOpen = (f == 0) ? kThumbOpen : kFingerOpen;
	const float* pClosed = (f == 0) ? kThumbClosed : kFingerClosed;
	float fWant = 0.0f;
	for (int n = 1; n <= joint.nSeg; ++n)
		fWant += pOpen[n] + (pClosed[n] - pOpen[n]) * fGrip;
	fWant = DEG2RAD(fWant);

	// Its angle now, about the hinge (it already carries the bend of the
	// joints before it), then turn it about the hinge only, so the finger
	// bends like a hinge and never twists.
	const LTVector& k = m_avFingerAxis[f];
	const LTVector& b = m_avFingerBase[f];
	LTVector vFlat = vDir - k * vDir.Dot(k);
	if (vFlat.Mag() < 1e-4f)
		return;
	float fNow = (float)atan2(k.Dot(VR_Cross(b, vFlat)), b.Dot(vFlat));

	float r[3][3];
	VR_AxisAngleMatrix(k, fWant - fNow, r);
	LTMatrix mOld = mNode;
	for (int i = 0; i < 3; ++i)
	{
		for (int c = 0; c < 3; ++c)
			mNode.m[i][c] = r[i][0] * mOld.m[0][c] + r[i][1] * mOld.m[1][c] + r[i][2] * mOld.m[2][c];
	}
	// (Position, column 3, unchanged: the bone turns about its own joint.)
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
	if (m_pfnBeginLayer)
		return m_pfnBeginLayer(LTVR_LAYER_CROSSHAIR);
	return m_pfnBeginCrosshair ? m_pfnBeginCrosshair() : 0;
}

void CVRMgr::EndCrosshair()
{
	if (m_pfnEndLayer)
		m_pfnEndLayer();
	else if (m_pfnEndCrosshair)
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

bool CVRMgr::NoteMouseMove(int x, int y)
{
	if (!IsActive())
		return true;
	bool bMoved = m_bMouseKnown && (abs(x - m_nLastMouseX) + abs(y - m_nLastMouseY) >= 3);
	m_nLastMouseX = x;
	m_nLastMouseY = y;
	m_bMouseKnown = true;
	if (bMoved)
		m_bMouseInUse = true;
	return bMoved;
}

void CVRMgr::SendKey(int nKey)
{
	// The controllers are driving the menu: the mouse steps aside.
	m_bMouseInUse = false;
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

// ----------------------------------------------------------------------- //
// Height, seated / standing, crouching for real
// ----------------------------------------------------------------------- //

// Where your head is now becomes Cate's standing eyes, facing forward (the
// engine re-centres the headset there). Seated or standing alike: sit and
// your seated head is Cate standing up.
void CVRMgr::SetHeightNow()
{
	Recenter();
	m_bRealCrouch = false;
	const bool bSeated = IsSeated();
	Log("Height set (%s): head was %.2f m above the old centre", bSeated ? "seated" : "standing",
		m_State.unitsPerMeter > 0.0f ? m_State.head.pos[1] / m_State.unitsPerMeter : 0.0f);
	Message(bSeated ? "Height set (seated): this is Cate standing. The right stick crouches." :
		"Height set (standing): this is Cate standing. Crouch for real to crouch.");
}

bool CVRMgr::IsSeated() const
{
	return g_vtVRSeated.GetFloat(0.0f) != 0.0f;
}

// Standing: your head well below where you set your height crouches Cate
// (collision, sneaking, fitting under things); the view just follows your
// head (see CPlayerMgr, where the game's own crouch lowering is left out
// then). Coming up 7 cm stands her again (if there's room).
void CVRMgr::UpdateRealCrouch()
{
	bool bWas = m_bRealCrouch;
	float fDepthCm = g_vtVRCrouchDepth.GetFloat(25.0f);
	if (!IsActive() || IsSeated() || fDepthCm <= 0.0f || m_State.unitsPerMeter <= 0.0f || !m_State.head.valid)
	{
		m_bRealCrouch = false;
	}
	else
	{
		float fDropCm = -m_State.head.pos[1] / m_State.unitsPerMeter * 100.0f;
		if (!m_bRealCrouch && fDropCm > fDepthCm)
			m_bRealCrouch = true;
		else if (m_bRealCrouch && fDropCm < fDepthCm - 7.0f)
			m_bRealCrouch = false;
	}

	if (m_bRealCrouch != bWas)
	{
		// Standing up for real also stands up from a stick crouch.
		if (m_bRealCrouch && g_pPlayerMgr && g_pPlayerMgr->GetMoveMgr())
			g_pPlayerMgr->GetMoveMgr()->SetDuckLock(LTFALSE);
		Log("Crouch: %s (head %.0f cm below your standing height)", m_bRealCrouch ? "down" : "up",
			m_State.unitsPerMeter > 0.0f ? -m_State.head.pos[1] / m_State.unitsPerMeter * 100.0f : 0.0f);
	}
}

// Stick clicks act when let go, unless both were held together: held for a
// second, that sets your height. (Left click: the adjust mode, see
// UpdateGunAdjust. Right click: next ammo, or reload with the game's own
// reloading, sent to the game as a key press.)
void CVRMgr::UpdateStickClicks(bool bPlay)
{
	m_bLeftStickClick = false;
	const bool bL = (m_State.hand[LTVR_LEFT].buttons & LTVR_BTN_STICK) != 0;
	const bool bR = (m_State.hand[LTVR_RIGHT].buttons & LTVR_BTN_STICK) != 0;
	const bool bLWas = (m_aPrevButtons[LTVR_LEFT] & LTVR_BTN_STICK) != 0;
	const bool bRWas = (m_aPrevButtons[LTVR_RIGHT] & LTVR_BTN_STICK) != 0;
	const float fNow = g_pLTClient->GetTime();

	if (bL && bR)
	{
		if (!m_bComboHeld)
		{
			m_bComboHeld = true;
			m_bComboDone = false;
			m_fComboStart = fNow;
		}
		m_bStickCombo = true;
		if (!m_bComboDone && fNow - m_fComboStart >= 1.0f)
		{
			m_bComboDone = true;
			SetHeightNow();
		}
	}
	else
	{
		m_bComboHeld = false;
	}

	if (!m_bStickCombo)
	{
		m_bLeftStickClick = bPlay && bLWas && !bL;
		if (bPlay && bRWas && !bR && g_pPlayerMgr && g_pInterfaceMgr && g_pPlayerMgr->IsPlayerInWorld())
		{
			// A key press and release, routed as CGameClientShell routes
			// commands: the interface first, then the player.
			int nCommand = (g_vtVRReload.GetFloat(1.0f) != 0.0f) ? COMMAND_ID_NEXT_AMMO : COMMAND_ID_RELOAD;
			if (g_pPlayerMgr->IsPlayerDead() || !g_pInterfaceMgr->OnCommandOn(nCommand))
				g_pPlayerMgr->OnCommandOn(nCommand);
			if (!g_pInterfaceMgr->OnCommandOff(nCommand))
				g_pPlayerMgr->OnCommandOff(nCommand);
		}
	}
	if (!bL && !bR)
		m_bStickCombo = false;
}

// ----------------------------------------------------------------------- //
// Weapon ring
// ----------------------------------------------------------------------- //

bool CVRMgr::GetRingCentre(LTVector& vCentre) const
{
	HOBJECT hCamera = g_pPlayerMgr ? g_pPlayerMgr->GetCamera() : LTNULL;
	if (!hCamera)
		return false;
	LTVector vCamPos;
	g_pLTClient->GetObjectPos(hCamera, &vCamPos);
	vCentre = vCamPos + m_vRingOffset;
	return true;
}

bool CVRMgr::OpenWeaponRing()
{
	CClientWeaponMgr* pWeaponMgr = g_pPlayerMgr ? g_pPlayerMgr->GetClientWeaponMgr() : NULL;
	if (!pWeaponMgr || !g_pWeaponMgr || !g_pPlayerStats)
		return false;

	// Every weapon and gadget you can switch to, in the game's own order.
	m_nRingItems = 0;
	int nFirst = g_pWeaponMgr->GetFirstWeaponCommandId();
	int nLast = g_pWeaponMgr->GetLastWeaponCommandId();
	for (int nCommand = nFirst; nCommand <= nLast && m_nRingItems < kMaxRingItems; ++nCommand)
	{
		uint8 nWeapon = g_pWeaponMgr->GetWeaponId(nCommand);
		if (!g_pWeaponMgr->IsPlayerWeapon(nWeapon) || !g_pPlayerStats->HaveWeapon(nWeapon) ||
			!pWeaponMgr->CanChangeToWeapon(nWeapon))
			continue;
		m_aRingWeapon[m_nRingItems++] = nWeapon;
	}
	if (!m_nRingItems)
		return false;

	// Where: centred on the hand, facing the head, upright.
	LTVector vHand;
	LTRotation rAim;
	HOBJECT hCamera = g_pPlayerMgr->GetCamera();
	if (!hCamera || !GetAimRay(vHand, rAim))
		return false;
	LTVector vCamPos;
	g_pLTClient->GetObjectPos(hCamera, &vCamPos);
	m_vRingOffset = vHand - vCamPos;

	LTVector vAway = m_vRingOffset;
	float fLen = vAway.Mag();
	if (fLen < 0.001f)
		return false;
	vAway /= fLen;
	// (Cross products written out: LTVector::Cross is reversed.)
	LTVector vUpWorld(0.0f, 1.0f, 0.0f);
	LTVector vRight(vUpWorld.y * vAway.z - vUpWorld.z * vAway.y,
					vUpWorld.z * vAway.x - vUpWorld.x * vAway.z,
					vUpWorld.x * vAway.y - vUpWorld.y * vAway.x);
	float fRightLen = vRight.Mag();
	if (fRightLen < 0.001f)
		vRight.Init(1.0f, 0.0f, 0.0f);	// looking straight up or down
	else
		vRight /= fRightLen;
	m_vRingRight = vRight;
	m_vRingUp.Init(vAway.y * vRight.z - vAway.z * vRight.y,
				   vAway.z * vRight.x - vAway.x * vRight.z,
				   vAway.x * vRight.y - vAway.y * vRight.x);

	// Layout: one ring for up to 10, an inner and an outer ring for more.
	// Positions run clockwise from the top, -1..1 across the ring.
	const float kTwoPi = 6.2831853f;
	if (m_nRingItems <= 10)
	{
		m_fRingIconSize = 0.075f;
		for (int i = 0; i < m_nRingItems; ++i)
		{
			float fAngle = kTwoPi * (float)i / (float)m_nRingItems;
			m_aRingX[i] = 0.66f * (float)sin(fAngle);
			m_aRingY[i] = 0.66f * (float)cos(fAngle);
		}
	}
	else
	{
		m_fRingIconSize = 0.06f;
		int nInner = (m_nRingItems + 2) / 3;
		int nOuter = m_nRingItems - nInner;
		for (int i = 0; i < m_nRingItems; ++i)
		{
			bool bInner = i < nInner;
			int nIndex = bInner ? i : i - nInner;
			int nCount = bInner ? nInner : nOuter;
			float fRadius = bInner ? 0.40f : 0.80f;
			float fAngle = kTwoPi * ((float)nIndex + (bInner ? 0.0f : 0.5f)) / (float)nCount;
			m_aRingX[i] = fRadius * (float)sin(fAngle);
			m_aRingY[i] = fRadius * (float)cos(fAngle);
		}
	}

	m_nRingSel = -1;
	m_bRingOpen = true;
	g_pClientSoundMgr->PlayInterfaceSound((char*)g_pInterfaceResMgr->GetSoundSelect());
	return true;
}

void CVRMgr::CloseWeaponRing(bool bChoose)
{
	if (!m_bRingOpen)
		return;
	m_bRingOpen = false;
	if (!bChoose || m_nRingSel < 0 || m_nRingSel >= m_nRingItems || !g_pPlayerMgr)
		return;

	uint8 nWeapon = m_aRingWeapon[m_nRingSel];
	IClientWeaponBase* pCurrent = g_pPlayerMgr->GetCurrentClientWeapon();
	if (pCurrent && pCurrent->GetWeaponId() == nWeapon)
		return;
	g_pPlayerMgr->ChangeWeapon(nWeapon);
}

void CVRMgr::UpdateWeaponRing(float fFrameTime)
{
	bool bGrip = (m_State.hand[LTVR_RIGHT].buttons & LTVR_BTN_GRIP) != 0;

	if (!m_bRingOpen)
	{
		if (Pressed(LTVR_RIGHT, LTVR_BTN_GRIP) && !m_bAdjusting && !IsDecisionOpen())
			OpenWeaponRing();
		return;
	}

	if (!bGrip || m_bAdjusting)
	{
		CloseWeaponRing(!m_bAdjusting);
		return;
	}

	// Which item the hand is nearest, seen from your eyes (the hand's
	// offset from the centre, along the ring's own axes).
	LTVector vCentre, vHand;
	LTRotation rAim;
	if (!GetRingCentre(vCentre) || !GetAimRay(vHand, rAim))
		return;
	LTVector vOffset = vHand - vCentre;
	float fHalf = kRingWidthMeters * 0.5f * (m_State.unitsPerMeter > 1.0f ? m_State.unitsPerMeter : 54.5454f);
	float x = vOffset.Dot(m_vRingRight) / fHalf;
	float y = vOffset.Dot(m_vRingUp) / fHalf;

	int nSel = -1;
	if (x * x + y * y > kRingDeadZone * kRingDeadZone)
	{
		float fBest = 1e9f;
		for (int i = 0; i < m_nRingItems; ++i)
		{
			float dx = x - m_aRingX[i], dy = y - m_aRingY[i];
			float fDist = dx * dx + dy * dy;
			if (fDist < fBest)
			{
				fBest = fDist;
				nSel = i;
			}
		}
	}
	if (nSel != m_nRingSel)
	{
		m_nRingSel = nSel;
		if (nSel >= 0)
			g_pClientSoundMgr->PlayInterfaceSound((char*)g_pInterfaceResMgr->GetSoundSelect());
	}
}

void CVRMgr::RenderWeaponRing()
{
	if (!m_bRingOpen || !m_pfnBeginLayer || !m_pfnEndLayer || !m_pfnSetLayer)
		return;

	LTVector vCentre;
	if (!GetRingCentre(vCentre))
		return;
	float aPos[3] = { vCentre.x, vCentre.y, vCentre.z };
	m_pfnSetLayer(LTVR_LAYER_HAND, 1, aPos, kRingWidthMeters);

	int nSize = m_pfnBeginLayer(LTVR_LAYER_HAND);
	if (!nSize)
		return;

	g_pDrawPrim->SetTransformType(DRAWPRIM_TRANSFORM_SCREEN);
	g_pDrawPrim->SetZBufferMode(DRAWPRIM_NOZ);
	g_pDrawPrim->SetClipMode(DRAWPRIM_NOCLIP);
	g_pDrawPrim->SetFillMode(DRAWPRIM_FILL);
	g_pDrawPrim->SetColorOp(DRAWPRIM_MODULATE);
	g_pDrawPrim->SetAlphaTestMode(DRAWPRIM_NOALPHATEST);
	g_pDrawPrim->SetAlphaBlendMode(DRAWPRIM_BLEND_MOD_SRCALPHA);

	const float fHalf = (float)nSize * 0.5f;
	const float fTexScale = g_pLayoutMgr->GetChooserTextureScale(GetConsoleInt("HUDLayout", 0), 0.75f);
	IClientWeaponBase* pCurrent = g_pPlayerMgr ? g_pPlayerMgr->GetCurrentClientWeapon() : NULL;
	uint8 nCurrent = pCurrent ? pCurrent->GetWeaponId() : WMGR_INVALID_ID;

	for (int i = 0; i < m_nRingItems; ++i)
	{
		WEAPON const* pWeapon = g_pWeaponMgr->GetWeapon(m_aRingWeapon[i]);
		if (!pWeapon)
			continue;

		// The game's own icons: the "normal" one for the highlighted item
		// (and the one in your hand), "unselected" for the rest.
		bool bLit = (i == m_nRingSel) || (m_nRingSel < 0 && m_aRingWeapon[i] == nCurrent);
		std::string icon = bLit ? pWeapon->GetNormalIcon() : pWeapon->GetUnselectedIcon();
		HTEXTURE hTex = g_pInterfaceResMgr->GetTexture(icon.c_str());
		if (!hTex)
			continue;

		uint32 tw = 1, th = 1;
		g_pTexInterface->GetTextureDims(hTex, tw, th);
		float fHeight = m_fRingIconSize * (float)nSize * (i == m_nRingSel ? 1.3f : 1.0f);
		float fWidth = (th > 0) ? fHeight * (float)tw / (float)th : fHeight;
		float cx = fHalf + m_aRingX[i] * fHalf;
		float cy = fHalf - m_aRingY[i] * fHalf;

		LTPoly_GT4 poly;
		memset(&poly, 0, sizeof(poly));
		g_pDrawPrim->SetRGBA(&poly, argbWhite);
		g_pDrawPrim->SetXYWH(&poly, cx - fWidth * 0.5f, cy - fHeight * 0.5f, fWidth, fHeight);
		g_pDrawPrim->SetTexture(hTex);
		SetupQuadUVs(poly, hTex, 0.0f, 0.0f, fTexScale, fTexScale);
		g_pDrawPrim->DrawPrim(&poly, 1);
	}

	// The highlighted item's name in the middle.
	if (!m_pRingText)
	{
		CUIFont* pFont = g_pInterfaceResMgr->GetFont(g_pLayoutMgr->GetHUDFont());
		if (pFont)
		{
			m_pRingText = g_pFontManager->CreateFormattedPolyString(pFont, "", 0.0f, 0.0f);
			if (m_pRingText)
				m_pRingText->SetAlignmentH(CUI_HALIGN_CENTER);
		}
	}
	if (m_pRingText)
	{
		const char* pszName = "";
		int nShow = m_nRingSel;
		if (nShow < 0)
		{
			for (int i = 0; i < m_nRingItems; ++i)
			{
				if (m_aRingWeapon[i] == nCurrent)
					nShow = i;
			}
		}
		WEAPON const* pWeapon = (nShow >= 0) ? g_pWeaponMgr->GetWeapon(m_aRingWeapon[nShow]) : NULL;
		if (pWeapon && pWeapon->szShortName)
			pszName = pWeapon->szShortName;
		m_pRingText->SetText(pszName);
		m_pRingText->SetCharScreenHeight((uint8)(nSize / 18));
		m_pRingText->SetPosition(fHalf + 2.0f, fHalf - (float)(nSize / 36) + 2.0f);
		m_pRingText->SetColor(argbBlack);
		m_pRingText->Render();
		m_pRingText->SetPosition(fHalf, fHalf - (float)(nSize / 36));
		m_pRingText->SetColor(argbWhite);
		m_pRingText->Render();
	}

	m_pfnEndLayer();
}


// ----------------------------------------------------------------------- //
//
//	Wrist and gun HUD
//
//	The game's own health and ammo pieces, drawn exactly as on the monitor
//	but onto a spare canvas; the engine copies that part of the canvas into
//	a small layer that floats by your left wrist (health and armour) or by
//	the gun (ammo), facing you.
//
// ----------------------------------------------------------------------- //

bool CVRMgr::GetCamera(LTVector& vPos, LTRotation& rRot) const
{
	HOBJECT hCamera = g_pPlayerMgr ? g_pPlayerMgr->GetCamera() : LTNULL;
	if (!hCamera)
		return false;
	g_pLTClient->GetObjectPos(hCamera, &vPos);
	g_pLTClient->GetObjectRotation(hCamera, &rRot);
	return true;
}

bool CVRMgr::GetWristHudPose(LTVector& vPos, LTRotation& rRot, float& fOpacity) const
{
	LTVector vLeft, vCamPos;
	LTRotation rLeft, rCamRot;
	if (!GetHandInCamera(LTVR_LEFT, vLeft, rLeft, false) || !GetCamera(vCamPos, rCamRot))
		return false;
	const float fUnitsPerCm = m_State.unitsPerMeter / 100.0f;
	LTVector vOffset(g_vtVRWristHudX.GetFloat(-3.0f), g_vtVRWristHudY.GetFloat(0.0f), g_vtVRWristHudZ.GetFloat(-9.0f));
	vPos = vCamPos + VR_Rotate(rCamRot, vLeft + VR_Rotate(rLeft, vOffset * fUnitsPerCm));

	if (g_vtVRWristHudWatch.GetFloat(1.0f) == 0.0f)
	{
		// Always shown, facing you.
		rRot = rCamRot;
		fOpacity = 1.0f;
		return true;
	}

	// Like a watch on the back of the left wrist. In the controller's axes
	// the back of the hand is -X, the thumb side +Y, the fingers +Z. With
	// your wrist raised in front of you, palm down, you look at the face
	// along +X; its right runs toward your fingers (+Z) and its top away
	// from you (-Y). (LTRotation(forward, up) gives right = +Z for these.)
	LTRotation rFace(LTVector(1.0f, 0.0f, 0.0f), LTVector(0.0f, -1.0f, 0.0f));
	rRot = rCamRot * rLeft * rFace;

	// Shows once you turn it toward you (within about 50 degrees of facing
	// you) and stays until you turn it well away (past about 60), so it
	// doesn't flicker at the edge.
	LTVector vToHead = vCamPos - vPos;
	float fDist = vToHead.Mag();
	if (fDist < 0.001f)
		return false;
	float fFacing = -rRot.Forward().Dot(vToHead / fDist);
	if (fFacing > 0.65f)
		m_bWatchShown = true;
	else if (fFacing < 0.5f)
		m_bWatchShown = false;
	fOpacity = m_bWatchShown ? 1.0f : 0.0f;
	return true;
}

bool CVRMgr::GetGunHudPos(LTVector& vPos) const
{
	LTVector vHand, vCamPos;
	LTRotation rAim, rGun, rCamRot;
	if (!ComputeGun(vHand, rAim, rGun) || !GetCamera(vCamPos, rCamRot))
		return false;
	const float fUnitsPerCm = m_State.unitsPerMeter / 100.0f;
	LTVector vOffset(g_vtVRGunHudX.GetFloat(-6.0f), g_vtVRGunHudY.GetFloat(-2.0f), g_vtVRGunHudZ.GetFloat(6.0f));
	vPos = vCamPos + VR_Rotate(rCamRot, vHand + VR_Rotate(rGun, vOffset * fUnitsPerCm));
	return true;
}

bool CVRMgr::KeepPieceVisible(const char* pszPiece)
{
	if (!pszPiece)
		return false;

	// Log each weapon + piece once (to see what each item hides, and when).
	static char s_aszSeen[64][96];
	static int s_nSeen = 0;
	char szKey[96];
	LTSNPrintF(szKey, sizeof(szKey), "%s / %s", m_szCurrentGun, pszPiece);
	bool bSeen = false;
	for (int i = 0; i < s_nSeen && !bSeen; ++i)
		bSeen = (strcmp(s_aszSeen[i], szKey) == 0);
	if (!bSeen && s_nSeen < 64)
	{
		strcpy(s_aszSeen[s_nSeen++], szKey);
		Log("Animation hides piece: %s", szKey);
	}

	// The two pieces every first-person model shares are Cate's arms (see
	// vr_models.log).
	return IsActive() && (stricmp(pszPiece, "polySurface7") == 0 || stricmp(pszPiece, "polySurface10") == 0);
}

bool CVRMgr::RenderHudItem(CHUDItem* pItem)
{
	if (!pItem || !m_pfnBeginLayerFromScreen || !m_pfnEndLayer || !m_pfnSetLayer || !IsHudFrame())
		return false;

	int nLayer = -1;
	float fWidthCm = 0.0f;
	LTVector vPos;
	LTRotation rRot;
	bool bTurned = false;
	float fOpacity = 1.0f;
	switch (pItem->GetVRSpot())
	{
		case kHUDVRWrist:
			if (g_vtVRWristHud.GetFloat(1.0f) == 0.0f || !GetWristHudPose(vPos, rRot, fOpacity))
				return false;
			if (fOpacity <= 0.0f)
				return true;	// turned away: nothing to show
			bTurned = (g_vtVRWristHudWatch.GetFloat(1.0f) != 0.0f);
			if (bTurned && !m_pfnSetLayerPose)
				return false;	// older engine: back on the panel
			nLayer = LTVR_LAYER_WRIST;
			fWidthCm = g_vtVRWristHudSize.GetFloat(9.0f);
			break;

		case kHUDVRGun:
			// (No gun in your hand: nothing to show; the item draws nothing
			// then anyway.)
			if (g_vtVRGunHud.GetFloat(1.0f) == 0.0f || !GetGunHudPos(vPos))
				return false;
			nLayer = LTVR_LAYER_GUN;
			fWidthCm = g_vtVRGunHudSize.GetFloat(10.0f);
			break;

		default:
			return false;
	}
	if (fWidthCm < 2.0f)	fWidthCm = 2.0f;
	if (fWidthCm > 60.0f)	fWidthCm = 60.0f;

	float aRect[4];
	if (!pItem->GetVRRect(aRect))
		return true;	// nothing to show right now
	if (!m_pfnBeginLayerFromScreen(nLayer, aRect))
		return false;	// couldn't: show it on the panel instead
	pItem->Render();
	m_pfnEndLayer();

	float aPos[3] = { vPos.x, vPos.y, vPos.z };
	if (bTurned)
	{
		float aRot[4] = { rRot.m_Quat[0], rRot.m_Quat[1], rRot.m_Quat[2], rRot.m_Quat[3] };
		m_pfnSetLayerPose(nLayer, 1, aPos, aRot, fWidthCm / 100.0f, fOpacity);
	}
	else
	{
		m_pfnSetLayer(nLayer, 1, aPos, fWidthCm / 100.0f);
	}
	return true;
}
