//------------------------------------------------------------------
//
//  MODULE  : D3D_VR.CPP
//
//  PURPOSE : Hooks between the Direct3D 9 renderer and the VR bridge.
//
//  CURRENT STAGE: "flat screen". The game renders exactly as it does
//  on a monitor, and each finished frame is shown on a large floating
//  screen in the headset. This proves the engine works on Direct3D 9Ex
//  and talks to the headset. True stereo rendering comes next.
//
//------------------------------------------------------------------

#include "precompile.h"

#include "d3d_vr.h"
#include "d3d_device.h"
#include "d3d_shell.h"
#include "xrbridge.h"

static xrb::Bridge*			g_pVRBridge		= NULL;
static IDirect3DSurface9*	g_pVRScreenSurf	= NULL;	// surface of the bridge's screen texture
static int					g_nVRRequested	= -1;	// -1 = not checked yet

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
	if (g_pVRScreenSurf)
	{
		g_pVRScreenSurf->Release();
		g_pVRScreenSurf = NULL;
	}
	if (g_pVRBridge)
	{
		g_pVRBridge->Shutdown();
		delete g_pVRBridge;
		g_pVRBridge = NULL;
	}
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

	xrb::Log("VR active (flat screen mode), %ux%u", nScreenWidth, nScreenHeight);
}

void d3d_VR_Term()
{
	VR_Destroy();
}

bool d3d_VR_IsActive()
{
	return g_pVRBridge && g_pVRBridge->IsInitialized();
}

void d3d_VR_OnSwapBuffers()
{
	if (!d3d_VR_IsActive() || !PD3DDEVICE)
		return;

	// Waits for the headset's next frame slot; this is what paces the game.
	if (!g_pVRBridge->BeginFrame())
		return;

	IDirect3DDevice9* pDevice = PD3DDEVICE->GetDevice();
	IDirect3DSurface9* pBackBuffer = NULL;
	if (SUCCEEDED(pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer)))
	{
		pDevice->StretchRect(pBackBuffer, NULL, g_pVRScreenSurf, NULL, D3DTEXF_LINEAR);
		pBackBuffer->Release();
	}

	g_pVRBridge->EndFrame(false, true);
}
