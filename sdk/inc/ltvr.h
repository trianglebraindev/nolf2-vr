//------------------------------------------------------------------
//
//  MODULE  : LTVR.H
//
//  PURPOSE : The VR interface between the engine (Lithtech.exe) and
//            the game code (CShell.dll). NOLF2 VR project.
//
//  The engine owns the headset (OpenXR, see vr/xrbridge and
//  runtime/render_a/src/sys/d3d/d3d_vr.cpp). The game reads the
//  headset and controller state through these plain-C functions,
//  which Lithtech.exe exports. Find them with:
//
//      GetProcAddress(GetModuleHandle(NULL), "LTVR_GetState")
//
//  Plain C on purpose: the game DLL is copied to a temp folder and
//  loaded separately, so nothing C++-specific crosses the boundary.
//
//  COORDINATES: LithTech's own. Game units (see unitsPerMeter),
//  left-handed, +X right, +Y up, +Z forward. Poses are relative to
//  the tracking origin: the head's position and heading at the last
//  recentre. They are NOT rotated by the player's body heading; the
//  game applies that itself.
//
//------------------------------------------------------------------

#ifndef __LTVR_H__
#define __LTVR_H__

#define LTVR_VERSION	3

#define LTVR_LEFT		0
#define LTVR_RIGHT		1

// Controller buttons, one bit each, per hand.
#define LTVR_BTN_TRIGGER		(1 << 0)	// trigger pulled past halfway
#define LTVR_BTN_GRIP			(1 << 1)	// grip squeezed past halfway
#define LTVR_BTN_STICK			(1 << 2)	// thumbstick clicked
#define LTVR_BTN_PRIMARY		(1 << 3)	// A (right) / X (left)
#define LTVR_BTN_SECONDARY		(1 << 4)	// B (right) / Y (left)
#define LTVR_BTN_MENU			(1 << 5)	// menu button (left only)
#define LTVR_BTN_STICK_UP		(1 << 6)	// thumbstick pushed firmly in a
#define LTVR_BTN_STICK_DOWN		(1 << 7)	//   direction (for using the stick
#define LTVR_BTN_STICK_LEFT		(1 << 8)	//   as buttons, e.g. jump/crouch)
#define LTVR_BTN_STICK_RIGHT	(1 << 9)

typedef struct LTVR_Pose
{
	int		valid;		// 0 if not tracked this frame
	float	pos[3];		// game units, relative to the tracking origin
	float	rot[4];		// quaternion x, y, z, w (same layout as LTRotation)
} LTVR_Pose;

typedef struct LTVR_HandState
{
	int			active;		// controller connected and tracked
	LTVR_Pose	grip;		// where the hand holds the controller
	LTVR_Pose	aim;		// pointing ray: position + forward (+Z)
	float		trigger;	// 0..1
	float		squeeze;	// grip 0..1
	float		stick[2];	// thumbstick x (right +), y (forward +), -1..1
	unsigned	buttons;	// LTVR_BTN_* currently held
} LTVR_HandState;

typedef struct LTVR_State
{
	int				version;		// LTVR_VERSION
	int				active;			// VR is running (headset session up)
	int				tracking;		// headset is showing our frames and tracking
	float			unitsPerMeter;	// world scale in use
	LTVR_Pose		head;
	LTVR_HandState	hand[2];		// LTVR_LEFT, LTVR_RIGHT
} LTVR_State;

// Fills *pState with the latest headset/controller state. Returns 0 (and
// sets pState->active = 0) when VR isn't running.
typedef int  (*LTVR_GetStateFn)(LTVR_State* pState);

// The game tells the engine where the player's eyes are this frame
// (without any head tracking: standing still, eyes level) and which way
// the body faces (yaw, radians, LithTech convention: 0 = +Z, positive
// turns toward +X). The headset view is built on top of this. Call every
// frame the player's view is rendered.
typedef void (*LTVR_SetOriginFn)(const float pos[3], float fYaw);

// Binds a controller button (hand + one LTVR_BTN_* bit) to an engine
// command ID: while the button is held, the command reads as on, exactly
// as if its key were held. commandId < 0 removes the binding.
typedef void (*LTVR_SetBindingFn)(int hand, unsigned button, int commandId);

// Removes all button bindings.
typedef void (*LTVR_ClearBindingsFn)(void);

// Re-centres: the head's current position and heading become the origin.
typedef void (*LTVR_RecenterFn)(void);

// --- Interface (version 2) ---
//
// While the 3D world is in the headset, everything the game draws after
// the world (HUD, subtitles, menus, cursor, console) is caught in a
// transparent layer and shown over the world, in one of two ways:

#define LTVR_HUD_HEAD	0	// fixed in front of the eyes, moves with the head
#define LTVR_HUD_PANEL	1	// a panel that stays where it appeared (menus)

typedef void (*LTVR_SetHudModeFn)(int mode);

// Fades the headset view to black: 0 = clear, 1 = black. Call during the
// frame (before the flip) every frame a fade is wanted; it resets to 0
// after each frame. Also darkens whatever interface has been drawn so far
// this frame, like the game's own fade does on a monitor.
typedef void (*LTVR_SetFadeFn)(float alpha);

// Non-zero while this frame's interface drawing goes to the headset's
// interface layer (the 3D world was drawn in the headset this frame).
typedef int  (*LTVR_IsHudFrameFn)(void);

// --- The gun and the crosshair (version 3) ---

// The game draws its gun in "really close" space, a miniature space around
// the camera with its own projection. In the headset that space is drawn
// in the world around the camera, scaled up by fScale (really-close units
// -> game units) so the gun has true size and depth.
typedef void (*LTVR_SetCloseScaleFn)(float fScale);

// Draw the crosshair into its own small layer: call Begin, draw centred
// in a square of the returned size (pixels; 0 = not now, draw nothing),
// then End. Only during interface drawing on a HUD frame.
typedef int  (*LTVR_BeginCrosshairFn)(void);
typedef void (*LTVR_EndCrosshairFn)(void);

// Where in the world the crosshair goes this frame (the point the gun is
// aimed at). Not called / bVisible 0 = no crosshair this frame.
typedef void (*LTVR_SetCrosshairFn)(int bVisible, const float pos[3]);

#endif // __LTVR_H__
