// ----------------------------------------------------------------------- //
//
// MODULE  : ScreenVR.cpp
//
// PURPOSE : Options > VR: the VR settings, and Options > VR performance
//           (NOLF2 VR project)
//
// The settings are console variables (see VRMgr.cpp); these pages set them
// as you change them and save them to vr_settings.txt when you leave.
// They borrow the Game options page's look: its screen name is
// "ScreenGame" (see ScreenEnum.h), so the layout file's entries for that
// page (frame, positions, fonts, background) apply here too.
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "ScreenVR.h"
#include "ScreenMgr.h"
#include "ScreenCommands.h"
#include "VRMgr.h"
#include "GameClientShell.h"	// (brings the interface, layout and string ids)
#include "InterfaceMgr.h"
#include "LayoutMgr.h"
#include "CheatMgr.h"

namespace
{
	int kGap = 0;
	int kWidth = 0;

	const float kSnapAngles[]	= { 15.0f, 30.0f, 45.0f, 60.0f, 90.0f };
	const float kHudDistances[]	= { 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f };
	const float kHudSizes[]		= { 40.0f, 50.0f, 60.0f, 70.0f, 80.0f };	// degrees across

	const int kNumSnapAngles	= sizeof(kSnapAngles) / sizeof(kSnapAngles[0]);
	// Sword swings: how fast the blade's tip must move (m/s), light / normal / hard.
	const float kSwingSpeeds[]	= { 1.5f, 2.5f, 4.0f };
	const int kNumSwingSpeeds	= sizeof(kSwingSpeeds) / sizeof(kSwingSpeeds[0]);
	const int kNumHudDistances	= sizeof(kHudDistances) / sizeof(kHudDistances[0]);
	const int kNumHudSizes		= sizeof(kHudSizes) / sizeof(kHudSizes[0]);

	// Help lines: ids of our own, well away from the game's string ids.
	enum
	{
		kHelpBase = 0x00F00000,
		kHelpTurnMode = kHelpBase,
		kHelpTurnSpeed,
		kHelpSnapAngle,
		kHelpMoveDir,
		kHelpMainHand,
		kHelpWalkStick,
		kHelpSeated,
		kHelpSetHeight,
		kHelpVignette,
		kHelpVignetteWhen,
		kHelpWorldSize,
		kHelpHudDistance,
		kHelpHudSize,
		kHelpHudFollow,
		kHelpWrist,
		kHelpGunHud,
		kHelpArms,
		kHelpSwing,
		kHelpReload,
		kHelpThrow,
		kHelpVehicleTilt,
		kHelpCutscenes,
		kHelpReset,
		kHelpGiveAll,
		kHelpAdjustClick,
		kHelpMirror,
		kHelpRenderScale,
		kHelpPerfReset,
		kHelpTestSubmit,
		kHelpEnd
	};

