//------------------------------------------------------------------
//
//  MODULE  : D3D_VR.CPP
//
//  PURPOSE : Hooks between the Direct3D 9 renderer and the VR bridge.
//
//  CURRENT STAGE: stereo world + head tracking.
//
//  Each frame, the game's main camera is rendered once per eye into
//  the headset's textures, with that eye's own position and its own
//  off-centre field of view, straight from the headset. The game's
//  camera supplies the body: where you stand, and which way your body
//  faces (its yaw). Your head supplies everything else. The game's own
//  camera pitch and roll are ignored in the headset, because your
//  head does that now.
//
//  Screens with no 3D world (menus, loading, movies) still go to the
//  floating flat screen.
//
//  INTERFACE LAYER
//  On frames where the world goes to the headset, everything the game
//  draws after the world (HUD, subtitles, dialogue choices, in-game
//  menus, mouse cursor, console) is drawn into the screen texture
//  instead of the monitor picture, on a transparent background. That
//  layer is shown over the 3D world: fixed in front of the eyes while
//  playing, or as a panel that stays put for menus (LTVR_SetHudMode).
//  The monitor still gets the world with the interface laid on top.
//
//  Transparency: the texture is cleared to fully transparent, and alpha
//  blending is told to build up coverage in the alpha channel
//  (D3DRS_SEPARATEALPHABLENDENABLE). The colour that results is
//  "premultiplied" (already multiplied by its coverage), which is what
//  the headset's layer blending expects.
//
//  THE GUN
//  The game draws its gun (and the gun's muzzle flash and attachments)
//  in "really close" space: a miniature space around the camera, drawn
//  with its own projection. In the headset that space is placed in the
//  world around the game's camera, scaled up to true size (the game
//  says how much, LTVR_SetCloseScale), and drawn with each eye's own
//  view, so it has real depth. The game moves the gun within that space
//  to where the controller is.
//
//  SPOT LAYERS (crosshair, weapon ring, wrist and gun read-outs)
//  Small layers the game draws into (LTVR_BeginLayer / LTVR_EndLayer)
//  and places at a point in the world (LTVR_SetLayer), facing you: the
//  crosshair where the gun points (constant apparent size), the weapon
//  ring at your hand, health on your wrist and ammo on the gun (a fixed
//  size in metres). The wrist and gun read-outs are the game's own HUD
//  pieces: the game draws them where they'd be on the monitor, onto a
//  spare screen-sized canvas, and the engine copies that part of the
//  canvas into the layer (LTVR_BeginLayerFromScreen).
//
//  COORDINATES
//  OpenXR:   metres, right-handed, +X right, +Y up, -Z forward.
//  LithTech: game units, left-handed, +X right, +Y up, +Z forward.
//  Converting is a mirror along Z: positions (x, y, -z), rotations
//  (-x, -y, z, w), and metres * VRUnitsPerMeter.
//
//------------------------------------------------------------------

#include "precompile.h"

#include "d3d_vr.h"
#include "d3d_device.h"
#include "d3d_shell.h"
#include "3d_ops.h"
#include "renderstruct.h"
#include "common_draw.h"
#include "common_stuff.h"
#include "rendertargetmgr.h"
#include "rendererconsolevars.h"
#include "xrbridge.h"
#include "ltvr.h"

void d3d_FullDrawScene(const ViewParams& Params, SceneDesc *pDesc);	// d3d_draw.cpp

static xrb::Bridge*			g_pVRBridge			= NULL;
static IDirect3DSurface9*	g_pVRScreenSurf		= NULL;	// the floating screen texture's surface
static IDirect3DSurface9*	g_pVREyeSurf[2]		= { NULL, NULL };
static IDirect3DSurface9*	g_pVREyeDepth		= NULL;	// shared by both eyes, one after the other
static int					g_nVRRequested		= -1;	// -1 = not checked yet
static bool					g_bVRFrameOpen		= false;	// BeginFrame done, EndFrame not yet
static bool					g_bVREyesDrawn		= false;	// eyes rendered during the open frame

// Interface layer (see the top of this file).
static uint32				g_nVRScreenWidth	= 0;		// size of the screen texture (= monitor picture)
static uint32				g_nVRScreenHeight	= 0;
static bool					g_bVREyesJustDrawn	= false;	// eyes drawn for the scene being rendered
static bool					g_bVRHudActive		= false;	// interface drawing goes to the screen texture
static IDirect3DSurface9*	g_pVRHudSavedRT		= NULL;		// the monitor's back buffer meanwhile
static int					g_nVRHudMode		= LTVR_HUD_HEAD;
static bool					g_bVRPanelPlaced	= false;
static xrb::Pose			g_VRPanelPose;
static float				g_fVRFade			= 0.0f;		// 0 clear .. 1 black, this frame

// The HUD when it follows lazily (VRHudFollow 1): its heading, in tracking
// space, and whether it is gliding back in front of you.
static bool					g_bVRHudYawValid	= false;
static float				g_fVRHudYaw			= 0.0f;		// radians, OpenXR: 0 = -Z, positive turns left
static bool					g_bVRHudGliding		= false;
static LARGE_INTEGER		g_VRLastSwapTime;

// Really close space (the gun).
static float				g_fVRCloseScale		= 1.0f;		// really-close units -> world units
static D3DMATRIX			g_mVRCloseToWorld;

// The body transform the eyes were drawn with this frame (for placing
// things given in world coordinates, like the crosshair).
static LTVector				g_vVRBodyPosUsed;
static LTRotation			g_rVRBodyUsed;
static bool					g_bVRBodyUsedValid	= false;

// Spot layers (see the top of this file). Index = LTVR_LAYER_*.
static const uint32			kVRSpotWidth[LTVR_NUM_LAYERS]	= { 512, 512, 512, 512 };
static const uint32			kVRSpotHeight[LTVR_NUM_LAYERS]	= { 512, 512, 256, 256 };
struct VRSpot
{
	int					nLayer;			// bridge layer, -1 = none
	IDirect3DSurface9*	pSurf;
	bool				bDrawn;			// drawn into this frame
	bool				bSet;			// placed this frame
	LTVector			vPos;			// world position of its centre
	float				fWidth;			// metres; 0 = keep the HUD's size per degree
};
static VRSpot				g_aVRSpots[LTVR_NUM_LAYERS];
static int					g_nVRSpotDrawing	= -1;		// layer being drawn into, -1 = none

// The spare canvas for LTVR_BeginLayerFromScreen (the size of the screen),
// and the part of it being copied into the layer.
static IDirect3DSurface9*	g_pVRCanvas			= NULL;
static uint32				g_nVRCanvasWidth	= 0;
static uint32				g_nVRCanvasHeight	= 0;
static bool					g_bVRSpotFromCanvas	= false;
static RECT					g_rcVRCanvasSrc;

// Where the game says the player's eyes are this frame, and the body's
// heading (see LTVR_SetOrigin). Falls back to the camera if not set.
static bool					g_bVROriginSet		= false;
static LTVector				g_vVROrigin;
static float				g_fVROriginYaw		= 0.0f;

// Latest state handed to the game (LTVR_GetState).
static LTVR_State			g_VRState;

// Controller buttons bound to engine commands (LTVR_SetBinding).
struct VRBinding { int nHand; unsigned nButton; int nCommand; };
static VRBinding			g_aVRBindings[64];
static int					g_nVRBindings		= 0;

static void VR_BuildState();
static void VR_EndHud();

