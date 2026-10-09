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
//      cutscenes, and for testing the plumbing).
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

struct ScreenDesc
{
	unsigned	width, height;		// pixel size of the screen texture
	float		widthMeters;		// physical width of the floating screen
	float		distanceMeters;		// how far in front of the start position
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
	// bSubmitScreen: show the screen texture as a flat floating screen.
	void		EndFrame(bool bSubmitEyes, bool bSubmitScreen);

	bool		IsInitialized() const		{ return m_pImpl != 0 && m_bSessionCreated; }
	bool		IsSessionRunning() const;
	bool		ShouldQuit() const;		// runtime asked the app to exit

	unsigned	GetEyeWidth() const;
	unsigned	GetEyeHeight() const;

	// Textures owned by the bridge; do not release. Render targets in
	// D3DFMT_A8R8G8B8.
	IDirect3DTexture9*	GetEyeTexture(Eye eEye) const;
	IDirect3DTexture9*	GetScreenTexture() const;

	// True when this frame's eye poses are tracked (valid after a
	// BeginFrame() that returned true). If false, draw the flat screen.
	bool				HasValidViews() const;

	// Valid after a BeginFrame() that returned true.
	const EyeView&		GetEyeView(Eye eEye) const;

	// Where the head is, in the same space as the eye views.
	const Pose&			GetHeadPose() const;

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
