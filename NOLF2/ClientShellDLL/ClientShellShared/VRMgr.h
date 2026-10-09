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

// Rotates a vector by a rotation, and back.
inline LTVector VR_Rotate(const LTRotation& r, const LTVector& v)
{
	return r.Right() * v.x + r.Up() * v.y + r.Forward() * v.z;
}
inline LTVector VR_Unrotate(const LTRotation& r, const LTVector& v)
{
	return LTVector(v.Dot(r.Right()), v.Dot(r.Up()), v.Dot(r.Forward()));
}

// The smallest rotation that turns direction a into direction b (both unit
// length).
inline LTRotation VR_Arc(const LTVector& a, const LTVector& b)
{
	// (LTVector::Cross returns b x a, so the cross product is written out.)
	LTVector c(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
	float w = 1.0f + a.Dot(b);
	if (w < 1e-4f)
		return LTRotation(0.0f, 0.0f, 0.0f, 1.0f);	// opposite: leave alone
	float fLen = (float)sqrt(c.x * c.x + c.y * c.y + c.z * c.z + w * w);
	return LTRotation(c.x / fLen, c.y / fLen, c.z / fLen, w / fLen);
}

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

	// --- The gun (right hand) ---
	//
	// The game's gun model lives in "really close" space: a miniature copy
	// of the space around the camera, which the engine scales up by
	// GetGunScale() in the headset. The game's normal view shows the gun
	// held at a natural spot in front of you; in VR that whole picture is
	// moved from the natural spot to where the controller is.

	// A controller relative to the game's camera (your head), in game units
	// and the camera's axes: the hand's position (grip) and pointing
	// direction (aim, plus VRGunPitch). False if it isn't tracked.
	bool		GetHandInCamera(int nHand, LTVector& vPos, LTRotation& rRot, bool bGunPitch = true) const;

	float		GetGunScale() const;

	// Where to put a gun model that the normal view puts at vModelPos (in
	// really-close space, no rotation): position and rotation in
	// really-close space. False when the gun isn't held in VR.
	bool		GetGunPlacement(const LTVector& vModelPos, LTVector& vObjPos, LTRotation& rObjRot) const;

	// The aim, in the world: from the hand, pointing where the gun points.
	bool		GetAimRay(LTVector& vFrom, LTRotation& rRot) const;

	// The crosshair layer (see ltvr.h).
	int			BeginCrosshair();
	void		EndCrosshair();
	void		SetCrosshair(bool bVisible, const LTVector& vPos);

	// Appends a line to vr_game.log (run folder). printf-style.
	static void	Log(const char* pszFormat, ...);

	// The weapon now in your hand (its fit is per weapon). Also writes the
	// model's make-up to vr_models.log the first time each one is seen.
	void		SetCurrentGun(const char* pszName, HOBJECT hModel);

	// "Adjust gun" mode (click the left stick while playing): the sticks
	// slide, tilt and resize the gun in your hand; saved to vr_guns.txt.
	bool		IsAdjustingGun() const	{ return m_bAdjusting; }

	// --- Hands ---
	//
	// The game's gun models have both of Cate's arms in them. In VR the
	// forearms are shrunk away so only the gloved hands are left (VRArms 0),
	// the right hand stays on the gun and the left hand follows the left
	// controller. Squeeze the left grip with your left hand near where
	// Cate's left hand holds the weapon to hold it with both hands: the
	// left hand goes back onto the weapon and the gun aims along the line
	// between your hands. Done with the skeleton (node controls), so the
	// models themselves are untouched.

	// After the gun model is made (or remade): hook its arm and wrist bones.
	void		AttachHands(HOBJECT hGunModel);
	// Each frame: where the gun model was put (really-close space).
	void		SetGunModelTransform(const LTVector& vPos, const LTRotation& rRot);
	bool		HideArms() const;
	bool		IsTwoHanded() const		{ return m_bTwoHand; }

	static void	HandsNodeControl(const NodeControlData& data, void* pUser);

private:

	void		Init();
	void		SetDefaultBindings();
	void		UpdateMenuControls(float fFrameTime);
	void		UpdateHudMode();
	void		UpdateGunAdjust(float fFrameTime);
	void		UpdateTwoHand();

	// The right hand's position and aim, and the gun's rotation (fit and
	// two-hand steadying applied), in camera space.
	bool		ComputeGun(LTVector& vHand, LTRotation& rAim, LTRotation& rGun) const;
	bool		GetLeftWristTarget(LTMatrix& mWrist) const;

	HOBJECT		m_hGunModel;
	HMODELNODE	m_hNodeArmL, m_hNodeWristL, m_hNodeArmR, m_hNodeWristR;
	LTMatrix*	m_pArmLMatrix;			// this evaluation's arm bones (shrunk once the wrists are known)
	LTMatrix*	m_pArmRMatrix;
	bool		m_bGunPlaced;
	LTVector	m_vGunObjPos;
	LTRotation	m_rGunObjRot;
	bool		m_bLeftAnimValid;		// the left wrist as the game animates it (model space)
	LTMatrix	m_mLeftWristAnim;
	bool		m_bTwoHand;
	LTVector	m_vTwoHandDir;			// right hand -> left hand when grabbed, gun axes
	bool		m_bLeftOffsetValid;		// left controller -> left wrist, learnt at each grab
	LTMatrix	m_mLeftOffset;			// (kept as a full matrix: the left arm's bones may be
										//  mirrored, which a plain rotation can't hold)
	bool		GetLeftControllerInModel(LTMatrix& mCtrl) const;
	void		LoadHandFit();
	void		SaveHandFit();

	// How a gun sits in your hand, on top of the VRGun* settings.
	struct GunFit
	{
		char	szName[64];
		float	fX, fY, fZ;			// slide, cm: right, up, forward (controller axes)
		float	fYaw, fPitch;		// turn / tilt, degrees (pitch positive = barrel down)
		float	fScale;				// size; 0 = use VRGunScale
	};
	enum { kMaxGunFits = 64 };
	GunFit		m_aGunFits[kMaxGunFits];
	int			m_nGunFits;
	int			m_nCurrentFit;		// index into m_aGunFits, -1 = none
	char		m_szCurrentGun[64];
	bool		m_bAdjusting;

	const GunFit*	GetCurrentFit() const;
	GunFit*		GetOrAddCurrentFit();
	void		LoadGunFits();
	void		SaveGunFits();
	void		Message(const char* pszText);
	bool		Pressed(int nHand, unsigned nButton) const;		// went down this frame
	void		SendKey(int nKey);
	int			StickRepeat(unsigned nDirs, float fFrameTime);	// stick as arrow keys, with repeat
	bool		IsDecisionOpen() const;
	void		UpdateDecisionControls(float fFrameTime);

	bool					m_bInitialized;
	LTVR_GetStateFn			m_pfnGetState;
	LTVR_SetOriginFn		m_pfnSetOrigin;
	LTVR_SetBindingFn		m_pfnSetBinding;
	LTVR_ClearBindingsFn	m_pfnClearBindings;
	LTVR_RecenterFn			m_pfnRecenter;
	LTVR_SetHudModeFn		m_pfnSetHudMode;
	LTVR_SetFadeFn			m_pfnSetFade;
	LTVR_IsHudFrameFn		m_pfnIsHudFrame;
	LTVR_SetCloseScaleFn	m_pfnSetCloseScale;
	LTVR_BeginCrosshairFn	m_pfnBeginCrosshair;
	LTVR_EndCrosshairFn		m_pfnEndCrosshair;
	LTVR_SetCrosshairFn		m_pfnSetCrosshair;
	LTVR_LogModelFn			m_pfnLogModel;

	LTVR_State		m_State;
	unsigned		m_aPrevButtons[2];
	bool			m_bSnapLatched;			// snap turn waits for the stick to return
	int				m_nRepeatKey;			// menu key being held by the stick
	float			m_fRepeatTimer;
};

extern CVRMgr g_VRMgr;

#endif // __VRMGR_H__