static void VR_ReleaseCanvas()
{
	if (g_pVRCanvas)
	{
		g_pVRCanvas->Release();
		g_pVRCanvas = NULL;
	}
	g_nVRCanvasWidth = g_nVRCanvasHeight = 0;
	g_bVRSpotFromCanvas = false;
}

//------------------------------------------------------------------
// Set-up and tear-down
//------------------------------------------------------------------

// Looks for a whole-word argument (e.g. "-vr") on the process command line.
static bool HasCommandLineArg(const char* pszArg)
{
	const char* p = GetCommandLineA();
	size_t nLen = strlen(pszArg);
	while (p && *p)
	{
		while (*p == ' ' || *p == '\t')
			++p;
		const char* pStart = p;
		bool bQuoted = false;
		while (*p && (bQuoted || (*p != ' ' && *p != '\t')))
		{
			if (*p == '"')
				bQuoted = !bQuoted;
			++p;
		}
		if ((size_t)(p - pStart) == nLen && _strnicmp(pStart, pszArg, nLen) == 0)
			return true;
	}
	return false;
}

bool d3d_VR_Requested()
{
	if (g_nVRRequested < 0)
		g_nVRRequested = HasCommandLineArg("-vr") ? 1 : 0;
	return g_nVRRequested == 1;
}

static void VR_Destroy()
{
	VR_EndHud();
	for (int e = 0; e < 2; ++e)
	{
		if (g_pVREyeSurf[e]) { g_pVREyeSurf[e]->Release(); g_pVREyeSurf[e] = NULL; }
	}
	if (g_pVREyeDepth)		{ g_pVREyeDepth->Release();		g_pVREyeDepth = NULL; }
	if (g_pVRScreenSurf)	{ g_pVRScreenSurf->Release();	g_pVRScreenSurf = NULL; }
	for (int i = 0; i < LTVR_NUM_LAYERS; ++i)
	{
		if (g_aVRSpots[i].pSurf)
			g_aVRSpots[i].pSurf->Release();
		memset(&g_aVRSpots[i], 0, sizeof(g_aVRSpots[i]));
		g_aVRSpots[i].nLayer = -1;
	}
	g_nVRSpotDrawing = -1;
	VR_ReleaseCanvas();
	g_bVRBodyUsedValid = false;
	if (g_pVRBridge)
	{
		g_pVRBridge->Shutdown();
		delete g_pVRBridge;
		g_pVRBridge = NULL;
	}
	g_bVRFrameOpen = false;
	g_bVREyesDrawn = false;
	g_bVREyesJustDrawn = false;
	g_nVRScreenWidth = g_nVRScreenHeight = 0;
	memset(&g_VRState, 0, sizeof(g_VRState));
	ClipCursor(NULL);
}

bool d3d_VR_PreDeviceCreate(uint32 iAdapter)
{
	if (!d3d_VR_Requested())
		return false;

	if (!g_D3DShell.m_pD3DEx)
	{
		xrb::Log("VR disabled: Direct3D 9Ex is not available");
		return false;
	}

	if (!g_pVRBridge)
	{
		g_pVRBridge = new xrb::Bridge;
		if (!g_pVRBridge->InitInstance("No One Lives Forever 2"))
		{
			xrb::Log("VR disabled: could not connect to the OpenXR runtime");
			VR_Destroy();
			return false;
		}
	}

	int nHeadsetAdapter = g_pVRBridge->GetD3D9AdapterIndex(g_D3DShell.m_pD3DEx);
	if (nHeadsetAdapter != (int)iAdapter)
	{
		xrb::Log("VR disabled: the game is using display adapter %u but the headset is on adapter %d",
			iAdapter, nHeadsetAdapter);
		VR_Destroy();
		return false;
	}
	return true;
}

void d3d_VR_PostDeviceCreate(IDirect3DDevice9Ex* pDevice, uint32 nScreenWidth, uint32 nScreenHeight)
{
	if (!g_pVRBridge)
		return;

	if (!pDevice)
	{
		xrb::Log("VR disabled: the device is not a Direct3D 9Ex device");
		VR_Destroy();
		return;
	}

	xrb::ScreenDesc screen;
	screen.width			= nScreenWidth;
	screen.height			= nScreenHeight;
	screen.widthMeters		= 2.4f;		// a large TV, 2 m away
	screen.distanceMeters	= 2.0f;

	if (!g_pVRBridge->InitSession(pDevice, screen) ||
		FAILED(g_pVRBridge->GetScreenTexture()->GetSurfaceLevel(0, &g_pVRScreenSurf)))
	{
		xrb::Log("VR disabled: could not start the headset session");
		VR_Destroy();
		return;
	}

	// Eye render targets, and one depth buffer the size of an eye (the
	// game's own depth buffer is the size of the monitor picture). Same
	// depth format as the game's, so stencil effects behave the same.
	for (int e = 0; e < 2; ++e)
		g_pVRBridge->GetEyeTexture((xrb::Eye)e)->GetSurfaceLevel(0, &g_pVREyeSurf[e]);

	D3DFORMAT eDepthFormat = D3DFMT_D24S8;
	IDirect3DSurface9* pGameDepth = NULL;
	if (SUCCEEDED(pDevice->GetDepthStencilSurface(&pGameDepth)) && pGameDepth)
	{
		D3DSURFACE_DESC desc;
		if (SUCCEEDED(pGameDepth->GetDesc(&desc)))
			eDepthFormat = desc.Format;
		pGameDepth->Release();
	}
	HRESULT hr = pDevice->CreateDepthStencilSurface(g_pVRBridge->GetEyeWidth(), g_pVRBridge->GetEyeHeight(),
		eDepthFormat, D3DMULTISAMPLE_NONE, 0, TRUE, &g_pVREyeDepth, NULL);
	if (FAILED(hr) || !g_pVREyeSurf[0] || !g_pVREyeSurf[1])
	{
		xrb::Log("VR disabled: could not create the eye render targets (hr=0x%08X)", (unsigned)hr);
		VR_Destroy();
		return;
	}

	D3DSURFACE_DESC screenDesc;
	if (SUCCEEDED(g_pVRScreenSurf->GetDesc(&screenDesc)))
	{
		g_nVRScreenWidth = screenDesc.Width;
		g_nVRScreenHeight = screenDesc.Height;
	}

	// The spot layers (not fatal if one fails).
	for (int i = 0; i < LTVR_NUM_LAYERS; ++i)
	{
		VRSpot& spot = g_aVRSpots[i];
		spot.nLayer = g_pVRBridge->CreateLayer(kVRSpotWidth[i], kVRSpotHeight[i]);
		if (spot.nLayer >= 0 && g_pVRBridge->GetLayerTexture(spot.nLayer))
			g_pVRBridge->GetLayerTexture(spot.nLayer)->GetSurfaceLevel(0, &spot.pSurf);
	}

	// Put the world origin wherever the head is when tracking starts.
	g_pVRBridge->Recenter();

	xrb::Log("VR active (stereo), eyes %ux%u, monitor %ux%u", g_pVRBridge->GetEyeWidth(),
		g_pVRBridge->GetEyeHeight(), nScreenWidth, nScreenHeight);
}

