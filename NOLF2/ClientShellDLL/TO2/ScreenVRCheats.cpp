// ----------------------------------------------------------------------- //
//
// MODULE  : ScreenVRCheats.cpp
//
// PURPOSE : Options > Cheats (NOLF2 VR project): the game's own cheats on a
//           page, so testing doesn't need the keyboard, and Level select.
//
// Tre's picks: its own page under VR; god mode, invisible to enemies,
// infinite ammo and flying bodies stay on until turned off (put back after
// every level or save loads, see CVRMgr::UpdateCheats, and remembered in
// vr_settings.txt); the give-items cheats, finishing a level or mission, a
// snowmobile, the chapter unlock; and every level of every mission to start
// fresh at the current difficulty with the game's loadout there. Left out:
// the chase (third person) view, which doesn't work in VR, and the
// multiplayer and debugging cheats.
//
// Both pages borrow the Game options page's look, as the VR page does (see
// ScreenEnum.h).
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "ScreenVRCheats.h"
#include "ScreenMgr.h"
#include "ScreenCommands.h"
#include "VRMgr.h"
#include "GameClientShell.h"	// (brings the interface, layout and string ids)
#include "InterfaceMgr.h"
#include "LayoutMgr.h"
#include "CheatMgr.h"
#include "MissionMgr.h"
#include "MissionButeMgr.h"
#include <map>

namespace
{
	int kGap = 0;

	// The toggles, in page order: the setting each keeps (see VRMgr.cpp).
	const char* const kToggleSettings[] = { "VRCheatGod", "VRCheatInvisible", "VRCheatAmmo", "VRCheatBodies" };
	const int kNumToggles = sizeof(kToggleSettings) / sizeof(kToggleSettings[0]);

	// Help lines: ids of our own, well away from the game's and the VR page's.
	enum
	{
		kHelpBase = 0x00F10000,
		kHelpGod = kHelpBase,
		kHelpInvisible,
		kHelpAmmo,
		kHelpBodies,
		kHelpChapters,
		kHelpGuns,
		kHelpGear,
		kHelpMods,
		kHelpFullAmmo,
		kHelpHealth,
		kHelpArmor,
		kHelpSkills,
		kHelpLevels,
		kHelpExitLevel,
		kHelpNextMission,
		kHelpSnowmobile,
		kHelpEnd
	};

	const char* const kHelpText[kHelpEnd - kHelpBase] =
	{
		"Nothing hurts you. Stays on (through levels and loading) until you turn it off.",
		"Enemies can't see you (they can still hear you). Stays on until you turn it off.",
		"Never run out of ammo; also gives you every weapon. Stays on until you turn it off.",
		"Explosions throw bodies a long way. Stays on until you turn it off.",
		"The main menu's chapter list, as if you'd finished the game.",
		"Every weapon.",
		"Every gadget and piece of gear.",
		"Every mod for the weapons you have.",
		"Full ammo for everything you have.",
		"Full health.",
		"Full armor.",
		"1,000 skill points to spend, each time.",
		"Pick any level of any mission and start it fresh, at this difficulty.",
		"Finish this level now and go on to the next.",
		"Finish this whole mission now.",
		"A snowmobile, right here.",
	};

	// These menus don't scroll: if the rows run past the bottom of the page,
	// close them up evenly to fit (every control but the frame, control 0).
	// (As on the VR page.)
	void FitRows(CBaseScreen* pScreen, std::vector<CLTGUICtrl*>& aControls, LTIntPt& ptNext, int nTop, int nBottom)
	{
		const int nEnd = ptNext.y;
		if (nEnd > nBottom && nEnd > nTop)
		{
			const float fSqueeze = (float)(nBottom - nTop) / (float)(nEnd - nTop);
			for (unsigned i = 1; i < aControls.size(); ++i)
			{
				LTIntPt ptRow = aControls[i]->GetBasePos();
				ptRow.y = nTop + (int)((float)(ptRow.y - nTop) * fSqueeze);
				aControls[i]->SetBasePos(ptRow);
			}
			ptNext.y = nBottom;
		}
	}
}

// ----------------------------------------------------------------------- //
// Options > Cheats
// ----------------------------------------------------------------------- //

CScreenVRCheats::CScreenVRCheats()
{
	m_bGod = m_bInvisible = m_bAmmo = m_bBodies = m_bChapters = LTFALSE;
	for (int i = 0; i < 5; ++i)
		m_abWas[i] = LTFALSE;
}

