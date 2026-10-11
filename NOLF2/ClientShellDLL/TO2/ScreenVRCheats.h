// ----------------------------------------------------------------------- //
//
// MODULE  : ScreenVRCheats.h
//
// PURPOSE : Options > Cheats: the game's own cheats on a page (NOLF2 VR
//           project), and Options > Cheats > Level select.
//
// ----------------------------------------------------------------------- //

#ifndef _SCREEN_VR_CHEATS_H_
#define _SCREEN_VR_CHEATS_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "BaseScreen.h"
#include "ScreenCommands.h"
#include <vector>
#include <string>

// Commands for these screens and the Options entry that opens them (the VR
// page's are CMD_CUSTOM + 300 on).
enum
{
	CMD_VR_CHEATS = CMD_CUSTOM + 320,
	CMD_CHEAT_GUNS,
	CMD_CHEAT_GEAR,
	CMD_CHEAT_MODS,
	CMD_CHEAT_AMMO,
	CMD_CHEAT_HEALTH,
	CMD_CHEAT_ARMOR,
	CMD_CHEAT_SKILLS,
	CMD_CHEAT_LEVELS,
	CMD_CHEAT_EXITLEVEL,
	CMD_CHEAT_NEXTMISSION,
	CMD_CHEAT_SNOWMOBILE,
	CMD_VR_LEVEL_FIRST = CMD_CUSTOM + 2000		// Level select: + the level's number in the list
};

class CScreenVRCheats : public CBaseScreen
{
public:
	CScreenVRCheats();
	virtual ~CScreenVRCheats();

    LTBOOL	Build();
    void	OnFocus(LTBOOL bFocus);

	// Toggles take effect as soon as they're changed.
    LTBOOL	OnLeft();
    LTBOOL	OnRight();
    LTBOOL	OnLButtonUp(int x, int y);
    LTBOOL	OnRButtonUp(int x, int y);

	void	GetHelpString(uint32 dwHelpId, uint16 nControlIndex, char *buffer, int bufLen);

protected:
    uint32	OnCommand(uint32 dwCommand, uint32 dwParam1, uint32 dwParam2);

	void	ReadSettings();		// settings -> controls
	void	WriteSettings();	// controls -> settings (and the cheats, for any that changed)
	void	RunCheat(int nCheat);

	LTBOOL	m_bGod;
	LTBOOL	m_bInvisible;
	LTBOOL	m_bAmmo;			// infinite ammo (the game's "tears")
	LTBOOL	m_bBodies;			// explosions throw bodies far
	LTBOOL	m_bChapters;		// the main menu's chapter list unlocked
	LTBOOL	m_abWas[5];			// (as last set, to act only on changes)
};

class CScreenVRLevels : public CBaseScreen
{
public:
	CScreenVRLevels();
	virtual ~CScreenVRLevels();

    LTBOOL	Build();
    void	OnFocus(LTBOOL bFocus);

protected:
    uint32	OnCommand(uint32 dwCommand, uint32 dwParam1, uint32 dwParam2);
	void	BuildList();

	CLTGUIListCtrl*				m_pList;
	int							m_nListWidth;
	std::vector<std::string>	m_aLevels;		// level files, in list order
};

#endif // _SCREEN_VR_CHEATS_H_