void d3d_VR_OnModeChange(uint32 nScreenWidth, uint32 nScreenHeight)
{
	if (!d3d_VR_IsActive())
		return;
	if (nScreenWidth == g_nVRScreenWidth && nScreenHeight == g_nVRScreenHeight)
		return;

	VR_EndHud();
	VR_ReleaseCanvas();		// made again at the new size when next needed
	if (g_pVRScreenSurf)
	{
		g_pVRScreenSurf->Release();
		g_pVRScreenSurf = NULL;
	}
	g_nVRScreenWidth = g_nVRScreenHeight = 0;

	if (g_pVRBridge->ResizeScreen(nScreenWidth, nScreenHeight) && g_pVRBridge->GetScreenTexture() &&
		SUCCEEDED(g_pVRBridge->GetScreenTexture()->GetSurfaceLevel(0, &g_pVRScreenSurf)))
	{
		g_nVRScreenWidth = nScreenWidth;
		g_nVRScreenHeight = nScreenHeight;
	}
	xrb::Log("Game screen is now %ux%u", nScreenWidth, nScreenHeight);
}

void d3d_VR_Term()
{
	VR_Destroy();
}

bool d3d_VR_IsActive()
{
	return g_pVRBridge && g_pVRBridge->IsInitialized();
}

//------------------------------------------------------------------
// Frame pacing
//------------------------------------------------------------------

// Opens the headset frame if it isn't open yet (waits for the headset's
// frame slot and reads the latest poses).
static bool VR_EnsureFrameOpen()
{
	if (!d3d_VR_IsActive())
		return false;
	if (!g_bVRFrameOpen)
	{
		g_bVRFrameOpen = g_pVRBridge->BeginFrame();
		g_bVREyesDrawn = false;
		VR_BuildState();
	}
	return g_bVRFrameOpen;
}

//------------------------------------------------------------------
// Coordinate helpers
//------------------------------------------------------------------

// OpenXR position (metres) -> LithTech position (game units).
static LTVector XRToLTPos(const xrb::Vec3& v, float fUnitsPerMeter)
{
	return LTVector(v.x * fUnitsPerMeter, v.y * fUnitsPerMeter, -v.z * fUnitsPerMeter);
}

// OpenXR orientation -> LithTech orientation (mirror along Z).
static LTRotation XRToLTRot(const xrb::Quat& q)
{
	LTRotation r;
	r.m_Quat[0] = -q.x;
	r.m_Quat[1] = -q.y;
	r.m_Quat[2] =  q.z;
	r.m_Quat[3] =  q.w;
	return r;
}

// The heading of a rotation: a rotation about the vertical axis only.
static LTRotation YawOnly(const LTRotation& r)
{
	LTVector vForward = r.Forward();
	float fYaw = (float)atan2(vForward.x, vForward.z);
	return LTRotation(LTVector(0.0f, 1.0f, 0.0f), fYaw);
}

// Rotates a vector by a rotation (its basis vectors are the matrix columns).
static LTVector RotateVec(const LTRotation& r, const LTVector& v)
{
	return r.Right() * v.x + r.Up() * v.y + r.Forward() * v.z;
}

//------------------------------------------------------------------
// State for the game (ltvr.h)
//------------------------------------------------------------------

static float VR_UnitsPerMeter()
{
	return g_CV_VRUnitsPerMeter.m_Val > 1.0f ? g_CV_VRUnitsPerMeter.m_Val : 60.0f;
}

static void VR_ToLTPose(const xrb::Pose& in, bool bValid, float fUnitsPerMeter, LTVR_Pose& out)
{
	LTVector vPos = XRToLTPos(in.position, fUnitsPerMeter);
	LTRotation rRot = XRToLTRot(in.orientation);
	out.valid = bValid ? 1 : 0;
	out.pos[0] = vPos.x; out.pos[1] = vPos.y; out.pos[2] = vPos.z;
	for (int i = 0; i < 4; ++i)
		out.rot[i] = rRot.m_Quat[i];
}

// Thumbstick pushed firmly in a direction counts as a button press.
// Press at 0.7, release below 0.5, so it doesn't flicker at the edge.
static unsigned VR_StickBits(float x, float y, unsigned nPrevBits)
{
	unsigned nBits = 0;
	struct { float v; unsigned bit; } aDir[4] = {
		{  y, LTVR_BTN_STICK_UP }, { -y, LTVR_BTN_STICK_DOWN },
		{ -x, LTVR_BTN_STICK_LEFT }, { x, LTVR_BTN_STICK_RIGHT } };
	for (int i = 0; i < 4; ++i)
	{
		float fThreshold = (nPrevBits & aDir[i].bit) ? 0.5f : 0.7f;
		if (aDir[i].v > fThreshold)
			nBits |= aDir[i].bit;
	}
	return nBits;
}

// Rebuilds g_VRState from the bridge (after each BeginFrame).
static void VR_BuildState()
{
	LTVR_State prev = g_VRState;
	memset(&g_VRState, 0, sizeof(g_VRState));
	g_VRState.version = LTVR_VERSION;
	if (!d3d_VR_IsActive())
		return;

	const float fUnitsPerMeter = VR_UnitsPerMeter();
	g_VRState.active = 1;
	g_VRState.tracking = g_pVRBridge->HasValidViews() ? 1 : 0;
	g_VRState.unitsPerMeter = fUnitsPerMeter;
	VR_ToLTPose(g_pVRBridge->GetHeadPose(), g_VRState.tracking != 0, fUnitsPerMeter, g_VRState.head);

	for (int h = 0; h < 2; ++h)
	{
		const xrb::HandInput& in = g_pVRBridge->GetHand((xrb::Hand)h);
		LTVR_HandState& out = g_VRState.hand[h];
		out.active = in.active ? 1 : 0;
		VR_ToLTPose(in.grip, in.gripValid, fUnitsPerMeter, out.grip);
		VR_ToLTPose(in.aim, in.aimValid, fUnitsPerMeter, out.aim);
		out.trigger = in.trigger;
		out.squeeze = in.squeeze;
		out.stick[0] = in.stickX;
		out.stick[1] = in.stickY;
		unsigned nBits = 0;
		if (in.trigger > 0.5f)	nBits |= LTVR_BTN_TRIGGER;
		if (in.squeeze > 0.5f)	nBits |= LTVR_BTN_GRIP;
		if (in.stickClick)		nBits |= LTVR_BTN_STICK;
		if (in.primary)			nBits |= LTVR_BTN_PRIMARY;
		if (in.secondary)		nBits |= LTVR_BTN_SECONDARY;
		if (in.menu)			nBits |= LTVR_BTN_MENU;
		nBits |= VR_StickBits(in.stickX, in.stickY, prev.hand[h].buttons);
		out.buttons = nBits;
	}
}

void d3d_VR_ApplyCommands(uint8* pCommands, uint32 nCommands)
{
	if (!g_VRState.active || !pCommands)
		return;
	for (int i = 0; i < g_nVRBindings; ++i)
	{
		const VRBinding& b = g_aVRBindings[i];
		if (b.nCommand >= 0 && (uint32)b.nCommand < nCommands && b.nHand >= 0 && b.nHand < 2 &&
			(g_VRState.hand[b.nHand].buttons & b.nButton))
		{
			pCommands[b.nCommand] = 1;
		}
	}
}

// Exported to the game DLL (see ltvr.h).
extern "C" __declspec(dllexport) int LTVR_GetState(LTVR_State* pState)
{
	if (!pState)
		return 0;
	*pState = g_VRState;
	pState->version = LTVR_VERSION;
	return pState->active;
}