CScreenVRCheats::~CScreenVRCheats()
{
}

LTBOOL CScreenVRCheats::Build()
{
	CreateTitle((char*)"Cheats");

	kGap = g_pLayoutMgr->GetScreenCustomInt(SCREEN_ID_GAME,"ColumnWidth");

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

	char szOn[16];
	char szOff[16];
	FormatString(IDS_ON,szOn,sizeof(szOn));
	FormatString(IDS_OFF,szOff,sizeof(szOff));

	// Stay on until turned off
	LTBOOL* apToggles[] = { &m_bGod, &m_bInvisible, &m_bAmmo, &m_bBodies, &m_bChapters };
	const char* aszToggles[] = { "God mode", "Invisible to enemies", "Infinite ammo", "Flying bodies", "Chapters unlocked" };
	for (int i = 0; i < 5; ++i)
	{
		CLTGUIToggle* pToggle = AddToggle((char*)aszToggles[i], kHelpGod + i, kGap, apToggles[i]);
		pToggle->SetOnString(szOn);
		pToggle->SetOffString(szOff);
	}

	// Give
	AddTextItem((char*)"All weapons", CMD_CHEAT_GUNS, kHelpGuns);
	AddTextItem((char*)"All gear", CMD_CHEAT_GEAR, kHelpGear);
	AddTextItem((char*)"All mods for my weapons", CMD_CHEAT_MODS, kHelpMods);
	AddTextItem((char*)"Full ammo", CMD_CHEAT_AMMO, kHelpFullAmmo);
	AddTextItem((char*)"Full health", CMD_CHEAT_HEALTH, kHelpHealth);
	AddTextItem((char*)"Full armor", CMD_CHEAT_ARMOR, kHelpArmor);
	AddTextItem((char*)"Skill points", CMD_CHEAT_SKILLS, kHelpSkills);

	// Levels
	AddTextItem((char*)"Level select", CMD_CHEAT_LEVELS, kHelpLevels);
	AddTextItem((char*)"Finish this level", CMD_CHEAT_EXITLEVEL, kHelpExitLevel);
	AddTextItem((char*)"Finish this mission", CMD_CHEAT_NEXTMISSION, kHelpNextMission);
	AddTextItem((char*)"Spawn a snowmobile", CMD_CHEAT_SNOWMOBILE, kHelpSnowmobile);

	FitRows(this, m_controlArray, m_nextPos, GetPageTop(), GetPageBottom());

	int nNeeded = m_nextPos.y - frameRect.top + 8;
	if (nNeeded > nHt + 8)
		pFrame->SetSize(nWd, nNeeded);

	if (! CBaseScreen::Build()) return LTFALSE;

	UseBack(LTTRUE,LTTRUE);
	return LTTRUE;
}

void CScreenVRCheats::RunCheat(int nCheat)
{
	if (!CVRMgr::CanCheat())
	{
		CVRMgr::Log("Cheat %d: not in a game", nCheat);
		return;
	}
	g_pCheatMgr->Run((CheatCode)nCheat);
	CVRMgr::Log("Cheat %d run", nCheat);
	g_pClientSoundMgr->PlayInterfaceSound((char*)g_pInterfaceResMgr->GetSoundSelect());
}

uint32 CScreenVRCheats::OnCommand(uint32 dwCommand, uint32 dwParam1, uint32 dwParam2)
{
	switch(dwCommand)
	{
	case CMD_CHEAT_GUNS:		RunCheat(CHEAT_FULL_WEAPONS);	break;
	case CMD_CHEAT_GEAR:		RunCheat(CHEAT_FULL_GEAR);		break;
	case CMD_CHEAT_MODS:		RunCheat(CHEAT_MODSQUAD);		break;
	case CMD_CHEAT_AMMO:		RunCheat(CHEAT_AMMO);			break;
	case CMD_CHEAT_HEALTH:		RunCheat(CHEAT_HEALTH);			break;
	case CMD_CHEAT_ARMOR:		RunCheat(CHEAT_ARMOR);			break;
	case CMD_CHEAT_SKILLS:		RunCheat(CHEAT_SKILLZ);			break;
	case CMD_CHEAT_EXITLEVEL:	RunCheat(CHEAT_EXITLEVEL);		break;
	case CMD_CHEAT_NEXTMISSION:	RunCheat(CHEAT_NEXTMISSION);	break;
	case CMD_CHEAT_SNOWMOBILE:	RunCheat(CHEAT_SNOWMOBILE);		break;
	case CMD_CHEAT_LEVELS:
		m_pScreenMgr->SetCurrentScreen(SCREEN_ID_VRLEVELS);
		break;
	default:
		return CBaseScreen::OnCommand(dwCommand,dwParam1,dwParam2);
	}
	return 1;
}

