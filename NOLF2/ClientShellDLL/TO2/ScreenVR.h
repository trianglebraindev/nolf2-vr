// ----------------------------------------------------------------------- //
//
// MODULE  : ScreenVR.h
//
// PURPOSE : Options > VR: the VR settings, and Options > VR performance
//           (NOLF2 VR project)
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
	CMD_VR_RESET,
	CMD_VR_GIVEALL,
	CMD_VR_SETHEIGHT,
	CMD_VR_PERF,			// Options > VR performance
	CMD_VR_PERF_RESET
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
	uint8	m_nSwing;			// 0 off, else 1 + index into kSwingSpeeds
	uint8	m_nReload;			// 0 manual, 1 game original
	uint8	m_nThrow;			// 0 by hand, 1 game original
	uint8	m_nSeated;			// 0 standing, 1 seated
	uint8	m_nVehicleTilt;		// 0 level horizon, 1 tilt with the vehicle
	uint8	m_nCutscenes;		// 0 in 3D, 1 on a flat screen
	uint8	m_nMainHand;		// 0 right, 1 left (VRLeftHanded)
	uint8	m_nWalkStick;		// 0 left stick, 1 right stick (VRWalkStick)
	uint8	m_nVignette;		// 0 off, 1 light, 2 medium, 3 strong
	uint8	m_nVignetteWhen;	// 0 moving, 1 turning, 2 both
	LTBOOL	m_bAdjustClick;		// left stick click adjusts the gun (VRAdjustClick)
};

// Options > VR performance: the settings that trade looks for frame rate.
// (Their own page: the VR page is full.)
class CScreenVRPerf : public CBaseScreen
{
public:
	CScreenVRPerf();
	virtual ~CScreenVRPerf();

    LTBOOL	Build();
    void	OnFocus(LTBOOL bFocus);

	// Settings take effect as soon as they're changed.
    LTBOOL	OnLeft();
    LTBOOL	OnRight();
    LTBOOL	OnLButtonUp(int x, int y);
    LTBOOL	OnRButtonUp(int x, int y);

	void	GetHelpString(uint32 dwHelpId, uint16 nControlIndex, char *buffer, int bufLen);

protected:
    uint32	OnCommand(uint32 dwCommand, uint32 dwParam1, uint32 dwParam2);

	void	ReadSettings();
	void	WriteSettings();

	LTBOOL	m_bMirror;			// VRMirror
	int		m_nRenderScale;		// VRRenderScale, percent
};

#endif // _SCREEN_VR_H_