extern "C" __declspec(dllexport) void LTVR_SetOrigin(const float pos[3], float fYaw)
{
	if (!pos)
		return;
	g_vVROrigin.Init(pos[0], pos[1], pos[2]);
	g_fVROriginYaw = fYaw;
	g_bVROriginSet = true;
}

extern "C" __declspec(dllexport) void LTVR_SetBinding(int hand, unsigned button, int commandId)
{
	// Replace an existing binding of the same button.
	for (int i = 0; i < g_nVRBindings; ++i)
	{
		if (g_aVRBindings[i].nHand == hand && g_aVRBindings[i].nButton == button)
		{
			if (commandId < 0)
				g_aVRBindings[i] = g_aVRBindings[--g_nVRBindings];
			else
				g_aVRBindings[i].nCommand = commandId;
			return;
		}
	}
	if (commandId >= 0 && g_nVRBindings < (int)(sizeof(g_aVRBindings) / sizeof(g_aVRBindings[0])))
	{
		g_aVRBindings[g_nVRBindings].nHand = hand;
		g_aVRBindings[g_nVRBindings].nButton = button;
		g_aVRBindings[g_nVRBindings].nCommand = commandId;
		++g_nVRBindings;
	}
}

extern "C" __declspec(dllexport) void LTVR_ClearBindings(void)
{
	g_nVRBindings = 0;
}

extern "C" __declspec(dllexport) void LTVR_Recenter(void)
{
	if (g_pVRBridge)
		g_pVRBridge->Recenter();
	g_bVRHudYawValid = false;
}

//------------------------------------------------------------------
// Stereo rendering
//------------------------------------------------------------------

// Is this scene the frame's main first-person view of the world? Other
// scenes (render-to-texture cameras, small inset views, object-list
// renders for interface models) are left alone.
static bool IsMainWorldView(const SceneDesc* pDesc)
{
	if (pDesc->m_DrawMode != DRAWMODE_NORMAL)
		return false;
	if (CRenderTargetMgr::GetSingleton().GetCurrentRenderTargetHandle() != INVALID_RENDER_TARGET)
		return false;
	return pDesc->m_Rect.left <= 0 && pDesc->m_Rect.top <= 0 &&
		pDesc->m_Rect.right >= (int)g_ScreenWidth && pDesc->m_Rect.bottom >= (int)g_ScreenHeight;
}

void d3d_VR_RenderEyes(SceneDesc* pDesc)
{
	if (!pDesc || !d3d_VR_IsActive())
		return;
	if (!IsMainWorldView(pDesc))
		return;

	// The interface layer is still installed: the last frame never reached
	// the flip (it can happen as a level starts loading). Put the monitor
	// back and draw this frame's eyes afresh.
	if (g_bVRHudActive)
	{
		VR_EndHud();
		g_bVREyesDrawn = false;
	}

	if (g_bVREyesDrawn)
		return;
	if (!VR_EnsureFrameOpen() || !g_pVRBridge->HasValidViews())
		return;

	IDirect3DDevice9* pDevice = PD3DDEVICE->GetDevice();

	// Remember the game's render target and depth buffer.
	IDirect3DSurface9* pOldRT = NULL;
	IDirect3DSurface9* pOldDepth = NULL;
	pDevice->GetRenderTarget(0, &pOldRT);
	pDevice->GetDepthStencilSurface(&pOldDepth);

	const float fUnitsPerMeter = VR_UnitsPerMeter();
	const uint32 nEyeWidth  = g_pVRBridge->GetEyeWidth();
	const uint32 nEyeHeight = g_pVRBridge->GetEyeHeight();

	// The body: where the player's eyes are (head untracked) and which way
	// the body faces. The game provides this (LTVR_SetOrigin); if it
	// hasn't this frame, fall back to the camera's position and heading.
	const LTRotation rBody = g_bVROriginSet ? LTRotation(LTVector(0.0f, 1.0f, 0.0f), g_fVROriginYaw) : YawOnly(pDesc->m_Rotation);
	const LTVector vBodyPos = g_bVROriginSet ? g_vVROrigin : pDesc->m_Pos;
	g_vVRBodyPosUsed = vBodyPos;
	g_rVRBodyUsed = rBody;
	g_bVRBodyUsedValid = true;

	// Really close space sits around the game's camera, scaled to world
	// size (D3D row-vector form: rows are the scaled axes, then position).
	{
		const float s = g_fVRCloseScale;
		const LTVector vR = pDesc->m_Rotation.Right() * s;
		const LTVector vU = pDesc->m_Rotation.Up() * s;
		const LTVector vF = pDesc->m_Rotation.Forward() * s;
		D3DMATRIX& m = g_mVRCloseToWorld;
		m._11 = vR.x; m._12 = vR.y; m._13 = vR.z; m._14 = 0.0f;
		m._21 = vU.x; m._22 = vU.y; m._23 = vU.z; m._24 = 0.0f;
		m._31 = vF.x; m._32 = vF.y; m._33 = vF.z; m._34 = 0.0f;
		m._41 = pDesc->m_Pos.x; m._42 = pDesc->m_Pos.y; m._43 = pDesc->m_Pos.z; m._44 = 1.0f;
	}

	for (int e = 0; e < 2; ++e)
	{
		const xrb::EyeView& view = g_pVRBridge->GetEyeView((xrb::Eye)e);

		SceneDesc eyeDesc = *pDesc;
		eyeDesc.m_Rect.left		= 0;
		eyeDesc.m_Rect.top		= 0;
		eyeDesc.m_Rect.right	= (int)nEyeWidth;
		eyeDesc.m_Rect.bottom	= (int)nEyeHeight;
		eyeDesc.m_Pos			= vBodyPos + RotateVec(rBody, XRToLTPos(view.pose.position, fUnitsPerMeter));
		eyeDesc.m_Rotation		= rBody * XRToLTRot(view.pose.orientation);
		eyeDesc.m_FrameTime		= 0.0f;	// time moves once per frame, in the monitor pass

		// This eye's exact (off-centre) view window at distance 1.
		float fTanLeft  = (float)tan(view.fov.angleLeft);
		float fTanRight = (float)tan(view.fov.angleRight);
		float fTanUp    = (float)tan(view.fov.angleUp);
		float fTanDown  = (float)tan(view.fov.angleDown);
		ViewBoxDef viewBox;
		viewBox.m_COP.Init((fTanLeft + fTanRight) * 0.5f, (fTanUp + fTanDown) * 0.5f, 1.0f);
		viewBox.m_WindowSize[0] = (fTanRight - fTanLeft) * 0.5f;
		viewBox.m_WindowSize[1] = (fTanUp - fTanDown) * 0.5f;
		viewBox.m_NearZ = 0.0f;		// filled in by d3d_InitFrame
		viewBox.m_FarZ  = 0.0f;

		pDevice->SetRenderTarget(0, g_pVREyeSurf[e]);
		pDevice->SetDepthStencilSurface(g_pVREyeDepth);
		g_Device.InvalidateViewport();
		pDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

		g_pVRViewBoxOverride = &viewBox;
		if (d3d_InitFrame(&g_ViewParams, &eyeDesc))
		{
			g_pVRReallyCloseToWorld = &g_mVRCloseToWorld;
			d3d_FullDrawScene(g_ViewParams, &eyeDesc);
			g_pVRReallyCloseToWorld = NULL;
		}
		g_pVRViewBoxOverride = NULL;
	}

	// Back to the game's own targets for the monitor pass.
	pDevice->SetRenderTarget(0, pOldRT);
	pDevice->SetDepthStencilSurface(pOldDepth);
	g_Device.InvalidateViewport();
	if (pOldRT)		pOldRT->Release();
	if (pOldDepth)	pOldDepth->Release();

	g_bVREyesDrawn = true;
	g_bVREyesJustDrawn = true;
}