void CScreenVRCheats::ReadSettings()
{
	LTBOOL* apToggles[] = { &m_bGod, &m_bInvisible, &m_bAmmo, &m_bBodies };
	for (int i = 0; i < kNumToggles; ++i)
		*apToggles[i] = m_abWas[i] = (CVRMgr::GetSetting(kToggleSettings[i]) != 0.0f);
	m_bChapters = m_abWas[4] = (g_pCheatMgr && g_pCheatMgr->IsEndgameUnlocked()) ? LTTRUE : LTFALSE;
}

void CScreenVRCheats::WriteSettings()
{
	LTBOOL* apToggles[] = { &m_bGod, &m_bInvisible, &m_bAmmo, &m_bBodies };
	for (int i = 0; i < kNumToggles; ++i)
	{
		if (!*apToggles[i] != !m_abWas[i])
		{
			m_abWas[i] = *apToggles[i];
			g_VRMgr.SetStayOnCheat(kToggleSettings[i], *apToggles[i] != LTFALSE);
		}
	}
	if (!m_bChapters != !m_abWas[4] && g_pCheatMgr)
	{
		m_abWas[4] = m_bChapters;
		g_pCheatMgr->SetEndgameUnlocked(m_bChapters != LTFALSE);
	}
}

void CScreenVRCheats::OnFocus(LTBOOL bFocus)
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

LTBOOL CScreenVRCheats::OnLeft()
{
	LTBOOL bHandled = CBaseScreen::OnLeft();
	if (bHandled)
	{
		UpdateData();
		WriteSettings();
	}
	return bHandled;
}

LTBOOL CScreenVRCheats::OnRight()
{
	LTBOOL bHandled = CBaseScreen::OnRight();
	if (bHandled)
	{
		UpdateData();
		WriteSettings();
	}
	return bHandled;
}

LTBOOL CScreenVRCheats::OnLButtonUp(int x, int y)
{
	LTBOOL bHandled = CBaseScreen::OnLButtonUp(x,y);
	if (bHandled)
	{
		UpdateData();
		WriteSettings();
	}
	return bHandled;
}

LTBOOL CScreenVRCheats::OnRButtonUp(int x, int y)
{
	LTBOOL bHandled = CBaseScreen::OnRButtonUp(x,y);
	if (bHandled)
	{
		UpdateData();
		WriteSettings();
	}
	return bHandled;
}

void CScreenVRCheats::GetHelpString(uint32 dwHelpId, uint16 nControlIndex, char *buffer, int bufLen)
{
	if (dwHelpId >= (uint32)kHelpBase && dwHelpId < (uint32)kHelpEnd)
	{
		strncpy(buffer, kHelpText[dwHelpId - kHelpBase], bufLen - 1);
		buffer[bufLen - 1] = '\0';
		return;
	}
	CBaseScreen::GetHelpString(dwHelpId, nControlIndex, buffer, bufLen);
}

// ----------------------------------------------------------------------- //
// Options > Cheats > Level select
// ----------------------------------------------------------------------- //

CScreenVRLevels::CScreenVRLevels()
{
	m_pList = LTNULL;
	m_nListWidth = 0;
}

CScreenVRLevels::~CScreenVRLevels()
{
}

