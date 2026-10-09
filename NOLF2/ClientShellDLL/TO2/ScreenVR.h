// ----------------------------------------------------------------------- //
//
// MODULE  : ScreenVR.h
//
// PURPOSE : Options > VR: the VR settings (NOLF2 VR project)
//
// ----------------------------------------------------------------------- //

#ifndef _SCREEN_VR_H_
#define _SCREEN_VR_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "BaseScreen.h"
#include "ScreenCommands.h"

// Commands for this screen and the Options entry that opens it.
enum
{
	CMD_VR_OPTIONS = CMD_CUSTOM + 300,
	CMD_VR_RESET
};

class CScreenVR : public CBaseScreen
{
public:
	CScreenVR();
	virtual ~CScreenVR();

	// Build the screen
    LTBOOL	Build();
    void	OnFocus(LTBOOL bFocus);

	// Settings take effect as soon as they're changed.
    LTBOOL	OnLeft();
    LTBOOL	OnRight();
    LTBOOL	OnLButtonUp(int x, int y);
    LTBOOL	OnRButtonUp(int x, int y);

	// The help line under the options (this screen's own text).
	void	GetHelpString(uint32 dwHelpId, uint16 nControlIndex, char *buffer, int bufLen);

protected:
    uint32	OnCommand(uint32 dwCommand, uint32 dwParam1, uint32 dwParam2);

	void	ReadSettings();		// settings -> controls
	void	WriteSettings();	// controls -> settings

	uint8	m_nTurnMode;		// 0 smooth, 1 snap
	int		m_nTurnSpeed;		// degrees per second
	uint8	m_nSnapAngle;		// index into kSnapAngles
	uint8	m_nMoveDir;			// 0 head, 1 left hand
	int		m_nWorldSize;		// percent
	uint8	m_nHudDistance;		// index into kHudDistances
	uint8	m_nHudSize;			// index into kHudSizes
	LTBOOL	m_bHudFollow;
	uint8	m_nWristMode;		// 0 off (on the HUD), 1 watch, 2 always shown
	LTBOOL	m_bGunHud;
	uint8	m_nArms;			// 0 hands only, 1 full arms
};

#endif // _SCREEN_VR_H_
