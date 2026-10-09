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
//  floating flat screen. The HUD and the gun are not in the 3D view
//  yet; they come in later stages.
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
	for (int e = 0; e < 2; ++e)
	{
		if (g_pVREyeSurf[e]) { g_pVREyeSurf[e]->Release(); g_pVREyeSurf[e] = NULL; }
	}
	if (g_pVREyeDepth)		{ g_pVREyeDepth->Release();		g_pVREyeDepth = NULL; }
	if (g_pVRScreenSurf)	{ g_pVRScreenSurf->Release();	g_pVRScreenSurf = NULL; }
	if (g_pVRBridge)
	{
		g_pVRBridge->Shutdown();
		delete g_pVRBridge;
		g_pVRBridge = NULL;
	}
	g_bVRFrameOpen = false;
	g_bVREyesDrawn = false;
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

	// Put the world origin wherever the head is when tracking starts.
	g_pVRBridge->Recenter();

	xrb::Log("VR active (stereo), eyes %ux%u, monitor %ux%u", g_pVRBridge->GetEyeWidth(),
		g_pVRBridge->GetEyeHeight(), nScreenWidth, nScreenHeight);
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
	if (!pDesc || !d3d_VR_IsActive() || g_bVREyesDrawn)
		return;
	if (!IsMainWorldView(pDesc))
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
			// The gun is drawn with its own fixed projection, which can't
			// be right in stereo, so it's hidden in the headset until it is
			// held in your hand. (Set after d3d_InitFrame, which re-reads
			// the console variables; the monitor pass re-reads it again.)
			int nOldDrawGuns = g_CV_DrawGuns.m_Val;
			g_CV_DrawGuns.m_Val = 0;
			d3d_FullDrawScene(g_ViewParams, &eyeDesc);
			g_CV_DrawGuns.m_Val = nOldDrawGuns;
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

	// Screens without a 3D world (menus etc.) never opened the frame.
	if (VR_EnsureFrameOpen())
	{
		if (g_bVREyesDrawn)
		{
			g_pVRBridge->EndFrame(true, false);
		}
		else
		{
			// No world this frame: show the monitor picture on the screen.
			IDirect3DDevice9* pDevice = PD3DDEVICE->GetDevice();
			IDirect3DSurface9* pBackBuffer = NULL;
			if (SUCCEEDED(pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer)))
			{
				pDevice->StretchRect(pBackBuffer, NULL, g_pVRScreenSurf, NULL, D3DTEXF_LINEAR);
				pBackBuffer->Release();
			}
			g_pVRBridge->EndFrame(false, true);
		}
		g_bVRFrameOpen = false;
		g_bVREyesDrawn = false;
	}

	// The game sets the origin again each frame it draws the world.
	g_bVROriginSet = false;

	VR_UpdateCursorClip();

	// Recentre on the Home key (stand-in until it's on a controller button;
	// the headset's own long-press recentre also works).
	static bool s_bRecenterKeyWasDown = false;
	bool bRecenterKeyDown = g_hWnd && GetForegroundWindow() == g_hWnd && (GetAsyncKeyState(VK_HOME) & 0x8000) != 0;
	if (bRecenterKeyDown && !s_bRecenterKeyWasDown)
		g_pVRBridge->Recenter();
	s_bRecenterKeyWasDown = bRecenterKeyDown;

	// Wait for the next frame slot now, so the next frame is simulated
	// and drawn with fresh head poses.
	VR_EnsureFrameOpen();
}
