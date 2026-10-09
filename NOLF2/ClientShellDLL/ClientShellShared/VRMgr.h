// ----------------------------------------------------------------------- //
//
// MODULE  : VRMgr.h
//
// PURPOSE : The game's side of VR (NOLF2 VR project). Reads the headset
//           and controllers from the engine (sdk/inc/ltvr.h) and turns
//           them into what the game understands: where the player looks,
//           which way they walk and turn, and which commands are pressed.
//
//           Everything here is inert when the game isn't running in VR.
//
// ----------------------------------------------------------------------- //

#ifndef __VRMGR_H__
#define __VRMGR_H__

#include "ltvr.h"

class CVRMgr
{
public:

	CVRMgr();

	// Once per frame, before the player and interface update.
	void		Update(float fFrameTime);

	// VR is running (headset session up). Head tracking and controllers
	// are only meaningful while IsActive().
	bool		IsActive() const	{ return m_State.active != 0; }

	// The raw state, LithTech coordinates, relative to the tracking origin.
	const LTVR_State&	GetState() const	{ return m_State; }

	// Head, relative to the tracking origin (not rotated by the body).
	LTVector	GetHeadOffset() const;			// game units
	LTRotation	GetHeadRotation() const;
	float		GetHeadYaw() const;				// radians, LithTech convention
	float		GetHeadPitch() const;			// radians, positive = looking down

	// Tells the engine where the player's eyes are this frame (no head
	// tracking applied) and which way the body faces. The headset view is
	// built on top of this.
	void		SetOrigin(const LTVector& vEyePos, float fBodyYaw);

	// How much to turn the body this frame (radians), from the turn
	// stick: smooth by default, snap turns if VRSnapTurn is 1.
	float		GetTurnDelta(float fFrameTime);

	// Walking from the move stick, as a world-space direction on the
	// ground whose length is how far the stick is pushed (0..1). By
	// default it follows where the head faces; VRMoveWithHand 1 makes it
	// follow where the left controller points.
	LTVector	GetMoveVector(float fBodyYaw) const;

	void		Recenter();

	// True while this frame's interface drawing goes to the headset's
	// interface layer (the 3D world is in the headset this frame).
	bool		IsHudFrame() const;

	// Fade the headset view to black this frame (0 = clear, 1 = black).
	void		SetFade(float fAlpha);

private:

	void		Init();
	void		SetDefaultBindings();
	void		UpdateMenuControls(float fFrameTime);
	void		UpdateHudMode();
	bool		Pressed(int nHand, unsigned nButton) const;		// went down this frame
	void		SendKey(int nKey);

	bool					m_bInitialized;
	LTVR_GetStateFn			m_pfnGetState;
	LTVR_SetOriginFn		m_pfnSetOrigin;
	LTVR_SetBindingFn		m_pfnSetBinding;
	LTVR_ClearBindingsFn	m_pfnClearBindings;
	LTVR_RecenterFn			m_pfnRecenter;
	LTVR_SetHudModeFn		m_pfnSetHudMode;
	LTVR_SetFadeFn			m_pfnSetFade;
	LTVR_IsHudFrameFn		m_pfnIsHudFrame;

	LTVR_State		m_State;
	unsigned		m_aPrevButtons[2];
	bool			m_bSnapLatched;			// snap turn waits for the stick to return
	int				m_nRepeatKey;			// menu key being held by the stick
	float			m_fRepeatTimer;
};

extern CVRMgr g_VRMgr;

#endif // __VRMGR_H__
