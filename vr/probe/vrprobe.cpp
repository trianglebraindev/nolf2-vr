//------------------------------------------------------------------
//
//  MODULE  : VRPROBE.CPP
//
//  PURPOSE : Stand-alone test of the D3D9Ex -> OpenXR bridge,
//            before any of it touches the game engine.
//
//  WHAT YOU SHOULD SEE
//  -------------------
//  In the headset: a floating screen 2 m in front of you, slowly
//  cycling colour, with a white square sliding left and right and a
//  white frame around the edge. The same picture appears in this
//  program's window on the desktop.
//
//  KEYS (with this window focused)
//    R    re-centre the screen in front of you
//    Esc  quit
//
//  A log is written to xrbridge.log next to this program.
//
//  TIMING
//  While it runs, the log gets the same "Hitch:" lines as the game (any
//  frame over 25 ms and 2.5 headset frames: where its time went) and a
//  "Perf:" line every 10 seconds. With nothing but this program feeding
//  the headset, it shows whether stutters come from the bridge and the
//  runtime, or from something in the game.
//
//------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <math.h>

#include "xrbridge.h"

static bool g_bQuit = false;
static bool g_bRecenter = false;

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) g_bQuit = true;
		if (wParam == 'R') g_bRecenter = true;
		return 0;
	case WM_CLOSE:
	case WM_DESTROY:
		g_bQuit = true;
		return 0;
	}
	return DefWindowProcA(hWnd, msg, wParam, lParam);
}

static void Fail(const char* pszMsg)
{
	xrb::Log("PROBE FAILED: %s", pszMsg);
	MessageBoxA(NULL, pszMsg, "VR probe", MB_OK | MB_ICONERROR);
}