LTBOOL CScreenVRLevels::Build()
{
	CreateTitle((char*)"Level select");

	// The list: the Game page's frame, but stretched left almost to the
	// screen's edge (640 x 480 layout) so the long mission and level names
	// fit (it scrolls).
	LTRect frameRect = g_pLayoutMgr->GetScreenCustomRect(SCREEN_ID_GAME,"FrameRect");
	const int kLeft = 24;
	if (frameRect.left > kLeft)
		frameRect.left = kLeft;
	LTIntPt pos(frameRect.left,frameRect.top);
	int nHt = frameRect.bottom - frameRect.top;
	int nWd = frameRect.right - frameRect.left;
	CVRMgr::Log("Level select: list at x %d..%d, y %d..%d", frameRect.left, frameRect.right, frameRect.top, frameRect.top + nHt);
	if (GetPageBottom() > frameRect.top + nHt)
		nHt = GetPageBottom() - frameRect.top;

	char szFrame[128];
	g_pLayoutMgr->GetScreenCustomString(SCREEN_ID_GAME,"FrameTexture",szFrame,sizeof(szFrame));
	HTEXTURE hFrame = g_pInterfaceResMgr->GetTexture(szFrame);
	CLTGUIFrame *pFrame = debug_new(CLTGUIFrame);
	pFrame->Create(hFrame,nWd,nHt,LTTRUE);
	pFrame->SetBasePos(pos);
	AddControl(pFrame);

	m_nListWidth = nWd - 32;
	m_pList = AddList(pos, (uint16)nHt, LTTRUE, (uint16)(nWd - 16));
	if (m_pList)
	{
		m_pList->SetIndent(LTIntPt(8,8));
		m_pList->SetFrameWidth(2);
	}

	if (! CBaseScreen::Build()) return LTFALSE;

	UseBack(LTTRUE,LTTRUE);
	return LTTRUE;
}

// Every level of every mission, "Mission: level". (Built when the page
// opens: the missions are loaded by then.)
// What a level has to ride, read from its world file's list of objects
// (just their class names, not the level itself): a scripted ride (the game
// moves you along, "PlayerLure") and / or a vehicle you drive
// ("PlayerVehicle"). Remembered for the session.
enum { kLevelRide = 1, kLevelVehicle = 2 };

static int VR_LevelRides(const char* pszLevel)
{
	static std::map<std::string, int> s_mapRides;
	std::map<std::string, int>::iterator it = s_mapRides.find(pszLevel);
	if (it != s_mapRides.end())
		return it->second;

	int nRides = 0;
	char szFile[MAX_PATH];
	const size_t nLen = strlen(pszLevel);
	if (nLen > 4 && stricmp(pszLevel + nLen - 4, ".dat") == 0)
		LTSNPrintF(szFile, sizeof(szFile), "%s", pszLevel);
	else
		LTSNPrintF(szFile, sizeof(szFile), "%s.dat", pszLevel);

	ILTStream* pStream = LTNULL;
	if (g_pLTClient->OpenFile(szFile, &pStream) == LT_OK && pStream)
	{
		uint32 nVersion = 0, nObjectPos = 0, nObjects = 0;
		pStream->Read(&nVersion, sizeof(nVersion));
		pStream->Read(&nObjectPos, sizeof(nObjectPos));
		uint32 nFileLen = 0;
		pStream->GetLen(&nFileLen);
		if (pStream->ErrorStatus() == LT_OK && nObjectPos > 0 && nObjectPos < nFileLen)
		{
			pStream->SeekTo(nObjectPos);
			pStream->Read(&nObjects, sizeof(nObjects));
			if (nObjects > 50000)
				nObjects = 0;
			// (Each object: its size, class name and properties. The size
			// lets us jump to the next one; checked on the first object by
			// reading its properties through as the engine's LoadObjects
			// does, and if it doesn't match, every object is read through.)
			bool bSkip = false;
			for (uint32 i = 0; i < nObjects && pStream->ErrorStatus() == LT_OK; ++i)
			{
				uint16 nDataLen = 0;
				pStream->Read(&nDataLen, sizeof(nDataLen));
				uint32 nStart = 0;
				pStream->GetPos(&nStart);
				char szClass[64];
				pStream->ReadString(szClass, sizeof(szClass));
				if (stricmp(szClass, "PlayerLure") == 0)
					nRides |= kLevelRide;
				else if (stricmp(szClass, "PlayerVehicle") == 0)
					nRides |= kLevelVehicle;

				if (bSkip)
				{
					pStream->SeekTo(nStart + nDataLen);
					continue;
				}

				uint32 nProps = 0;
				pStream->Read(&nProps, sizeof(nProps));
				for (uint32 k = 0; k < nProps && pStream->ErrorStatus() == LT_OK; ++k)
				{
					char szName[8];
					pStream->ReadString(szName, sizeof(szName));	// (reads past the whole name)
					uint8 nCode = 0;
					uint32 nFlags = 0;
					uint16 nPropLen = 0;
					pStream->Read(&nCode, sizeof(nCode));
					pStream->Read(&nFlags, sizeof(nFlags));
					pStream->Read(&nPropLen, sizeof(nPropLen));
					if (nCode == PT_STRING)
					{
						char szValue[8];
						pStream->ReadString(szValue, sizeof(szValue));
					}
					else
					{
						uint32 nPos = 0;
						pStream->GetPos(&nPos);
						pStream->SeekTo(nPos + nPropLen);
					}
				}
				if (i == 0)
				{
					uint32 nEnd = 0;
					pStream->GetPos(&nEnd);
					bSkip = (nEnd == nStart + nDataLen);
				}
			}
		}
		pStream->Release();
	}
	s_mapRides[pszLevel] = nRides;
	return nRides;
}