	const char* const kHelpText[kHelpEnd - kHelpBase] =
	{
		"Turn smoothly with the right stick, or in steps.",
		"How fast smooth turning turns you, in degrees per second.",
		"How far each snap turn turns you, in degrees.",
		"Walk where your head faces, or where the hand with the walking stick points.",
		"The hand that holds the gun. Left: the gun, your free hand and every button swap sides (fire on the left trigger, magazines from over your right shoulder, health on your right wrist).",
		"The stick you walk with; the other turns. Your choice, whichever hand holds the gun.",
		"Standing: crouch for real and Cate crouches (the right stick still crouches too). Seated: the right stick crouches. Then set your height.",
		"Sit or stand as you'll play and look ahead: your head becomes Cate standing up. In play: hold both stick clicks for a second.",
		"Darken the edges of your view while you move or turn with the sticks, for comfort. Stronger narrows the view more.",
		"What brings the comfort vignette on: moving with the stick, smooth turning, or both.",
		"How big the world feels. Higher makes everything larger around you.",
		"How far away the HUD and subtitles float, in metres.",
		"How large the HUD and subtitles look.",
		"Yes: the HUD stays put and glides back when you turn well away. No: it stays in front of your eyes.",
		"Health and armour on your left wrist: shown as you turn it toward you like a watch, always shown, or off (on the HUD).",
		"Show your ammo on the gun instead of on the HUD.",
		"Show just Cate's hands, or her full arms.",
		"Swing a sword to hit with it: how hard a swing has to be, or off (then only the trigger slashes). The trigger works either way.",
		"Manual: A drops the magazine, grip over your left shoulder for a new one and push it into the gun (stick click: next ammo). Game original: stick click reloads.",
		"By hand: hold the trigger, swing your arm and let go to throw (the harder, the further). Game original: the trigger throws straight ahead.",
		"Riding the snowmobile: keep the horizon level (comfortable), or let the view bank and climb with it.",
		"In 3D: the scene plays out around you and you can look about. Flat screen: watch it framed as the game's camera shot it, on a screen in front of you.",
		"Put every setting on this page back to how it started.",
		"For testing: every weapon, gadget and piece of gear, with full ammo (single player, in a mission).",
		"On: click the left stick to move the gun, scope, magazine grip or snowmobile hands to fit you; click again to save. Off: stray clicks do nothing.",
		"On: the monitor shows what the headset shows. Off: the monitor stays black, which saves a little work each frame.",
		"How sharp the headset picture is drawn, as a percentage of what your headset asks for. Lower runs faster but looks softer; 70 draws about half the pixels of 100.",
		"Put the settings on this page back to how they started.",
		"For tracking down stutters: what the game sends to the headset. World only hides the HUD, crosshair and read-outs. Not saved: back to Everything next time.",
	};