//------------------------------------------------------------------
// Interface layer
//------------------------------------------------------------------

enum VRQuadBlend
{
	VRBLEND_ALPHA,			// ordinary see-through colour
	VRBLEND_PREMULTIPLIED,	// a premultiplied picture (the interface layer)
	VRBLEND_DARKEN,			// scales what's there by (1 - alpha), colour and alpha alike
};

struct VRQuadVert
{
	float		x, y, z, rhw;
	D3DCOLOR	color;
	float		u, v;
};

// Draws one rectangle over the whole of the current render target
// (nWidth x nHeight): pTex, or the flat colour if pTex is NULL. Every
// device setting is put back afterwards, so this can be called in the
// middle of the game's own drawing.
static void VR_DrawFullQuad(IDirect3DTexture9* pTex, D3DCOLOR color, uint32 nWidth, uint32 nHeight, VRQuadBlend eBlend)
{
	if (!PD3DDEVICE || !nWidth || !nHeight)
		return;
	IDirect3DDevice9* pDevice = PD3DDEVICE->GetDevice();

	IDirect3DStateBlock9* pSaved = NULL;
	if (FAILED(pDevice->CreateStateBlock(D3DSBT_ALL, &pSaved)) || !pSaved)
		return;

	bool bOwnScene = !g_Device.IsIn3D();
	if (bOwnScene)
		pDevice->BeginScene();

	D3DVIEWPORT9 vp = { 0, 0, nWidth, nHeight, 0.0f, 1.0f };
	pDevice->SetViewport(&vp);
	pDevice->SetVertexShader(NULL);
	pDevice->SetPixelShader(NULL);
	pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);

	pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	pDevice->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0F);
	pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	pDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	pDevice->SetRenderState(D3DRS_SRCBLEND,
		eBlend == VRBLEND_ALPHA ? D3DBLEND_SRCALPHA : eBlend == VRBLEND_PREMULTIPLIED ? D3DBLEND_ONE : D3DBLEND_ZERO);
	pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	const DWORD dwArg = pTex ? D3DTA_TEXTURE : D3DTA_DIFFUSE;
	pDevice->SetTexture(0, pTex);
	pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, dwArg);
	pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, dwArg);
	pDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
	pDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	pDevice->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);

	// Pixel centres sit half a pixel in.
	const float fRight = (float)nWidth - 0.5f;
	const float fBottom = (float)nHeight - 0.5f;
	VRQuadVert aVerts[4] =
	{
		{ -0.5f,  -0.5f,   0.0f, 1.0f, color, 0.0f, 0.0f },
		{ fRight, -0.5f,   0.0f, 1.0f, color, 1.0f, 0.0f },
		{ -0.5f,  fBottom, 0.0f, 1.0f, color, 0.0f, 1.0f },
		{ fRight, fBottom, 0.0f, 1.0f, color, 1.0f, 1.0f },
	};
	pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, aVerts, sizeof(VRQuadVert));

	if (bOwnScene)
		pDevice->EndScene();

	pSaved->Apply();
	pSaved->Release();
	g_Device.InvalidateViewport();
}

// Switches the game's drawing over to the interface layer (right after
// the world has been drawn for the monitor).
static void VR_BeginHud()
{
	if (g_bVRHudActive || !g_pVRScreenSurf || !PD3DDEVICE)
		return;
	IDirect3DDevice9* pDevice = PD3DDEVICE->GetDevice();

	if (FAILED(pDevice->GetRenderTarget(0, &g_pVRHudSavedRT)) || !g_pVRHudSavedRT)
	{
		g_pVRHudSavedRT = NULL;
		return;
	}
	if (FAILED(pDevice->SetRenderTarget(0, g_pVRScreenSurf)))
	{
		g_pVRHudSavedRT->Release();
		g_pVRHudSavedRT = NULL;
		return;
	}
	g_Device.InvalidateViewport();
	pDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);

	// Build up coverage in alpha: new = src + old * (1 - src).
	pDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
	pDevice->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
	pDevice->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
	pDevice->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);

	g_bVRHudActive = true;
}

// Back to drawing on the monitor's back buffer.
static void VR_EndHud()
{
	if (!g_bVRHudActive)
		return;
	g_bVRHudActive = false;
	if (!PD3DDEVICE)
		return;
	IDirect3DDevice9* pDevice = PD3DDEVICE->GetDevice();
	pDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
	if (g_pVRHudSavedRT)
	{
		pDevice->SetRenderTarget(0, g_pVRHudSavedRT);
		g_pVRHudSavedRT->Release();
		g_pVRHudSavedRT = NULL;
	}
	g_Device.InvalidateViewport();
}

void d3d_VR_AfterRenderScene()
{
	if (!g_bVREyesJustDrawn)
		return;
	g_bVREyesJustDrawn = false;
	VR_BeginHud();
}

// Darkens both eye pictures by the frame's fade.
static void VR_FadeEyes()
{
	if (g_fVRFade <= 0.0f || !PD3DDEVICE)
		return;
	IDirect3DDevice9* pDevice = PD3DDEVICE->GetDevice();

	IDirect3DSurface9* pOldRT = NULL;
	IDirect3DSurface9* pOldDepth = NULL;
	pDevice->GetRenderTarget(0, &pOldRT);
	pDevice->GetDepthStencilSurface(&pOldDepth);
	pDevice->SetDepthStencilSurface(NULL);	// the game's is smaller than an eye

	const D3DCOLOR black = D3DCOLOR_ARGB((int)(g_fVRFade * 255.0f + 0.5f), 0, 0, 0);
	for (int e = 0; e < 2; ++e)
	{
		if (!g_pVREyeSurf[e])
			continue;
		pDevice->SetRenderTarget(0, g_pVREyeSurf[e]);
		VR_DrawFullQuad(NULL, black, g_pVRBridge->GetEyeWidth(), g_pVRBridge->GetEyeHeight(), VRBLEND_ALPHA);
	}

	pDevice->SetRenderTarget(0, pOldRT);
	pDevice->SetDepthStencilSurface(pOldDepth);
	g_Device.InvalidateViewport();
	if (pOldRT)		pOldRT->Release();
	if (pOldDepth)	pOldDepth->Release();
}

static float VR_WidthForAngle(float fDistance, float fAngleDegrees)
{
	if (fAngleDegrees < 10.0f)	fAngleDegrees = 10.0f;
	if (fAngleDegrees > 150.0f)	fAngleDegrees = 150.0f;
	return 2.0f * fDistance * (float)tan(fAngleDegrees * 0.5f * 3.14159265f / 180.0f);
}

static float VR_ClampDistance(float fDistance)
{
	return fDistance < 0.3f ? 0.3f : (fDistance > 20.0f ? 20.0f : fDistance);
}

// The head's heading in tracking space (OpenXR: 0 = facing -Z).
static float VR_HeadYaw()
{
	const xrb::Quat& q = g_pVRBridge->GetHeadPose().orientation;
	float fx = -(2.0f * (q.x * q.z + q.w * q.y));
	float fz = -(1.0f - 2.0f * (q.x * q.x + q.y * q.y));
	if (fx * fx + fz * fz < 1e-6f)
		return 0.0f;
	return (float)atan2(-fx, -fz);
}