// Draws the test picture into a render target using only Clear(),
// so there is nothing to go wrong apart from the plumbing itself.
static void DrawTestPicture(IDirect3DDevice9Ex* pDev, IDirect3DTexture9* pTex, float fTime)
{
	IDirect3DSurface9* pSurf = NULL;
	if (FAILED(pTex->GetSurfaceLevel(0, &pSurf)))
		return;
	D3DSURFACE_DESC desc;
	pSurf->GetDesc(&desc);
	pDev->SetRenderTarget(0, pSurf);

	int r = (int)(96 + 64 * sinf(fTime * 0.7f));
	int g = (int)(96 + 64 * sinf(fTime * 0.9f + 2.0f));
	int b = (int)(96 + 64 * sinf(fTime * 1.1f + 4.0f));
	pDev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(r, g, b), 1.0f, 0);

	const LONG W = (LONG)desc.Width, H = (LONG)desc.Height, T = 8;
	D3DRECT aFrame[4] = {
		{ 0, 0, W, T }, { 0, H - T, W, H }, { 0, 0, T, H }, { W - T, 0, W, H } };
	pDev->Clear(4, aFrame, D3DCLEAR_TARGET, D3DCOLOR_XRGB(255, 255, 255), 1.0f, 0);

	LONG nSize = H / 5;
	LONG x = (LONG)((W - nSize) * (0.5f + 0.5f * sinf(fTime)));
	D3DRECT square = { x, (H - nSize) / 2, x + nSize, (H + nSize) / 2 };
	pDev->Clear(1, &square, D3DCLEAR_TARGET, D3DCOLOR_XRGB(255, 255, 255), 1.0f, 0);

	pSurf->Release();
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
	xrb::Log("VR probe starting");

	xrb::Bridge bridge;
	if (!bridge.InitInstance("NOLF2 VR probe"))
	{
		Fail("Could not connect to OpenXR. Is Virtual Desktop streaming and VDXR "
			 "selected as the OpenXR runtime? Details in xrbridge.log.");
		return 1;
	}

	IDirect3D9Ex* pD3D = NULL;
	if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3D)))
	{
		Fail("Direct3DCreate9Ex failed.");
		return 1;
	}
	int nAdapter = bridge.GetD3D9AdapterIndex(pD3D);
	if (nAdapter < 0)
	{
		Fail("The headset's GPU was not found among the Direct3D 9 adapters.");
		return 1;
	}

	WNDCLASSA wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInst;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = "NOLF2VRProbe";
	RegisterClassA(&wc);
	RECT rc = { 0, 0, 1280, 720 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	HWND hWnd = CreateWindowA(wc.lpszClassName, "NOLF2 VR probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInst, NULL);

	D3DPRESENT_PARAMETERS pp;
	ZeroMemory(&pp, sizeof(pp));
	pp.Windowed = TRUE;
	pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	pp.BackBufferFormat = D3DFMT_X8R8G8B8;
	pp.BackBufferWidth = 1280;
	pp.BackBufferHeight = 720;
	pp.hDeviceWindow = hWnd;
	pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;	// the headset paces us, not the monitor

	IDirect3DDevice9Ex* pDev = NULL;
	if (FAILED(pD3D->CreateDeviceEx(nAdapter, D3DDEVTYPE_HAL, hWnd,
		D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &pp, NULL, &pDev)))
	{
		Fail("CreateDeviceEx failed.");
		return 1;
	}

	xrb::ScreenDesc screen;
	screen.width = 1280;
	screen.height = 720;
	screen.widthMeters = 2.0f;
	screen.distanceMeters = 2.0f;
	if (!bridge.InitSession(pDev, screen))
	{
		Fail("Could not start the OpenXR session. Details in xrbridge.log.");
		return 1;
	}

	IDirect3DSurface9* pBackBuffer = NULL;
	pDev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
	IDirect3DSurface9* pScreenSurf = NULL;
	bridge.GetScreenTexture()->GetSurfaceLevel(0, &pScreenSurf);

	LARGE_INTEGER freq, start, now;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start);
	unsigned nFrames = 0;

	// Timing (see the top of this file).
	LARGE_INTEGER lastEnd, windowStart, t0, t1;
	bool bHaveLast = false;
	windowStart.QuadPart = 0;
	int nWinFrames = 0, nWinHitches = 0;
	double fWinWorst = 0.0, fWinSum = 0.0;
	DWORD dwLastTitle = GetTickCount();

	while (!g_bQuit && !bridge.ShouldQuit())
	{
		MSG msg;
		while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
		if (g_bRecenter)
		{
			g_bRecenter = false;
			bridge.Recenter();
		}

		QueryPerformanceCounter(&t0);
		bool bXR = bridge.BeginFrame();

		QueryPerformanceCounter(&now);
		float fTime = (float)(now.QuadPart - start.QuadPart) / (float)freq.QuadPart;

		pDev->BeginScene();
		DrawTestPicture(pDev, bridge.GetScreenTexture(), fTime);
		pDev->EndScene();

		// Mirror to the desktop window.
		pDev->SetRenderTarget(0, pBackBuffer);
		pDev->StretchRect(pScreenSurf, NULL, pBackBuffer, NULL, D3DTEXF_LINEAR);
		pDev->PresentEx(NULL, NULL, NULL, NULL, 0);

		if (bXR)
		{
			QueryPerformanceCounter(&t1);
			bridge.EndFrame(false, true);
			++nFrames;

			LARGE_INTEGER tEnd;
			QueryPerformanceCounter(&tEnd);
			const double fMsPerTick = 1000.0 / (double)freq.QuadPart;
			if (bHaveLast)
			{
				double fFrame = (double)(tEnd.QuadPart - lastEnd.QuadPart) * fMsPerTick;
				double fWait = bridge.GetLastWaitMs();
				double fOwn = (double)(t1.QuadPart - t0.QuadPart) * fMsPerTick - fWait;	// BeginFrame's rest + drawing + Present
				double fEnd = (double)(tEnd.QuadPart - t1.QuadPart) * fMsPerTick;
				double fPeriod = bridge.GetDisplayPeriodMs();
				if (!windowStart.QuadPart)
					windowStart = tEnd;
				++nWinFrames;
				fWinSum += fFrame;
				if (fFrame > fWinWorst)
					fWinWorst = fFrame;
				if (fPeriod > 0.0 && fFrame > fPeriod * 2.5 && fFrame > 25.0)
				{
					++nWinHitches;
					double aSub[xrb::Bridge::SUBMIT_PARTS];
					bridge.GetLastSubmitTimes(aSub);
					xrb::Log("Hitch: %.1f ms = headset wait %.1f + probe %.1f + hand-over %.1f (other %.1f); xrEndFrame %.1f",
						fFrame, fWait, fOwn, fEnd, fFrame - fWait - fOwn - fEnd, aSub[xrb::Bridge::SUBMIT_END_FRAME]);
				}
				double fWindow = (double)(tEnd.QuadPart - windowStart.QuadPart) * fMsPerTick;
				if (fWindow >= 10000.0 && nWinFrames > 0)
				{
					xrb::Log("Perf: %.1f fps (headset %.0f Hz), frame %.1f ms avg / %.1f worst, hitches %d",
						(double)nWinFrames * 1000.0 / fWindow, fPeriod > 0.0 ? 1000.0 / fPeriod : 0.0,
						fWinSum / (double)nWinFrames, fWinWorst, nWinHitches);
					windowStart = tEnd;
					nWinFrames = nWinHitches = 0;
					fWinSum = fWinWorst = 0.0;
				}
			}
			lastEnd = tEnd;
			bHaveLast = true;
		}
		else
		{
			Sleep(10);	// headset not ready yet; don't spin
			bHaveLast = false;
		}

		if (GetTickCount() - dwLastTitle > 500)
		{
			dwLastTitle = GetTickCount();
			const xrb::Pose& head = bridge.GetHeadPose();
			char szTitle[256];
			_snprintf(szTitle, sizeof(szTitle),
				"NOLF2 VR probe  |  %s  |  headset frames: %u  |  head x %.2f  y %.2f  z %.2f  (R = recentre)",
				bridge.IsSessionRunning() ? "RUNNING" : "waiting for headset",
				nFrames, head.position.x, head.position.y, head.position.z);
			SetWindowTextA(hWnd, szTitle);
		}
	}

	xrb::Log("VR probe exiting after %u headset frames", nFrames);
	if (pScreenSurf) pScreenSurf->Release();
	if (pBackBuffer) pBackBuffer->Release();
	bridge.Shutdown();
	pDev->Release();
	pD3D->Release();
	return 0;
}
