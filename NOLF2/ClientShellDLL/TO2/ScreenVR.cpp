// ----------------------------------------------------------------------- //
//
// MODULE  : ScreenVR.cpp
//
// PURPOSE : Options > VR: the VR settings (NOLF2 VR project)
//
// The settings are console variables (see VRMgr.cpp); this page sets them
// as you change them and saves them to vr_settings.txt when you leave it.
// It borrows the Game options page's look: its screen name is
// "ScreenGame" (see ScreenEnum.h), so the layout file's entries for that
// page (frame, positions, fonts, background) apply here too.
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "ScreenVR.h"
#include "ScreenMgr.h"
#include "ScreenCommands.h"
#include "VRMgr.h"

namespace
{
	int kGap = 0;
	int kWidth = 0;

	const float kSnapAngles[]	= { 15.0f, 30.0f, 45.0f, 60.0f, 90.0f };
	const float kHudDistances[]	= { 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f };
	const float kHudSizes[]		= { 40.0f, 50.0f, 60.0f, 70.0f, 80.0f };	// degrees across

	const int kNumSnapAngles	= sizeof(kSnapAngles) / sizeof(kSnapAngles[0]);
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
		kHelpWorldSize,
		kHelpHudDistance,
		kHelpHudSize,
		kHelpHudFollow,
		kHelpWrist,
		kHelpGunHud,
		kHelpArms,
		kHelpReset,
		kHelpEnd
	};

	const char* const kHelpText[kHelpEnd - kHelpBase] =
	{
		"Turn smoothly with the right stick, or in steps.",
		"How fast smooth turning turns you, in degrees per second.",
		"How far each snap turn turns you, in degrees.",
		"Walk where your head faces, or where your left controller points.",
		"How big the world feels. Higher makes everything larger around you.",
		"How far away the HUD and subtitles float, in metres.",
		"How large the HUD and subtitles look.",
		"Yes: the HUD stays put and glides back when you turn well away. No: it stays in front of your eyes.",
		"Health and armour on your left wrist: shown as you turn it toward you like a watch, always shown, or off (on the HUD).",
		"Show your ammo on the gun instead of on the HUD.",
		"Show just Cate's hands, or her full arms.",
		"Put every setting on this page back to how it started.",
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
	pCycle->AddString("Left hand");

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

	CLTGUIToggle* pToggle = AddToggle((char*)"HUD stays put", kHelpHudFollow, kGap, &m_bHudFollow);
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

	AddTextItem((char*)"Reset to defaults", CMD_VR_RESET, kHelpReset);

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
				"VRWristHudWatch", "VRGunHud", "VRArms" };
			for (int i = 0; i < (int)(sizeof(aNames) / sizeof(aNames[0])); ++i)
				CVRMgr::SetSetting(aNames[i], CVRMgr::GetSettingDefault(aNames[i]));
			ReadSettings();
			UpdateData(LTFALSE);
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
}