// A spot fDistance metres from the head along heading fYaw, at eye
// height, standing upright and facing the head.
static xrb::Pose VR_PoseAtYaw(float fYaw, float fDistance)
{
	const xrb::Pose& head = g_pVRBridge->GetHeadPose();
	xrb::Pose p;
	p.orientation.x = 0.0f;
	p.orientation.y = (float)sin(fYaw * 0.5f);
	p.orientation.z = 0.0f;
	p.orientation.w = (float)cos(fYaw * 0.5f);
	p.position.x = head.position.x - (float)sin(fYaw) * fDistance;
	p.position.y = head.position.y;
	p.position.z = head.position.z - (float)cos(fYaw) * fDistance;
	return p;
}

// Lazy-follow HUD: stays at its heading while you look around, glides
// back in front once you've turned more than VRHudFollowAngle away.
static xrb::Pose VR_FollowHudPose(float fDistance)
{
	LARGE_INTEGER now, freq;
	QueryPerformanceCounter(&now);
	QueryPerformanceFrequency(&freq);
	float fDt = g_bVRHudYawValid ? (float)(now.QuadPart - g_VRLastSwapTime.QuadPart) / (float)freq.QuadPart : 0.0f;
	if (fDt < 0.0f || fDt > 0.25f)
		fDt = 0.0f;
	g_VRLastSwapTime = now;

	const float kPi = 3.14159265f;
	float fHeadYaw = VR_HeadYaw();
	if (!g_bVRHudYawValid)
	{
		g_fVRHudYaw = fHeadYaw;
		g_bVRHudYawValid = true;
	}

	float fDiff = fHeadYaw - g_fVRHudYaw;
	while (fDiff > kPi)		fDiff -= 2.0f * kPi;
	while (fDiff < -kPi)	fDiff += 2.0f * kPi;

	float fThreshold = g_CV_VRHudFollowAngle.m_Val * kPi / 180.0f;
	if (fThreshold < 0.0f)
		fThreshold = 0.0f;
	if (fabs(fDiff) > fThreshold)
		g_bVRHudGliding = true;

	if (g_bVRHudGliding)
	{
		// Ease toward straight ahead; stop once nearly there.
		float fStep = fDiff * (1.0f - (float)exp(-6.0f * fDt));
		g_fVRHudYaw += fStep;
		if (fabs(fDiff - fStep) < 2.0f * kPi / 180.0f)
			g_bVRHudGliding = false;
	}
	return VR_PoseAtYaw(g_fVRHudYaw, fDistance);
}

// A spot fDistance metres in front of the head, at eye height, facing the
// head (heading only, so it stands upright).
static xrb::Pose VR_PoseInFrontOfHead(float fDistance)
{
	const xrb::Pose& head = g_pVRBridge->GetHeadPose();
	const xrb::Quat& q = head.orientation;
	float fx = -(2.0f * (q.x * q.z + q.w * q.y));
	float fz = -(1.0f - 2.0f * (q.x * q.x + q.y * q.y));
	float fLen = (float)sqrt(fx * fx + fz * fz);
	if (fLen < 0.001f)
	{
		fx = 0.0f;
		fz = -1.0f;
	}
	else
	{
		fx /= fLen;
		fz /= fLen;
	}
	float fYaw = (float)atan2(-fx, -fz);

	xrb::Pose p;
	p.orientation.x = 0.0f;
	p.orientation.y = (float)sin(fYaw * 0.5f);
	p.orientation.z = 0.0f;
	p.orientation.w = (float)cos(fYaw * 0.5f);
	p.position.x = head.position.x + fx * fDistance;
	p.position.y = head.position.y;
	p.position.z = head.position.z + fz * fDistance;
	return p;
}

static xrb::Pose VR_IdentityPose()
{
	xrb::Pose p;
	p.orientation.x = p.orientation.y = p.orientation.z = 0.0f;
	p.orientation.w = 1.0f;
	p.position.x = p.position.y = p.position.z = 0.0f;
	return p;
}

// Exported to the game DLL (see ltvr.h).
extern "C" __declspec(dllexport) void LTVR_SetHudMode(int mode)
{
	if (mode != g_nVRHudMode)
	{
		g_nVRHudMode = mode;
		g_bVRPanelPlaced = false;	// a new panel appears where you're looking
		g_bVRHudYawValid = false;	// and so does the HUD when it comes back
	}
}

extern "C" __declspec(dllexport) void LTVR_SetFade(float fAlpha)
{
	if (!(fAlpha > 0.0f))	fAlpha = 0.0f;	// also catches NaN
	if (fAlpha > 1.0f)		fAlpha = 1.0f;
	g_fVRFade = fAlpha;

	// Like the game's own fade: darken what's been drawn so far.
	if (g_bVRHudActive && fAlpha > 0.0f)
	{
		VR_DrawFullQuad(NULL, D3DCOLOR_ARGB((int)(fAlpha * 255.0f + 0.5f), 0, 0, 0),
			g_nVRScreenWidth, g_nVRScreenHeight, VRBLEND_DARKEN);
	}
}

extern "C" __declspec(dllexport) int LTVR_IsHudFrame(void)
{
	return g_bVRHudActive ? 1 : 0;
}

extern "C" __declspec(dllexport) void LTVR_SetCloseScale(float fScale)
{
	g_fVRCloseScale = (fScale > 0.01f && fScale < 1000.0f) ? fScale : 1.0f;
}

extern "C" __declspec(dllexport) int LTVR_BeginLayer(int nLayer)
{
	if (nLayer < 0 || nLayer >= LTVR_NUM_LAYERS || !g_bVRHudActive || g_nVRSpotDrawing >= 0 || !PD3DDEVICE)
		return 0;
	VRSpot& spot = g_aVRSpots[nLayer];
	if (!spot.pSurf)
		return 0;
	IDirect3DDevice9* pDevice = PD3DDEVICE->GetDevice();
	if (FAILED(pDevice->SetRenderTarget(0, spot.pSurf)))
		return 0;
	g_Device.InvalidateViewport();
	if (!spot.bDrawn)
		pDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
	spot.bDrawn = true;
	g_nVRSpotDrawing = nLayer;
	g_bVRSpotFromCanvas = false;
	return (int)kVRSpotWidth[nLayer];
}