	// The index of the closest value in a list.
	uint8 Closest(const float* pValues, int nCount, float fValue)
	{
		int nBest = 0;
		for (int i = 1; i < nCount; ++i)
		{
			if (fabs(pValues[i] - fValue) < fabs(pValues[nBest] - fValue))
				nBest = i;
		}
		return (uint8)nBest;
	}
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CScreenVR::CScreenVR()
{
	m_nTurnMode = 0;
	m_nTurnSpeed = 150;
	m_nSnapAngle = 2;
	m_nMoveDir = 0;
	m_nWorldSize = 100;
	m_nHudDistance = 2;
	m_nHudSize = 2;
	m_bHudFollow = LTTRUE;
	m_nWristMode = 1;
	m_bGunHud = LTTRUE;
	m_nArms = 0;
	m_nSwing = 2;
	m_nReload = 0;
	m_nThrow = 0;
	m_nSeated = 0;
	m_nVehicleTilt = 0;
	m_nCutscenes = 0;
	m_nMainHand = 0;
	m_nWalkStick = 0;
	m_nVignette = 0;
	m_nVignetteWhen = 2;
	m_bAdjustClick = LTFALSE;
}

CScreenVR::~CScreenVR()
{
}

// Build the screen
LTBOOL CScreenVR::Build()
{
	CreateTitle((char*)"VR");

	kGap = g_pLayoutMgr->GetScreenCustomInt(SCREEN_ID_GAME,"ColumnWidth");
	kWidth = g_pLayoutMgr->GetScreenCustomInt(SCREEN_ID_GAME,"SliderWidth");

	// Background frame, like the Game options page (made taller below if
	// our list is longer).
	LTRect frameRect = g_pLayoutMgr->GetScreenCustomRect(SCREEN_ID_GAME,"FrameRect");
	LTIntPt pos(frameRect.left,frameRect.top);
	int nHt = frameRect.bottom - frameRect.top;
	int nWd = frameRect.right - frameRect.left;

	char szFrame[128];
	g_pLayoutMgr->GetScreenCustomString(SCREEN_ID_GAME,"FrameTexture",szFrame,sizeof(szFrame));
	HTEXTURE hFrame = g_pInterfaceResMgr->GetTexture(szFrame);
	CLTGUIFrame *pFrame = debug_new(CLTGUIFrame);
	pFrame->Create(hFrame,nWd,nHt+8,LTTRUE);
	pFrame->SetBasePos(pos);
	pFrame->SetBorder(2,m_SelectedColor);
	AddControl(pFrame);

	char szYes[16];
	char szNo[16];
	FormatString(IDS_YES,szYes,sizeof(szYes));
	FormatString(IDS_NO,szNo,sizeof(szNo));
	char szTmp[64];

	// Turning and moving
	CLTGUICycleCtrl* pCycle = AddCycle((char*)"Turning", kHelpTurnMode, kGap, &m_nTurnMode);
	pCycle->AddString("Smooth");
	pCycle->AddString("Snap");

	CLTGUISlider* pSlider = AddSlider((char*)"Smooth turn speed", kHelpTurnSpeed, kGap, kWidth, -1, &m_nTurnSpeed);
	pSlider->SetSliderRange(60, 360);
	pSlider->SetSliderIncrement(30);
	pSlider->SetNumericDisplay(LTTRUE);

	pCycle = AddCycle((char*)"Snap turn step", kHelpSnapAngle, kGap, &m_nSnapAngle);
	for (int i = 0; i < kNumSnapAngles; ++i)
	{
		sprintf(szTmp, "%d", (int)kSnapAngles[i]);
		pCycle->AddString(szTmp);
	}

	pCycle = AddCycle((char*)"Walk toward", kHelpMoveDir, kGap, &m_nMoveDir);
	pCycle->AddString("Head");
	pCycle->AddString("Hand");

	pCycle = AddCycle((char*)"Gun hand", kHelpMainHand, kGap, &m_nMainHand);
	pCycle->AddString("Right");
	pCycle->AddString("Left");

	pCycle = AddCycle((char*)"Walk with", kHelpWalkStick, kGap, &m_nWalkStick);
	pCycle->AddString("Left stick");
	pCycle->AddString("Right stick");

	CLTGUIToggle* pToggle = AddToggle((char*)"Adjust with stick click", kHelpAdjustClick, kGap, &m_bAdjustClick);
	pToggle->SetOnString((char*)"On");
	pToggle->SetOffString((char*)"Off");

	pCycle = AddCycle((char*)"Play position", kHelpSeated, kGap, &m_nSeated);
	pCycle->AddString("Standing");
	pCycle->AddString("Seated");

	AddTextItem((char*)"Set my height now", CMD_VR_SETHEIGHT, kHelpSetHeight);

	pCycle = AddCycle((char*)"Comfort vignette", kHelpVignette, kGap, &m_nVignette);
	pCycle->AddString("Off");
	pCycle->AddString("Light");
	pCycle->AddString("Medium");
	pCycle->AddString("Strong");

	pCycle = AddCycle((char*)"Vignette when", kHelpVignetteWhen, kGap, &m_nVignetteWhen);
	pCycle->AddString("Moving");
	pCycle->AddString("Turning");
	pCycle->AddString("Moving and turning");

	pSlider = AddSlider((char*)"World size", kHelpWorldSize, kGap, kWidth, -1, &m_nWorldSize);
	pSlider->SetSliderRange(70, 130);
	pSlider->SetSliderIncrement(5);
	pSlider->SetNumericDisplay(LTTRUE);

	// HUD
	pCycle = AddCycle((char*)"HUD distance", kHelpHudDistance, kGap, &m_nHudDistance);
	for (int i = 0; i < kNumHudDistances; ++i)
	{
		sprintf(szTmp, "%g m", kHudDistances[i]);
		pCycle->AddString(szTmp);
	}

	pCycle = AddCycle((char*)"HUD size", kHelpHudSize, kGap, &m_nHudSize);
	pCycle->AddString("Very small");
	pCycle->AddString("Small");
	pCycle->AddString("Medium");
	pCycle->AddString("Large");
	pCycle->AddString("Very large");

	pToggle = AddToggle((char*)"HUD stays put", kHelpHudFollow, kGap, &m_bHudFollow);
	pToggle->SetOnString(szYes);
	pToggle->SetOffString(szNo);

	pCycle = AddCycle((char*)"Health on wrist", kHelpWrist, kGap, &m_nWristMode);
	pCycle->AddString("Off");
	pCycle->AddString("Like a watch");
	pCycle->AddString("Always");

	pToggle = AddToggle((char*)"Ammo on gun", kHelpGunHud, kGap, &m_bGunHud);
	pToggle->SetOnString(szYes);
	pToggle->SetOffString(szNo);

	// Hands
	pCycle = AddCycle((char*)"Arms", kHelpArms, kGap, &m_nArms);
	pCycle->AddString("Hands only");
	pCycle->AddString("Full arms");

	// Melee
	pCycle = AddCycle((char*)"Reloading", kHelpReload, kGap, &m_nReload);
	pCycle->AddString("Manual");
	pCycle->AddString("Game original");

	pCycle = AddCycle((char*)"Throwing", kHelpThrow, kGap, &m_nThrow);
	pCycle->AddString("By hand");
	pCycle->AddString("Game original");

	pCycle = AddCycle((char*)"Snowmobile view", kHelpVehicleTilt, kGap, &m_nVehicleTilt);
	pCycle->AddString("Level horizon");
	pCycle->AddString("Tilt with it");

	pCycle = AddCycle((char*)"Cutscenes", kHelpCutscenes, kGap, &m_nCutscenes);
	pCycle->AddString("In 3D around you");
	pCycle->AddString("On a flat screen");

	pCycle = AddCycle((char*)"Sword swings", kHelpSwing, kGap, &m_nSwing);
	pCycle->AddString("Off");
	pCycle->AddString("Light");
	pCycle->AddString("Normal");
	pCycle->AddString("Hard");

	AddTextItem((char*)"Reset to defaults", CMD_VR_RESET, kHelpReset);
	AddTextItem((char*)"Testing: all weapons and gear", CMD_VR_GIVEALL, kHelpGiveAll);

	// These menus don't scroll: if the rows run past the bottom of the page,
	// close them up evenly to fit (every control but the frame, control 0).
	{
		const int nTop = GetPageTop();
		const int nBottom = GetPageBottom();
		const int nEnd = m_nextPos.y;
		CVRMgr::Log("VR settings page: %d controls, rows from y %d to %d, page %d to %d", (int)m_controlArray.size(),
			nTop, nEnd, nTop, nBottom);
		if (nEnd > nBottom && nEnd > nTop)
		{
			const float fSqueeze = (float)(nBottom - nTop) / (float)(nEnd - nTop);
			for (unsigned i = 1; i < m_controlArray.size(); ++i)
			{
				LTIntPt ptRow = m_controlArray[i]->GetBasePos();
				ptRow.y = nTop + (int)((float)(ptRow.y - nTop) * fSqueeze);
				m_controlArray[i]->SetBasePos(ptRow);
			}
			m_nextPos.y = nBottom;
			CVRMgr::Log("VR settings page: rows closed up to %.0f%% to fit", fSqueeze * 100.0f);
		}
	}

	// Taller frame if the list runs past it.
	int nNeeded = m_nextPos.y - frameRect.top + 8;
	if (nNeeded > nHt + 8)
		pFrame->SetSize(nWd, nNeeded);

	// Make sure to call the base class
	if (! CBaseScreen::Build()) return LTFALSE;

	UseBack(LTTRUE,LTTRUE);
	return LTTRUE;
}

uint32 CScreenVR::OnCommand(uint32 dwCommand, uint32 dwParam1, uint32 dwParam2)
{
	switch(dwCommand)
	{
	case CMD_VR_RESET:
		{
			const char* const aNames[] = { "VRSnapTurn", "VRTurnSpeed", "VRSnapAngle", "VRMoveWithHand",
				"VRUnitsPerMeter", "VRHudDistance", "VRHudAngle", "VRHudFollow", "VRWristHud",
				"VRWristHudWatch", "VRGunHud", "VRArms", "VRSwing", "VRSwingSpeed", "VRVignette",
				"VRVignetteWhen", "VRReload", "VRThrow", "VRSeated", "VRVehicleTilt", "VRCutsceneFlat", "VRLeftHanded", "VRWalkStick", "VRAdjustClick" };
			for (int i = 0; i < (int)(sizeof(aNames) / sizeof(aNames[0])); ++i)
				CVRMgr::SetSetting(aNames[i], CVRMgr::GetSettingDefault(aNames[i]));
			ReadSettings();
			UpdateData(LTFALSE);
			break;
		}
	case CMD_VR_SETHEIGHT:
		{
			// (Standing / seated as picked on this page, before it's left.)
			UpdateData();
			WriteSettings();
			g_VRMgr.SetHeightNow();
			g_pClientSoundMgr->PlayInterfaceSound((char*)g_pInterfaceResMgr->GetSoundSelect());
			break;
		}
	case CMD_VR_GIVEALL:
		{
			// The game's own cheats ("guns", "gear", "ammo"), so testing
			// doesn't need the keyboard. Single player, in a mission only.
			if (g_pCheatMgr && g_pLTClient->IsConnected() && !IsMultiplayerGame())
			{
				g_pCheatMgr->Check("guns");
				g_pCheatMgr->Check("gear");
				g_pCheatMgr->Check("ammo");
				g_pClientSoundMgr->PlayInterfaceSound((char*)g_pInterfaceResMgr->GetSoundSelect());
			}
			break;
		}
	default:
		return CBaseScreen::OnCommand(dwCommand,dwParam1,dwParam2);
	}
	return 1;
}

void CScreenVR::OnFocus(LTBOOL bFocus)
{
	if (bFocus)
	{
		ReadSettings();
		UpdateData(LTFALSE);
	}
	else
	{
		UpdateData();
		WriteSettings();
		CVRMgr::SaveSettings();
	}
	CBaseScreen::OnFocus(bFocus);
}

LTBOOL CScreenVR::OnLeft()
{
	LTBOOL bHandled = CBaseScreen::OnLeft();
	if (bHandled)
	{
		UpdateData();
		WriteSettings();
	}
	return bHandled;
}

LTBOOL CScreenVR::OnRight()
{
	LTBOOL bHandled = CBaseScreen::OnRight();
	if (bHandled)
	{
		UpdateData();
		WriteSettings();
	}
	return bHandled;
}

LTBOOL CScreenVR::OnLButtonUp(int x, int y)
{
	LTBOOL bHandled = CBaseScreen::OnLButtonUp(x,y);
	if (bHandled)
	{
		UpdateData();
		WriteSettings();
	}
	return bHandled;
}

LTBOOL CScreenVR::OnRButtonUp(int x, int y)
{
	LTBOOL bHandled = CBaseScreen::OnRButtonUp(x,y);
	if (bHandled)
	{
		UpdateData();
		WriteSettings();
	}
	return bHandled;
}

void CScreenVR::GetHelpString(uint32 dwHelpId, uint16 nControlIndex, char *buffer, int bufLen)
{
	if (dwHelpId >= (uint32)kHelpBase && dwHelpId < (uint32)kHelpEnd)
	{
		strncpy(buffer, kHelpText[dwHelpId - kHelpBase], bufLen - 1);
		buffer[bufLen - 1] = '\0';
		return;
	}
	CBaseScreen::GetHelpString(dwHelpId, nControlIndex, buffer, bufLen);
}

void CScreenVR::ReadSettings()
{
	m_nTurnMode		= CVRMgr::GetSetting("VRSnapTurn") != 0.0f ? 1 : 0;
	m_nTurnSpeed	= (int)(CVRMgr::GetSetting("VRTurnSpeed") + 0.5f);
	m_nSnapAngle	= Closest(kSnapAngles, kNumSnapAngles, CVRMgr::GetSetting("VRSnapAngle"));
	m_nMoveDir		= CVRMgr::GetSetting("VRMoveWithHand") != 0.0f ? 1 : 0;

	// World size, percent of normal: 60 game units to the metre is 100%.
	float fUnitsPerMeter = CVRMgr::GetSetting("VRUnitsPerMeter");
	if (fUnitsPerMeter < 1.0f)
		fUnitsPerMeter = 60.0f;
	m_nWorldSize = 5 * (int)(6000.0f / fUnitsPerMeter / 5.0f + 0.5f);

	m_nHudDistance	= Closest(kHudDistances, kNumHudDistances, CVRMgr::GetSetting("VRHudDistance"));
	m_nHudSize		= Closest(kHudSizes, kNumHudSizes, CVRMgr::GetSetting("VRHudAngle"));
	m_bHudFollow	= CVRMgr::GetSetting("VRHudFollow") != 0.0f;

	if (CVRMgr::GetSetting("VRWristHud") == 0.0f)
		m_nWristMode = 0;
	else
		m_nWristMode = (CVRMgr::GetSetting("VRWristHudWatch") != 0.0f) ? 1 : 2;

	m_bGunHud		= CVRMgr::GetSetting("VRGunHud") != 0.0f;
	m_nArms			= CVRMgr::GetSetting("VRArms") != 0.0f ? 1 : 0;
	int nVignette = (int)(CVRMgr::GetSetting("VRVignette") + 0.5f);
	m_nVignette = (uint8)((nVignette < 0) ? 0 : (nVignette > 3 ? 3 : nVignette));
	int nWhen = (int)(CVRMgr::GetSetting("VRVignetteWhen") + 0.5f);
	m_nVignetteWhen = (uint8)((nWhen == 1) ? 0 : (nWhen == 2 ? 1 : 2));
	m_nReload = (CVRMgr::GetSetting("VRReload") != 0.0f) ? 0 : 1;
	m_nThrow = (CVRMgr::GetSetting("VRThrow") != 0.0f) ? 0 : 1;
	m_nSeated = (CVRMgr::GetSetting("VRSeated") != 0.0f) ? 1 : 0;
	m_nVehicleTilt = (CVRMgr::GetSetting("VRVehicleTilt") != 0.0f) ? 1 : 0;
	m_nCutscenes = (CVRMgr::GetSetting("VRCutsceneFlat") != 0.0f) ? 1 : 0;
	m_nMainHand = (CVRMgr::GetSetting("VRLeftHanded") != 0.0f) ? 1 : 0;
	m_nWalkStick = (CVRMgr::GetSetting("VRWalkStick") != 0.0f) ? 1 : 0;
	m_bAdjustClick = CVRMgr::GetSetting("VRAdjustClick") != 0.0f;
	if (CVRMgr::GetSetting("VRSwing") == 0.0f)
		m_nSwing = 0;
	else
		m_nSwing = 1 + Closest(kSwingSpeeds, kNumSwingSpeeds, CVRMgr::GetSetting("VRSwingSpeed"));
}

void CScreenVR::WriteSettings()
{
	CVRMgr::SetSetting("VRSnapTurn",		m_nTurnMode ? 1.0f : 0.0f);
	CVRMgr::SetSetting("VRTurnSpeed",		(float)m_nTurnSpeed);
	if (m_nSnapAngle < kNumSnapAngles)
		CVRMgr::SetSetting("VRSnapAngle",	kSnapAngles[m_nSnapAngle]);
	CVRMgr::SetSetting("VRMoveWithHand",	m_nMoveDir ? 1.0f : 0.0f);
	if (m_nWorldSize > 0)
		CVRMgr::SetSetting("VRUnitsPerMeter", 6000.0f / (float)m_nWorldSize);
	if (m_nHudDistance < kNumHudDistances)
		CVRMgr::SetSetting("VRHudDistance",	kHudDistances[m_nHudDistance]);
	if (m_nHudSize < kNumHudSizes)
		CVRMgr::SetSetting("VRHudAngle",	kHudSizes[m_nHudSize]);
	CVRMgr::SetSetting("VRHudFollow",		m_bHudFollow ? 1.0f : 0.0f);
	CVRMgr::SetSetting("VRWristHud",		m_nWristMode ? 1.0f : 0.0f);
	if (m_nWristMode)
		CVRMgr::SetSetting("VRWristHudWatch", m_nWristMode == 1 ? 1.0f : 0.0f);
	CVRMgr::SetSetting("VRGunHud",			m_bGunHud ? 1.0f : 0.0f);
	CVRMgr::SetSetting("VRArms",			m_nArms ? 1.0f : 0.0f);
	CVRMgr::SetSetting("VRVignette",		(float)m_nVignette);
	CVRMgr::SetSetting("VRVignetteWhen",	(float)(m_nVignetteWhen + 1));
	CVRMgr::SetSetting("VRReload",			m_nReload == 0 ? 1.0f : 0.0f);
	CVRMgr::SetSetting("VRThrow",			m_nThrow == 0 ? 1.0f : 0.0f);
	CVRMgr::SetSetting("VRSeated",			m_nSeated ? 1.0f : 0.0f);
	CVRMgr::SetSetting("VRVehicleTilt",		m_nVehicleTilt ? 1.0f : 0.0f);
	CVRMgr::SetSetting("VRCutsceneFlat",	m_nCutscenes ? 1.0f : 0.0f);
	CVRMgr::SetSetting("VRLeftHanded",		m_nMainHand ? 1.0f : 0.0f);
	CVRMgr::SetSetting("VRWalkStick",		m_nWalkStick ? 1.0f : 0.0f);
	CVRMgr::SetSetting("VRAdjustClick",		m_bAdjustClick ? 1.0f : 0.0f);
	CVRMgr::SetSetting("VRSwing",			m_nSwing ? 1.0f : 0.0f);
	if (m_nSwing > 0 && m_nSwing <= kNumSwingSpeeds)
		CVRMgr::SetSetting("VRSwingSpeed",	kSwingSpeeds[m_nSwing - 1]);
}

//////////////////////////////////////////////////////////////////////
// Options > VR performance
//////////////////////////////////////////////////////////////////////

CScreenVRPerf::CScreenVRPerf()
{
	m_bMirror = LTTRUE;
	m_nRenderScale = 100;
	m_nTestSubmit = 0;
}

CScreenVRPerf::~CScreenVRPerf()
{
}

LTBOOL CScreenVRPerf::Build()
{
	CreateTitle((char*)"VR performance");

	kGap = g_pLayoutMgr->GetScreenCustomInt(SCREEN_ID_GAME,"ColumnWidth");
	kWidth = g_pLayoutMgr->GetScreenCustomInt(SCREEN_ID_GAME,"SliderWidth");

	// Background frame, like the Game options page.
	LTRect frameRect = g_pLayoutMgr->GetScreenCustomRect(SCREEN_ID_GAME,"FrameRect");
	LTIntPt pos(frameRect.left,frameRect.top);
	int nHt = frameRect.bottom - frameRect.top;
	int nWd = frameRect.right - frameRect.left;

	char szFrame[128];
	g_pLayoutMgr->GetScreenCustomString(SCREEN_ID_GAME,"FrameTexture",szFrame,sizeof(szFrame));
	HTEXTURE hFrame = g_pInterfaceResMgr->GetTexture(szFrame);
	CLTGUIFrame *pFrame = debug_new(CLTGUIFrame);
	pFrame->Create(hFrame,nWd,nHt+8,LTTRUE);
	pFrame->SetBasePos(pos);
	pFrame->SetBorder(2,m_SelectedColor);
	AddControl(pFrame);

	CLTGUIToggle* pToggle = AddToggle((char*)"Desktop mirror", kHelpMirror, kGap, &m_bMirror);
	pToggle->SetOnString((char*)"On");
	pToggle->SetOffString((char*)"Off");

	CLTGUISlider* pSlider = AddSlider((char*)"Render resolution %", kHelpRenderScale, kGap, kWidth, -1, &m_nRenderScale);
	pSlider->SetSliderRange(50, 100);
	pSlider->SetSliderIncrement(10);
	pSlider->SetNumericDisplay(LTTRUE);

	CLTGUICycleCtrl* pCycle = AddCycle((char*)"Testing: send to headset", kHelpTestSubmit, kGap, &m_nTestSubmit);
	pCycle->AddString((char*)"Everything");
	pCycle->AddString((char*)"World only");
	pCycle->AddString((char*)"World only, monitor off");

	AddTextItem((char*)"Reset to defaults", CMD_VR_PERF_RESET, kHelpPerfReset);

	if (! CBaseScreen::Build()) return LTFALSE;

	UseBack(LTTRUE,LTTRUE);
	return LTTRUE;
}

uint32 CScreenVRPerf::OnCommand(uint32 dwCommand, uint32 dwParam1, uint32 dwParam2)
{
	if (dwCommand == CMD_VR_PERF_RESET)
	{
		CVRMgr::SetSetting("VRMirror", CVRMgr::GetSettingDefault("VRMirror"));
		CVRMgr::SetSetting("VRRenderScale", CVRMgr::GetSettingDefault("VRRenderScale"));
		CVRMgr::SetSetting("VRTestSubmit", 0.0f);
		ReadSettings();
		UpdateData(LTFALSE);
		return 1;
	}
	return CBaseScreen::OnCommand(dwCommand,dwParam1,dwParam2);
}

void CScreenVRPerf::OnFocus(LTBOOL bFocus)
{
	if (bFocus)
	{
		ReadSettings();
		UpdateData(LTFALSE);
	}
	else
	{
		UpdateData();
		WriteSettings();
		CVRMgr::SaveSettings();
	}
	CBaseScreen::OnFocus(bFocus);
}

LTBOOL CScreenVRPerf::OnLeft()
{
	LTBOOL bHandled = CBaseScreen::OnLeft();
	if (bHandled)
	{
		UpdateData();
		WriteSettings();
	}
	return bHandled;
}

LTBOOL CScreenVRPerf::OnRight()
{
	LTBOOL bHandled = CBaseScreen::OnRight();
	if (bHandled)
	{
		UpdateData();
		WriteSettings();
	}
	return bHandled;
}

LTBOOL CScreenVRPerf::OnLButtonUp(int x, int y)
{
	LTBOOL bHandled = CBaseScreen::OnLButtonUp(x,y);
	if (bHandled)
	{
		UpdateData();
		WriteSettings();
	}
	return bHandled;
}

LTBOOL CScreenVRPerf::OnRButtonUp(int x, int y)
{
	LTBOOL bHandled = CBaseScreen::OnRButtonUp(x,y);
	if (bHandled)
	{
		UpdateData();
		WriteSettings();
	}
	return bHandled;
}

void CScreenVRPerf::GetHelpString(uint32 dwHelpId, uint16 nControlIndex, char *buffer, int bufLen)
{
	if (dwHelpId >= (uint32)kHelpBase && dwHelpId < (uint32)kHelpEnd)
	{
		strncpy(buffer, kHelpText[dwHelpId - kHelpBase], bufLen - 1);
		buffer[bufLen - 1] = '\0';
		return;
	}
	CBaseScreen::GetHelpString(dwHelpId, nControlIndex, buffer, bufLen);
}

void CScreenVRPerf::ReadSettings()
{
	m_bMirror = CVRMgr::GetSetting("VRMirror") != 0.0f;
	int n = (int)(CVRMgr::GetSetting("VRRenderScale") + 0.5f);
	n = 10 * ((n + 5) / 10);		// (the slider's steps)
	m_nRenderScale = (n < 50) ? 50 : (n > 100 ? 100 : n);
	int nTest = (int)(CVRMgr::GetSetting("VRTestSubmit") + 0.5f);
	m_nTestSubmit = (uint8)((nTest < 0) ? 0 : (nTest > 2 ? 2 : nTest));
}

void CScreenVRPerf::WriteSettings()
{
	CVRMgr::SetSetting("VRMirror",		m_bMirror ? 1.0f : 0.0f);
	CVRMgr::SetSetting("VRRenderScale",	(float)m_nRenderScale);
	CVRMgr::SetSetting("VRTestSubmit",	(float)m_nTestSubmit);	// (not in the saved list: testing only)
}