void CScreenVRLevels::BuildList()
{
	if (!m_pList || !g_pMissionButeMgr)
		return;
	m_pList->RemoveAll();
	m_aLevels.clear();

	const DWORD dwStart = GetTickCount();
	uint8 nFontSize = (uint8)g_pLayoutMgr->GetScreenCustomInt(SCREEN_ID_SINGLE,"ListFontSize");
	for (int m = 0; m < g_pMissionButeMgr->GetNumMissions(); ++m)
	{
		const MISSION* pMission = g_pMissionButeMgr->GetMission(m);
		if (!pMission)
			continue;
		char szMission[128];
		SAFE_STRCPY(szMission, pMission->nNameId ? LoadTempString(pMission->nNameId) : "");

		for (int l = 0; l < pMission->nNumLevels; ++l)
		{
			const LEVEL& level = pMission->aLevels[l];
			if (!level.szLevel[0])
				continue;

			// The level's own name, or else its file's.
			char szLevel[128] = "";
			if (level.nNameId)
				SAFE_STRCPY(szLevel, LoadTempString(level.nNameId));
			if (!szLevel[0])
			{
				const char* pszFile = strrchr(level.szLevel, '\\');
				SAFE_STRCPY(szLevel, pszFile ? pszFile + 1 : level.szLevel);
			}

			// (Marked if there's something to ride, for testing those.)
			const int nRides = VR_LevelRides(level.szLevel);
			const char* pszRides = (nRides == (kLevelRide | kLevelVehicle)) ? "  [ride, vehicle]" :
				(nRides == kLevelRide) ? "  [ride]" : (nRides == kLevelVehicle) ? "  [vehicle]" : "";
			if (nRides)
				CVRMgr::Log("Level select: %s (%s)%s", szLevel, level.szLevel, pszRides);

			char szItem[300];
			if (szMission[0])
				LTSNPrintF(szItem, sizeof(szItem), "%s: %s%s", szMission, szLevel, pszRides);
			else
				LTSNPrintF(szItem, sizeof(szItem), "%s%s", szLevel, pszRides);

			CLTGUITextCtrl* pItem = CreateTextItem(szItem, CMD_VR_LEVEL_FIRST + (uint32)m_aLevels.size(), 0);
			if (nFontSize)
				pItem->SetFont(LTNULL, nFontSize);
			pItem->SetFixedWidth(m_nListWidth, LTTRUE);
			m_pList->AddControl(pItem);
			m_aLevels.push_back(level.szLevel);
		}
	}
	CVRMgr::Log("Level select: %d levels (%lu ms)", (int)m_aLevels.size(), GetTickCount() - dwStart);
}

void CScreenVRLevels::OnFocus(LTBOOL bFocus)
{
	if (bFocus)
		BuildList();
	CBaseScreen::OnFocus(bFocus);
	if (bFocus && m_pList)
		SetSelection(GetIndex(m_pList));
}

uint32 CScreenVRLevels::OnCommand(uint32 dwCommand, uint32 dwParam1, uint32 dwParam2)
{
	if (dwCommand >= (uint32)CMD_VR_LEVEL_FIRST && dwCommand < (uint32)CMD_VR_LEVEL_FIRST + (uint32)m_aLevels.size())
	{
		const std::string& sLevel = m_aLevels[dwCommand - CMD_VR_LEVEL_FIRST];
		CVRMgr::Log("Level select: starting %s", sLevel.c_str());
		g_pMissionMgr->StartGameFromLevel(sLevel.c_str());
		return 1;
	}
	return CBaseScreen::OnCommand(dwCommand,dwParam1,dwParam2);
}