extern "C" __declspec(dllexport) int LTVR_BeginLayerFromScreen(int nLayer, const float rect[4])
{
	if (nLayer < 0 || nLayer >= LTVR_NUM_LAYERS || !rect || !g_bVRHudActive || g_nVRSpotDrawing >= 0 ||
		!PD3DDEVICE || !g_nVRScreenWidth || !g_nVRScreenHeight)
		return 0;
	VRSpot& spot = g_aVRSpots[nLayer];
	if (!spot.pSurf)
		return 0;

	// The part of the screen to copy, a little padded, kept on the screen.
	const float kPad = 4.0f;
	float l = rect[0] - kPad, t = rect[1] - kPad, r = rect[2] + kPad, b = rect[3] + kPad;
	if (l < 0.0f) l = 0.0f;
	if (t < 0.0f) t = 0.0f;
	if (r > (float)g_nVRScreenWidth)	r = (float)g_nVRScreenWidth;
	if (b > (float)g_nVRScreenHeight)	b = (float)g_nVRScreenHeight;
	if (!(r - l >= 2.0f && b - t >= 2.0f))	// also catches NaN
		return 0;

	IDirect3DDevice9* pDevice = PD3DDEVICE->GetDevice();
	if (g_pVRCanvas && (g_nVRCanvasWidth != g_nVRScreenWidth || g_nVRCanvasHeight != g_nVRScreenHeight))
		VR_ReleaseCanvas();
	if (!g_pVRCanvas)
	{
		HRESULT hr = pDevice->CreateRenderTarget(g_nVRScreenWidth, g_nVRScreenHeight, D3DFMT_A8R8G8B8,
			D3DMULTISAMPLE_NONE, 0, FALSE, &g_pVRCanvas, NULL);
		if (FAILED(hr) || !g_pVRCanvas)
		{
			static bool s_bLogged = false;
			if (!s_bLogged)
				xrb::Log("Could not make the HUD canvas (hr=0x%08X)", (unsigned)hr);
			s_bLogged = true;
			g_pVRCanvas = NULL;
			return 0;
		}
		g_nVRCanvasWidth = g_nVRScreenWidth;
		g_nVRCanvasHeight = g_nVRScreenHeight;
	}

	if (FAILED(pDevice->SetRenderTarget(0, g_pVRCanvas)))
		return 0;
	g_Device.InvalidateViewport();

	g_rcVRCanvasSrc.left	= (LONG)l;
	g_rcVRCanvasSrc.top		= (LONG)t;
	g_rcVRCanvasSrc.right	= (LONG)(r + 0.999f);
	g_rcVRCanvasSrc.bottom	= (LONG)(b + 0.999f);
	if (g_rcVRCanvasSrc.right > (LONG)g_nVRScreenWidth)		g_rcVRCanvasSrc.right = (LONG)g_nVRScreenWidth;
	if (g_rcVRCanvasSrc.bottom > (LONG)g_nVRScreenHeight)	g_rcVRCanvasSrc.bottom = (LONG)g_nVRScreenHeight;
	D3DRECT clear = { g_rcVRCanvasSrc.left, g_rcVRCanvasSrc.top, g_rcVRCanvasSrc.right, g_rcVRCanvasSrc.bottom };
	pDevice->Clear(1, &clear, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);

	g_nVRSpotDrawing = nLayer;
	g_bVRSpotFromCanvas = true;
	return 1;
}

extern "C" __declspec(dllexport) void LTVR_EndLayer(void)
{
	if (g_nVRSpotDrawing < 0 || !PD3DDEVICE)
		return;
	IDirect3DDevice9* pDevice = PD3DDEVICE->GetDevice();

	if (g_bVRSpotFromCanvas && g_pVRCanvas)
	{
		// Copy the drawn part of the canvas into the layer, as large as fits
		// without stretching it, centred, on a transparent background.
		VRSpot& spot = g_aVRSpots[g_nVRSpotDrawing];
		const float fSrcW = (float)(g_rcVRCanvasSrc.right - g_rcVRCanvasSrc.left);
		const float fSrcH = (float)(g_rcVRCanvasSrc.bottom - g_rcVRCanvasSrc.top);
		const float fDstW = (float)kVRSpotWidth[g_nVRSpotDrawing];
		const float fDstH = (float)kVRSpotHeight[g_nVRSpotDrawing];
		float fScale = fDstW / fSrcW;
		if (fSrcH * fScale > fDstH)
			fScale = fDstH / fSrcH;
		RECT dst;
		dst.left	= (LONG)((fDstW - fSrcW * fScale) * 0.5f);
		dst.top		= (LONG)((fDstH - fSrcH * fScale) * 0.5f);
		dst.right	= dst.left + (LONG)(fSrcW * fScale);
		dst.bottom	= dst.top + (LONG)(fSrcH * fScale);
		if (dst.right <= dst.left)	dst.right = dst.left + 1;
		if (dst.bottom <= dst.top)	dst.bottom = dst.top + 1;

		pDevice->ColorFill(spot.pSurf, NULL, D3DCOLOR_ARGB(0, 0, 0, 0));
		if (SUCCEEDED(pDevice->StretchRect(g_pVRCanvas, &g_rcVRCanvasSrc, spot.pSurf, &dst, D3DTEXF_LINEAR)))
			spot.bDrawn = true;
	}
	g_bVRSpotFromCanvas = false;
	g_nVRSpotDrawing = -1;
	pDevice->SetRenderTarget(0, g_pVRScreenSurf);
	g_Device.InvalidateViewport();
}

extern "C" __declspec(dllexport) void LTVR_SetLayer(int nLayer, int bVisible, const float pos[3], float fWidthMeters)
{
	if (nLayer < 0 || nLayer >= LTVR_NUM_LAYERS)
		return;
	VRSpot& spot = g_aVRSpots[nLayer];
	spot.bSet = bVisible && pos;
	if (spot.bSet)
		spot.vPos.Init(pos[0], pos[1], pos[2]);
	spot.fWidth = fWidthMeters > 0.0f ? fWidthMeters : 0.0f;
}

// The crosshair calls of version 3: layer 0.
extern "C" __declspec(dllexport) int LTVR_BeginCrosshair(void)
{
	return LTVR_BeginLayer(LTVR_LAYER_CROSSHAIR);
}

extern "C" __declspec(dllexport) void LTVR_EndCrosshair(void)
{
	LTVR_EndLayer();
}

extern "C" __declspec(dllexport) void LTVR_SetCrosshair(int bVisible, const float pos[3])
{
	LTVR_SetLayer(LTVR_LAYER_CROSSHAIR, bVisible, pos, 0.0f);
}

// Where a spot layer goes: at its point, facing the head; its size either
// fixed in metres or (crosshair) keeping the HUD's pixels per degree.
static bool VR_GetSpotPlacement(int nSpot, xrb::ScreenPlacement& place)
{
	const VRSpot& spot = g_aVRSpots[nSpot];
	if (spot.nLayer < 0 || !spot.bDrawn || !spot.bSet || !g_bVRBodyUsedValid || !g_nVRScreenWidth)
		return false;

	// World -> tracking space (undo the body transform), then to OpenXR.
	const float fUnitsPerMeter = VR_UnitsPerMeter();
	LTVector vRel = spot.vPos - g_vVRBodyPosUsed;
	xrb::Vec3 p;
	p.x =  vRel.Dot(g_rVRBodyUsed.Right()) / fUnitsPerMeter;
	p.y =  vRel.Dot(g_rVRBodyUsed.Up()) / fUnitsPerMeter;
	p.z = -vRel.Dot(g_rVRBodyUsed.Forward()) / fUnitsPerMeter;

	const xrb::Vec3& h = g_pVRBridge->GetHeadPose().position;
	float dx = h.x - p.x, dy = h.y - p.y, dz = h.z - p.z;
	float fDist = (float)sqrt(dx * dx + dy * dy + dz * dz);
	if (fDist < 0.05f)
		return false;
	dx /= fDist; dy /= fDist; dz /= fDist;

	// A crosshair too close to focus on comfortably slides out along the
	// line of sight (same look, further away).
	const float kMinDist = 0.5f;
	if (spot.fWidth <= 0.0f && fDist < kMinDist)
	{
		p.x = h.x - dx * kMinDist;
		p.y = h.y - dy * kMinDist;
		p.z = h.z - dz * kMinDist;
		fDist = kMinDist;
	}

	// Face the head: the quad's +Z points at it (yaw, then pitch; no roll,
	// so its X axis stays level).
	float fYaw = (float)atan2(dx, dz);
	float fPitch = -(float)asin(dy < -1.0f ? -1.0f : (dy > 1.0f ? 1.0f : dy));
	float cy = (float)cos(fYaw * 0.5f), sy = (float)sin(fYaw * 0.5f);
	float cp = (float)cos(fPitch * 0.5f), sp = (float)sin(fPitch * 0.5f);
	place.pose.orientation.x = cy * sp;
	place.pose.orientation.y = sy * cp;
	place.pose.orientation.z = -sy * sp;
	place.pose.orientation.w = cy * cp;
	place.pose.position = p;

	if (spot.fWidth > 0.0f)
	{
		place.widthMeters = spot.fWidth;
	}
	else
	{
		float fHudAngle = g_CV_VRHudAngle.m_Val;
		if (fHudAngle < 10.0f) fHudAngle = 10.0f;
		if (fHudAngle > 150.0f) fHudAngle = 150.0f;
		float fAngle = (float)kVRSpotWidth[nSpot] * fHudAngle / (float)g_nVRScreenWidth;
		place.widthMeters = VR_WidthForAngle(fDist, fAngle);
	}
	place.headLocked = false;
	place.alpha = true;
	return true;
}

