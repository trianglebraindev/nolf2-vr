//------------------------------------------------------------------
//
//  MODULE  : XRBRIDGE.H
//
//  PURPOSE : Connects a Direct3D 9Ex renderer to an OpenXR headset.
//
//  HOW IT WORKS
//  ------------
//  OpenXR runtimes (VDXR, SteamVR, ...) do not accept Direct3D 9
//  images. So the bridge creates a small Direct3D 11 device of its
//  own, on the same GPU, and a set of "shared" textures that both
//  APIs can see:
//
//    D3D9Ex renders into a shared texture
//        -> the bridge waits for D3D9 to finish that frame
//        -> D3D11 draws the shared texture into the headset's image
//        -> the image is handed to OpenXR.
//
//  The bridge knows nothing about the game. It just provides:
//    - per-eye textures to render into, plus each eye's pose and
//      field of view (for proper stereo), and
//    - a "screen" texture shown as a flat floating screen (for menus,
//      cutscenes, and for testing the plumbing), or with transparency
//      laid over the 3D world (the game's HUD).
//
//  Coordinates are OpenXR's: metres, right-handed, +Y up, -Z forward.
//
//------------------------------------------------------------------

#ifndef __XRBRIDGE_H__
#define __XRBRIDGE_H__

#include <d3d9.h>

namespace xrb
{

struct Quat { float x, y, z, w; };
struct Vec3 { float x, y, z; };

struct Pose
{
	Quat	orientation;
	Vec3	position;
};

// Angles in radians, measured from straight ahead. Left and down are
// negative. Each eye's field of view is lopsided (asymmetric).
struct Fov
{
	float	angleLeft, angleRight, angleUp, angleDown;
};

struct EyeView
{
	Pose	pose;		// eye pose in the tracking space
	Fov		fov;
};

enum Eye { EYE_LEFT = 0, EYE_RIGHT = 1, EYE_COUNT = 2 };

enum Hand { HAND_LEFT = 0, HAND_RIGHT = 1, HAND_COUNT = 2 };

// One motion controller. Poses are in the same space as the eye views.
struct HandInput
{
	bool	active;			// connected and tracked this frame
	bool	gripValid;
	bool	aimValid;
	Pose	grip;			// where the hand holds the controller
	Pose	aim;			// pointing ray (forward = -Z, OpenXR convention)
	float	trigger;		// 0..1
	float	squeeze;		// 0..1
	float	stickX, stickY;	// -1..1, right / forward positive
	bool	stickClick;
	bool	primary;		// A (right) / X (left)
	bool	secondary;		// B (right) / Y (left)
	bool	menu;			// left menu button
	bool	touchValid;		// the controller reports the touches below
	bool	triggerTouch;	// index finger on the trigger
	bool	thumbTouch;		// thumb on the stick, a button or the thumb rest
};

struct ScreenDesc
{
	unsigned	width, height;		// pixel size of the screen texture
	float		widthMeters;		// physical width of the floating screen
	float		distanceMeters;		// how far in front of the start position
};

// Where and how to show the screen texture this frame.
struct ScreenPlacement
{
	bool		headLocked;		// true: pose is relative to the head and the
								// screen moves with it. false: pose is in the
								// same space as the eye views (stays put).
	Pose		pose;			// centre of the screen; it faces +Z of this pose
	float		widthMeters;	// height follows the texture's shape
	bool		alpha;			// the texture has transparency (premultiplied
								// alpha) and is laid over the 3D world
};

// An extra flat layer (see CreateLayer) to show this frame.
struct LayerSubmit
{
	int				layer;		// from CreateLayer
	ScreenPlacement	place;
};

class Bridge
{
public:
	Bridge();
	~Bridge();

	// Creates the OpenXR instance and finds the headset. Call before the
	// game creates its D3D9 device: the device must be created on the
	// adapter this returns, or the shared textures cannot be shared.
	// Returns false if no OpenXR runtime / headset is available.
	bool		InitInstance(const char* pszAppName);

	// The Direct3D 9 adapter index the headset is connected to.
	// Valid after InitInstance. -1 if no match was found.
	int			GetD3D9AdapterIndex(IDirect3D9Ex* pD3D9Ex) const;

	// Creates the D3D11 device, OpenXR session and all textures.
	// pDevice must be a D3D9Ex device on GetD3D9AdapterIndex().
	bool		InitSession(IDirect3DDevice9Ex* pDevice, const ScreenDesc& screen);

	void		Shutdown();