//------------------------------------------------------------------
// Mouse
//------------------------------------------------------------------

// In the headset you can't see your monitors, so a mouse pointer that
// wanders off the game window onto another monitor is lost (and a click
// there takes focus away from the game). While the game window is in
// front, keep the pointer inside it.
static bool g_bVRCursorClipped = false;

static void VR_UpdateCursorClip()
{
	bool bClip = d3d_VR_IsActive() && g_hWnd && GetForegroundWindow() == g_hWnd;
	if (bClip)
	{
		RECT rc;
		POINT tl = { 0, 0 };
		GetClientRect(g_hWnd, &rc);
		ClientToScreen(g_hWnd, &tl);
		OffsetRect(&rc, tl.x, tl.y);
		ClipCursor(&rc);
		g_bVRCursorClipped = true;
	}
	else if (g_bVRCursorClipped)
	{
		ClipCursor(NULL);
		g_bVRCursorClipped = false;
	}
}

//------------------------------------------------------------------
// End of frame
//------------------------------------------------------------------

void d3d_VR_OnSwapBuffers()
{
	if (!d3d_VR_IsActive() || !PD3DDEVICE)
		return;

	const float fScreenDistance = VR_ClampDistance(g_CV_VRScreenDistance.m_Val);
	const float fScreenWidth = VR_WidthForAngle(fScreenDistance, g_CV_VRScreenAngle.m_Val);

	// Screens without a 3D world (menus etc.) never opened the frame.
	if (VR_EnsureFrameOpen())
	{
		if (g_bVREyesDrawn)
		{
			bool bHud = g_bVRHudActive;
			VR_EndHud();	// back on the monitor's back buffer
			VR_FadeEyes();

			// The monitor: the world, faded like the headset, with the
			// interface on top.
			if (g_fVRFade > 0.0f)
			{
				VR_DrawFullQuad(NULL, D3DCOLOR_ARGB((int)(g_fVRFade * 255.0f + 0.5f), 0, 0, 0),
					g_nVRScreenWidth, g_nVRScreenHeight, VRBLEND_ALPHA);
			}
			if (bHud)
			{
				VR_DrawFullQuad(g_pVRBridge->GetScreenTexture(), D3DCOLOR_ARGB(255, 255, 255, 255),
					g_nVRScreenWidth, g_nVRScreenHeight, VRBLEND_PREMULTIPLIED);
			}

			if (bHud)
			{
				xrb::ScreenPlacement place;
				place.alpha = true;
				if (g_nVRHudMode == LTVR_HUD_PANEL)
				{
					if (!g_bVRPanelPlaced)
					{
						g_VRPanelPose = VR_PoseInFrontOfHead(fScreenDistance);
						g_bVRPanelPlaced = true;
					}
					place.headLocked = false;
					place.pose = g_VRPanelPose;
					place.widthMeters = fScreenWidth;
				}
				else
				{
					const float fHudDistance = VR_ClampDistance(g_CV_VRHudDistance.m_Val);
					if (g_CV_VRHudFollow.m_Val)
					{
						place.headLocked = false;
						place.pose = VR_FollowHudPose(fHudDistance);
					}
					else
					{
						place.headLocked = true;
						place.pose = VR_IdentityPose();
						place.pose.position.z = -fHudDistance;
						g_bVRHudYawValid = false;
					}
					place.widthMeters = VR_WidthForAngle(fHudDistance, g_CV_VRHudAngle.m_Val);
				}
				xrb::LayerSubmit aSpots[LTVR_NUM_LAYERS];
				unsigned nSpots = 0;
				for (int i = 0; i < LTVR_NUM_LAYERS; ++i)
				{
					if (VR_GetSpotPlacement(i, aSpots[nSpots].place))
						aSpots[nSpots++].layer = g_aVRSpots[i].nLayer;
				}
				g_pVRBridge->EndFrame(true, &place, nSpots ? aSpots : NULL, nSpots);
			}
			else
			{
				g_pVRBridge->EndFrame(true, (const xrb::ScreenPlacement*)NULL);
			}
		}
		else
		{
			// No world this frame: show the monitor picture on the screen.
			IDirect3DDevice9* pDevice = PD3DDEVICE->GetDevice();
			IDirect3DSurface9* pBackBuffer = NULL;
			if (g_pVRScreenSurf && SUCCEEDED(pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer)))
			{
				pDevice->StretchRect(pBackBuffer, NULL, g_pVRScreenSurf, NULL, D3DTEXF_LINEAR);
				pBackBuffer->Release();
			}
			xrb::ScreenPlacement place;
			place.alpha = false;
			place.headLocked = false;
			place.pose = VR_IdentityPose();
			place.pose.position.z = -fScreenDistance;
			place.widthMeters = fScreenWidth;
			g_pVRBridge->EndFrame(false, &place);
		}
		g_bVRFrameOpen = false;
		g_bVREyesDrawn = false;
	}
	else
	{
		VR_EndHud();
	}
	g_bVREyesJustDrawn = false;
	g_fVRFade = 0.0f;
	for (int i = 0; i < LTVR_NUM_LAYERS; ++i)
	{
		g_aVRSpots[i].bDrawn = false;
		g_aVRSpots[i].bSet = false;
	}
	g_nVRSpotDrawing = -1;
	g_bVRSpotFromCanvas = false;
	g_bVRBodyUsedValid = false;

	// The game sets the origin again each frame it draws the world.
	g_bVROriginSet = false;

	VR_UpdateCursorClip();

	// Recentre on the Home key (stand-in until it's on a controller button;
	// the headset's own long-press recentre also works).
	static bool s_bRecenterKeyWasDown = false;
	bool bRecenterKeyDown = g_hWnd && GetForegroundWindow() == g_hWnd && (GetAsyncKeyState(VK_HOME) & 0x8000) != 0;
	if (bRecenterKeyDown && !s_bRecenterKeyWasDown)
	{
		g_pVRBridge->Recenter();
		g_bVRHudYawValid = false;
	}
	s_bRecenterKeyWasDown = bRecenterKeyDown;

	// Wait for the next frame slot now, so the next frame is simulated
	// and drawn with fresh head poses.
	VR_EnsureFrameOpen();
}