	// Per frame, in this order:
	//   BeginFrame() -> render into the textures you intend to submit
	//   -> EndFrame().
	// BeginFrame handles OpenXR events, waits for the headset's frame
	// timing (this is what paces the game to the headset) and reads the
	// latest head/eye poses. Returns false when nothing should be drawn
	// to the headset this frame (session not running / headset off);
	// EndFrame must still be called after a true return only.
	bool		BeginFrame();

	// bSubmitEyes:   show the per-eye textures as the 3D world.
	// bSubmitScreen: show the screen texture as a flat floating screen,
	//                where the ScreenDesc given to InitSession says.
	void		EndFrame(bool bSubmitEyes, bool bSubmitScreen);

	// As above, with the screen placed as pScreen says (NULL: no screen).
	void		EndFrame(bool bSubmitEyes, const ScreenPlacement* pScreen);

	// As above, plus extra layers. They are drawn over the 3D world and
	// under the screen, in the order given.
	void		EndFrame(bool bSubmitEyes, const ScreenPlacement* pScreen,
						 const LayerSubmit* pLayers, unsigned nLayers);

	// Extra flat layers, e.g. the crosshair. Call after InitSession.
	// Returns the layer's number, or -1 on failure. The texture is a
	// D3DFMT_A8R8G8B8 render target, used with premultiplied alpha.
	int					CreateLayer(unsigned nWidth, unsigned nHeight);
	IDirect3DTexture9*	GetLayerTexture(int nLayer) const;

	bool		IsInitialized() const		{ return m_pImpl != 0 && m_bSessionCreated; }
	bool		IsSessionRunning() const;
	bool		ShouldQuit() const;		// runtime asked the app to exit

	// The eyes' size this frame: their textures' size times the render
	// scale (SetEyeRenderScale, 0.5 - 1). Draw into the top left that big;
	// EndFrame stretches it over the headset's images. The textures
	// themselves are always full size (GetEyeTextureWidth / Height).
	unsigned	GetEyeWidth() const;
	unsigned	GetEyeHeight() const;
	unsigned	GetEyeTextureWidth() const;
	unsigned	GetEyeTextureHeight() const;
	void		SetEyeRenderScale(float fScale);
	float		GetEyeRenderScale() const;

	// Timing, for the performance line in the log: how long the last
	// xrWaitFrame waited on the headset (ms; time to spare), and the
	// headset's frame time (ms, from its refresh rate).
	double		GetLastWaitMs() const;
	double		GetDisplayPeriodMs() const;

	// The last EndFrame, part by part (ms), for the hitch lines:
	enum
	{
		SUBMIT_WAIT_D3D9,	// the game's drawing finishing on the graphics card
		SUBMIT_IMAGE_WAIT,	// xrWaitSwapchainImage: the runtime handing us its images
		SUBMIT_COPY,		// copying into them (D3D11)
		SUBMIT_WAIT_D3D11,	// those copies finishing on the graphics card
		SUBMIT_END_FRAME,	// xrEndFrame: the runtime taking the frame
		SUBMIT_PARTS
	};
	void		GetLastSubmitTimes(double aMs[SUBMIT_PARTS]) const;

	// Textures owned by the bridge; do not release. Render targets in
	// D3DFMT_A8R8G8B8.
	IDirect3DTexture9*	GetEyeTexture(Eye eEye) const;
	IDirect3DTexture9*	GetScreenTexture() const;

	// Remakes the screen texture at a new size (the game changed its
	// screen resolution). The old GetScreenTexture() is gone afterwards.
	bool				ResizeScreen(unsigned nWidth, unsigned nHeight);

	// True when this frame's eye poses are tracked (valid after a
	// BeginFrame() that returned true). If false, draw the flat screen.
	bool				HasValidViews() const;

	// Valid after a BeginFrame() that returned true.
	const EyeView&		GetEyeView(Eye eEye) const;

	// Where the head is, in the same space as the eye views.
	const Pose&			GetHeadPose() const;

	// Motion controller state, read in BeginFrame. Buttons and sticks
	// only update while the game has input focus.
	const HandInput&	GetHand(Hand eHand) const;

	// True while the headset is showing our frames and the app has
	// input focus.
	bool				HasFocus() const;

	// Re-centre: the current head position/heading becomes the origin.
	// Takes effect at the next BeginFrame where the head is tracked.
	void		Recenter();

private:
	struct Impl;
	Impl*	m_pImpl;
	bool	m_bSessionCreated;

	Bridge(const Bridge&);
	Bridge& operator=(const Bridge&);
};

// Writes a line to xrbridge.log (in the working folder) and to the
// debugger output. printf-style.
void Log(const char* pszFormat, ...);

} // namespace xrb

#endif // __XRBRIDGE_H__
