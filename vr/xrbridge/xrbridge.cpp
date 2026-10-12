//------------------------------------------------------------------
//
//  MODULE  : XRBRIDGE.CPP
//
//  PURPOSE : Direct3D 9Ex -> Direct3D 11 -> OpenXR bridge.
//            See xrbridge.h for the overview.
//
//------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <float.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <vector>

#include "xrbridge.h"

namespace xrb
{

//------------------------------------------------------------------
// Logging
//------------------------------------------------------------------

void Log(const char* pszFormat, ...)
{
	char szBuf[1024];
	va_list args;
	va_start(args, pszFormat);
	_vsnprintf(szBuf, sizeof(szBuf) - 2, pszFormat, args);
	va_end(args);
	szBuf[sizeof(szBuf) - 2] = '\0';
	strcat(szBuf, "\n");

	OutputDebugStringA("[xrbridge] ");
	OutputDebugStringA(szBuf);

	static FILE* s_pFile = NULL;
	static bool s_bTried = false;
	if (!s_bTried)
	{
		s_bTried = true;
		s_pFile = fopen("xrbridge.log", "w");
	}
	if (s_pFile)
	{
		fputs(szBuf, s_pFile);
		fflush(s_pFile);
	}
}

#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p) = NULL; } } while (0)

//------------------------------------------------------------------
// Floating-point precision guard.
//
// LithTech lowers the x87 FPU to 24-bit precision every frame (an old
// speed trick, see d3d_SetFPState). OpenXR measures time in
// nanoseconds; values that large lose most of their digits at 24 bits,
// so a runtime doing its frame-timing maths on our thread can compute
// nonsense waits. Every entry point into the bridge therefore restores
// full (53-bit) precision for its duration, then puts the engine's
// setting back.
//------------------------------------------------------------------

struct FPUGuard
{
#if defined(_M_IX86)
	unsigned int m_nOld;
	FPUGuard()
	{
		unsigned int nTmp;
		_controlfp_s(&m_nOld, 0, 0);
		_controlfp_s(&nTmp, _PC_53, _MCW_PC);
	}
	~FPUGuard()
	{
		unsigned int nTmp;
		_controlfp_s(&nTmp, m_nOld & _MCW_PC, _MCW_PC);
	}
	bool WasReduced() const { return (m_nOld & _MCW_PC) != _PC_53; }
#else
	bool WasReduced() const { return false; }
#endif
};

//------------------------------------------------------------------
// Timing of calls that can block, so a stall shows up in the log.
//------------------------------------------------------------------

static double NowMs()
{
	static LARGE_INTEGER s_Freq = { 0 };
	if (!s_Freq.QuadPart)
		QueryPerformanceFrequency(&s_Freq);
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	return (double)now.QuadPart * 1000.0 / (double)s_Freq.QuadPart;
}

struct SlowCallLog
{
	const char*	m_pszWhat;
	double		m_fStart;
	bool		m_bAlways;
	SlowCallLog(const char* pszWhat, bool bAlways) : m_pszWhat(pszWhat), m_fStart(NowMs()), m_bAlways(bAlways) {}
	~SlowCallLog()
	{
		double fMs = NowMs() - m_fStart;
		if (m_bAlways || fMs > 500.0)
			Log("%s took %.1f ms%s", m_pszWhat, fMs, fMs > 500.0 ? "  <-- SLOW" : "");
	}
};

//------------------------------------------------------------------
// Copy shader: draws a source texture over the whole target.
//
// Compiled at runtime by d3dcompiler_47.dll, which is part of
// Windows 10/11. It is loaded dynamically on purpose: linking it
// statically could pick up the 2010 DirectX SDK's older compiler,
// which needs a DLL most machines no longer have.
//------------------------------------------------------------------

static const char g_szCopyShader[] =
	"Texture2D    g_Tex     : register(t0);\n"
	"SamplerState g_Sampler : register(s0);\n"
	"cbuffer Params : register(b0) { float4 g_vParams; };\n"	// x = linearize (0/1), y = keep alpha (0/1), zw = part of the source used (render scale)
	"struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };\n"
	"VSOut VSMain(uint id : SV_VertexID)\n"
	"{\n"
	"    VSOut o;\n"
	"    o.uv  = float2((id << 1) & 2, id & 2);\n"		// one triangle covering the screen
	"    o.pos = float4(o.uv * float2(2, -2) + float2(-1, 1), 0, 1);\n"
	"    return o;\n"
	"}\n"
	"float3 SRGBToLinear(float3 c)\n"
	"{\n"
	"    return (c <= 0.04045) ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);\n"
	"}\n"
	"float4 PSMain(VSOut i) : SV_Target\n"
	"{\n"
	"    float4 t = g_Tex.Sample(g_Sampler, i.uv * g_vParams.zw);\n"
	"    float3 c = t.rgb;\n"
	"    float  a = (g_vParams.y > 0.5) ? t.a : 1.0;\n"
	"    if (g_vParams.x > 0.5)\n"
	"    {\n"
	// Premultiplied colour has to be un-multiplied before the sRGB curve
	// and multiplied again after, or edges of the HUD go dark.
	"        if (g_vParams.y > 0.5) c = (a > 0.0001) ? SRGBToLinear(saturate(c / a)) * a : float3(0, 0, 0);\n"
	"        else c = SRGBToLinear(c);\n"
	"    }\n"
	"    return float4(c, a);\n"
	"}\n";

typedef HRESULT (WINAPI *PFN_D3DCompile)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*,
	ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);

//------------------------------------------------------------------
// One shared texture: a D3D9Ex render target, and the same memory
// opened as a D3D11 texture the copy shader can read.
//------------------------------------------------------------------

struct SharedTexture
{
	IDirect3DTexture9*			m_p9;
	ID3D11Texture2D*			m_p11;
	ID3D11ShaderResourceView*	m_pSRV;
	unsigned					m_nWidth, m_nHeight;

	SharedTexture() : m_p9(NULL), m_p11(NULL), m_pSRV(NULL), m_nWidth(0), m_nHeight(0) {}

	bool Create(IDirect3DDevice9Ex* pDev9, ID3D11Device* pDev11, unsigned nWidth, unsigned nHeight)
	{
		HANDLE hShared = NULL;
		HRESULT hr = pDev9->CreateTexture(nWidth, nHeight, 1, D3DUSAGE_RENDERTARGET,
			D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_p9, &hShared);
		if (FAILED(hr) || !hShared)
		{
			Log("ERROR: D3D9Ex shared texture %ux%u failed (hr=0x%08X)", nWidth, nHeight, (unsigned)hr);
			return false;
		}

		hr = pDev11->OpenSharedResource(hShared, __uuidof(ID3D11Texture2D), (void**)&m_p11);
		if (FAILED(hr))
		{
			Log("ERROR: D3D11 could not open the shared texture (hr=0x%08X). "
				"Are D3D9 and D3D11 on the same GPU?", (unsigned)hr);
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;	// D3DFMT_A8R8G8B8 in D3D11 terms
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		hr = pDev11->CreateShaderResourceView(m_p11, &srvDesc, &m_pSRV);
		if (FAILED(hr))
		{
			Log("ERROR: shared texture SRV failed (hr=0x%08X)", (unsigned)hr);
			return false;
		}

		m_nWidth = nWidth;
		m_nHeight = nHeight;
		return true;
	}

	void Release()
	{
		SAFE_RELEASE(m_pSRV);
		SAFE_RELEASE(m_p11);
		SAFE_RELEASE(m_p9);
	}
};

//------------------------------------------------------------------
// One OpenXR swapchain plus a render-target view for each image.
//------------------------------------------------------------------

struct Swapchain
{
	XrSwapchain							m_hSwapchain;
	unsigned							m_nWidth, m_nHeight;
	std::vector<ID3D11RenderTargetView*>	m_aRTV;
	bool								m_bLinearize;	// copy shader must convert sRGB -> linear

	Swapchain() : m_hSwapchain(XR_NULL_HANDLE), m_nWidth(0), m_nHeight(0), m_bLinearize(false) {}

	void Release()
	{
		for (size_t i = 0; i < m_aRTV.size(); ++i)
			SAFE_RELEASE(m_aRTV[i]);
		m_aRTV.clear();
		if (m_hSwapchain != XR_NULL_HANDLE)
		{
			xrDestroySwapchain(m_hSwapchain);
			m_hSwapchain = XR_NULL_HANDLE;
		}
	}
};

//------------------------------------------------------------------
// Implementation state
//------------------------------------------------------------------

struct Bridge::Impl
{
	// OpenXR
	XrInstance				m_hInstance;
	XrSystemId				m_SystemId;
	XrSession				m_hSession;
	XrSpace					m_hLocalSpace;		// runtime's LOCAL space
	XrSpace					m_hAppSpace;		// LOCAL, offset by the last recentre
	XrSpace					m_hViewSpace;		// the head
	XrSessionState			m_eSessionState;
	bool					m_bSessionRunning;
	bool					m_bQuit;
	bool					m_bRecenterPending;
	LUID					m_AdapterLuid;
	D3D_FEATURE_LEVEL		m_eMinFeatureLevel;
	int64_t					m_nSwapchainFormat;

	XrViewConfigurationView	m_aConfigViews[EYE_COUNT];
	XrView					m_aViews[EYE_COUNT];
	EyeView					m_aEyeViews[EYE_COUNT];
	Pose					m_HeadPose;
	XrFrameState			m_FrameState;
	bool					m_bViewsValid;

	// D3D
	IDirect3DDevice9Ex*		m_pDev9;
	IDirect3DQuery9*		m_pQuery9;
	ID3D11Device*			m_pDev11;
	ID3D11DeviceContext*	m_pCtx11;
	ID3D11Query*			m_pQuery11;
	ID3D11VertexShader*		m_pVS;
	ID3D11PixelShader*		m_pPS;
	ID3D11SamplerState*		m_pSampler;
	ID3D11Buffer*			m_pParams;
	ID3D11RasterizerState*	m_pRaster;

	SharedTexture			m_aEyeTex[EYE_COUNT];
	SharedTexture			m_ScreenTex;
	Swapchain				m_aEyeChain[EYE_COUNT];
	Swapchain				m_ScreenChain;
	ScreenDesc				m_Screen;

	struct Layer
	{
		SharedTexture		m_Tex;
		Swapchain			m_Chain;
	};
	std::vector<Layer*>		m_apLayers;

	// Controllers
	XrActionSet				m_hActionSet;
	XrAction				m_hGripPose, m_hAimPose;
	XrAction				m_hTrigger, m_hSqueeze, m_hStick;
	XrAction				m_hStickClick, m_hPrimary, m_hSecondary, m_hMenu;
	XrAction				m_hTriggerTouch, m_hThumbTouch;		// (finger sensing)
	XrPath					m_aHandPath[HAND_COUNT];
	XrSpace					m_aGripSpace[HAND_COUNT];
	XrSpace					m_aAimSpace[HAND_COUNT];
	HandInput				m_aHands[HAND_COUNT];

	Impl()
	{
		m_hInstance = XR_NULL_HANDLE;
		m_SystemId = XR_NULL_SYSTEM_ID;
		m_hSession = XR_NULL_HANDLE;
		m_hLocalSpace = m_hAppSpace = m_hViewSpace = XR_NULL_HANDLE;
		m_eSessionState = XR_SESSION_STATE_UNKNOWN;
		m_bSessionRunning = m_bQuit = m_bRecenterPending = false;
		m_fEyeScale = 1.0f;
		m_fLastWaitMs = 0.0;
		ZeroMemory(&m_AdapterLuid, sizeof(m_AdapterLuid));
		m_eMinFeatureLevel = D3D_FEATURE_LEVEL_11_0;
		m_nSwapchainFormat = 0;
		ZeroMemory(m_aConfigViews, sizeof(m_aConfigViews));
		ZeroMemory(m_aViews, sizeof(m_aViews));
		ZeroMemory(m_aEyeViews, sizeof(m_aEyeViews));
		ZeroMemory(&m_HeadPose, sizeof(m_HeadPose));
		m_HeadPose.orientation.w = 1.0f;
		ZeroMemory(&m_FrameState, sizeof(m_FrameState));
		m_bViewsValid = false;
		m_pDev9 = NULL; m_pQuery9 = NULL;
		m_pDev11 = NULL; m_pCtx11 = NULL; m_pQuery11 = NULL;
		m_pVS = NULL; m_pPS = NULL; m_pSampler = NULL; m_pParams = NULL; m_pRaster = NULL;
		ZeroMemory(&m_Screen, sizeof(m_Screen));
		m_hActionSet = XR_NULL_HANDLE;
		m_hGripPose = m_hAimPose = m_hTrigger = m_hSqueeze = m_hStick = XR_NULL_HANDLE;
		m_hStickClick = m_hPrimary = m_hSecondary = m_hMenu = XR_NULL_HANDLE;
		m_hTriggerTouch = m_hThumbTouch = XR_NULL_HANDLE;
		for (int h = 0; h < HAND_COUNT; ++h)
		{
			m_aHandPath[h] = XR_NULL_PATH;
			m_aGripSpace[h] = m_aAimSpace[h] = XR_NULL_HANDLE;
		}
		ZeroMemory(m_aHands, sizeof(m_aHands));
	}

	bool Check(XrResult res, const char* pszWhat)
	{
		if (XR_SUCCEEDED(res))
			return true;
		char szRes[XR_MAX_RESULT_STRING_SIZE] = "";
		if (m_hInstance != XR_NULL_HANDLE)
			xrResultToString(m_hInstance, res, szRes);
		Log("ERROR: %s failed: %s (%d)", pszWhat, szRes, (int)res);
		return false;
	}

	bool CreateD3D11();
	bool CreateCopyPipeline();
	bool CreateSwapchain(Swapchain& chain, unsigned nWidth, unsigned nHeight);
	bool CreateAppSpace(const XrPosef& offset);
	XrPath Path(const char* psz);
	XrAction CreateAction(const char* pszName, const char* pszLocalized, XrActionType eType, bool bBothHands);
	bool CreateInput();
	void UpdateInput();
	void DestroyInput();
	void PollEvents();
	void WaitForD3D9();
	void WaitForD3D11();
	void Blit(SharedTexture& src, Swapchain& dst, bool bAlpha, float fUScale = 1.0f, float fVScale = 1.0f);
	bool DoRecenter();

	// The eyes are drawn at this share of their textures' size (the
	// render resolution setting), from the top left; the copy into the
	// headset's images stretches that part over them.
	float		m_fEyeScale;
	double		m_fLastWaitMs;		// last xrWaitFrame (waiting on the headset)
};

static XrPosef IdentityPose()
{
	XrPosef p;
	p.orientation.x = p.orientation.y = p.orientation.z = 0.0f;
	p.orientation.w = 1.0f;
	p.position.x = p.position.y = p.position.z = 0.0f;
	return p;
}

static Pose ToPose(const XrPosef& p)
{
	Pose o;
	o.orientation.x = p.orientation.x; o.orientation.y = p.orientation.y;
	o.orientation.z = p.orientation.z; o.orientation.w = p.orientation.w;
	o.position.x = p.position.x; o.position.y = p.position.y; o.position.z = p.position.z;
	return o;
}

//------------------------------------------------------------------
// Bridge
//------------------------------------------------------------------

Bridge::Bridge() : m_pImpl(new Impl), m_bSessionCreated(false) {}

Bridge::~Bridge()
{
	Shutdown();
	delete m_pImpl;
	m_pImpl = NULL;
}

bool Bridge::InitInstance(const char* pszAppName)
{
	FPUGuard fpu;
	Impl& im = *m_pImpl;

	// Is there a runtime, and does it speak D3D11?
	uint32_t nExt = 0;
	XrResult res = xrEnumerateInstanceExtensionProperties(NULL, 0, &nExt, NULL);
	if (XR_FAILED(res))
	{
		Log("No OpenXR runtime found (%d). Is VDXR / SteamVR set as the active OpenXR runtime?", (int)res);
		return false;
	}
	std::vector<XrExtensionProperties> aExt(nExt);
	for (uint32_t i = 0; i < nExt; ++i)
	{
		aExt[i].type = XR_TYPE_EXTENSION_PROPERTIES;
		aExt[i].next = NULL;
	}
	xrEnumerateInstanceExtensionProperties(NULL, nExt, &nExt, aExt.data());
	bool bHasD3D11 = false;
	for (uint32_t i = 0; i < nExt; ++i)
		if (!strcmp(aExt[i].extensionName, XR_KHR_D3D11_ENABLE_EXTENSION_NAME))
			bHasD3D11 = true;
	if (!bHasD3D11)
	{
		Log("ERROR: the OpenXR runtime does not support Direct3D 11");
		return false;
	}

	const char* aEnabled[] = { XR_KHR_D3D11_ENABLE_EXTENSION_NAME };
	XrInstanceCreateInfo ci;
	ZeroMemory(&ci, sizeof(ci));
	ci.type = XR_TYPE_INSTANCE_CREATE_INFO;
	strncpy(ci.applicationInfo.applicationName, pszAppName ? pszAppName : "NOLF2 VR",
		XR_MAX_APPLICATION_NAME_SIZE - 1);
	ci.applicationInfo.applicationVersion = 1;
	strncpy(ci.applicationInfo.engineName, "LithTech Jupiter EX", XR_MAX_ENGINE_NAME_SIZE - 1);
	ci.applicationInfo.engineVersion = 1;
	ci.applicationInfo.apiVersion = XR_API_VERSION_1_0;
	ci.enabledExtensionCount = 1;
	ci.enabledExtensionNames = aEnabled;
	if (!im.Check(xrCreateInstance(&ci, &im.m_hInstance), "xrCreateInstance"))
		return false;

	XrInstanceProperties props;
	ZeroMemory(&props, sizeof(props));
	props.type = XR_TYPE_INSTANCE_PROPERTIES;
	if (XR_SUCCEEDED(xrGetInstanceProperties(im.m_hInstance, &props)))
	{
		Log("OpenXR runtime: %s %u.%u.%u", props.runtimeName,
			XR_VERSION_MAJOR(props.runtimeVersion), XR_VERSION_MINOR(props.runtimeVersion),
			XR_VERSION_PATCH(props.runtimeVersion));
	}

	XrSystemGetInfo sgi;
	ZeroMemory(&sgi, sizeof(sgi));
	sgi.type = XR_TYPE_SYSTEM_GET_INFO;
	sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	if (!im.Check(xrGetSystem(im.m_hInstance, &sgi, &im.m_SystemId), "xrGetSystem (is the headset connected?)"))
		return false;

	XrSystemProperties sysProps;
	ZeroMemory(&sysProps, sizeof(sysProps));
	sysProps.type = XR_TYPE_SYSTEM_PROPERTIES;
	if (XR_SUCCEEDED(xrGetSystemProperties(im.m_hInstance, im.m_SystemId, &sysProps)))
		Log("Headset: %s", sysProps.systemName);

	// Which GPU does the runtime want us on?
	PFN_xrGetD3D11GraphicsRequirementsKHR pfnReq = NULL;
	if (!im.Check(xrGetInstanceProcAddr(im.m_hInstance, "xrGetD3D11GraphicsRequirementsKHR",
		(PFN_xrVoidFunction*)&pfnReq), "xrGetInstanceProcAddr(xrGetD3D11GraphicsRequirementsKHR)"))
		return false;
	XrGraphicsRequirementsD3D11KHR req;
	ZeroMemory(&req, sizeof(req));
	req.type = XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR;
	if (!im.Check(pfnReq(im.m_hInstance, im.m_SystemId, &req), "xrGetD3D11GraphicsRequirementsKHR"))
		return false;
	im.m_AdapterLuid = req.adapterLuid;
	im.m_eMinFeatureLevel = req.minFeatureLevel;

	// Recommended per-eye render size.
	uint32_t nViews = 0;
	xrEnumerateViewConfigurationViews(im.m_hInstance, im.m_SystemId,
		XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &nViews, NULL);
	if (nViews != EYE_COUNT)
	{
		Log("ERROR: expected 2 views, runtime reports %u", nViews);
		return false;
	}
	for (int i = 0; i < EYE_COUNT; ++i)
	{
		im.m_aConfigViews[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
		im.m_aConfigViews[i].next = NULL;
	}
	if (!im.Check(xrEnumerateViewConfigurationViews(im.m_hInstance, im.m_SystemId,
		XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, nViews, &nViews, im.m_aConfigViews),
		"xrEnumerateViewConfigurationViews"))
		return false;
	Log("Recommended eye size: %ux%u",
		im.m_aConfigViews[0].recommendedImageRectWidth, im.m_aConfigViews[0].recommendedImageRectHeight);

	return true;
}

int Bridge::GetD3D9AdapterIndex(IDirect3D9Ex* pD3D9Ex) const
{
	const Impl& im = *m_pImpl;
	if (!pD3D9Ex)
		return -1;
	UINT nAdapters = pD3D9Ex->GetAdapterCount();
	for (UINT i = 0; i < nAdapters; ++i)
	{
		LUID luid;
		if (SUCCEEDED(pD3D9Ex->GetAdapterLUID(i, &luid)) &&
			luid.LowPart == im.m_AdapterLuid.LowPart && luid.HighPart == im.m_AdapterLuid.HighPart)
		{
			D3DADAPTER_IDENTIFIER9 id;
			if (SUCCEEDED(pD3D9Ex->GetAdapterIdentifier(i, 0, &id)))
				Log("Headset GPU is D3D9 adapter %u: %s", i, id.Description);
			return (int)i;
		}
	}
	Log("ERROR: no D3D9 adapter matches the headset's GPU");
	return -1;
}

bool Bridge::Impl::CreateD3D11()
{
	IDXGIFactory1* pFactory = NULL;
	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory)))
	{
		Log("ERROR: CreateDXGIFactory1 failed");
		return false;
	}

	IDXGIAdapter1* pAdapter = NULL;
	for (UINT i = 0; pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		DXGI_ADAPTER_DESC1 desc;
		pAdapter->GetDesc1(&desc);
		if (desc.AdapterLuid.LowPart == m_AdapterLuid.LowPart &&
			desc.AdapterLuid.HighPart == m_AdapterLuid.HighPart)
			break;
		SAFE_RELEASE(pAdapter);
	}
	SAFE_RELEASE(pFactory);
	if (!pAdapter)
	{
		Log("ERROR: no DXGI adapter matches the headset's GPU");
		return false;
	}

	D3D_FEATURE_LEVEL aLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
	HRESULT hr = D3D11CreateDevice(pAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0,
		aLevels, 2, D3D11_SDK_VERSION, &m_pDev11, NULL, &m_pCtx11);
	if (FAILED(hr))	// 11_1 is not known to every runtime; retry with 11_0 only
		hr = D3D11CreateDevice(pAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0,
			aLevels + 1, 1, D3D11_SDK_VERSION, &m_pDev11, NULL, &m_pCtx11);
	SAFE_RELEASE(pAdapter);
	if (FAILED(hr))
	{
		Log("ERROR: D3D11CreateDevice failed (hr=0x%08X)", (unsigned)hr);
		return false;
	}

	D3D11_QUERY_DESC qd = { D3D11_QUERY_EVENT, 0 };
	m_pDev11->CreateQuery(&qd, &m_pQuery11);
	return CreateCopyPipeline();
}

bool Bridge::Impl::CreateCopyPipeline()
{
	HMODULE hCompiler = LoadLibraryA("d3dcompiler_47.dll");
	PFN_D3DCompile pfnCompile = hCompiler ? (PFN_D3DCompile)GetProcAddress(hCompiler, "D3DCompile") : NULL;
	if (!pfnCompile)
	{
		Log("ERROR: d3dcompiler_47.dll not available");
		return false;
	}

	ID3DBlob* pVSBlob = NULL;
	ID3DBlob* pPSBlob = NULL;
	ID3DBlob* pErr = NULL;
	HRESULT hr = pfnCompile(g_szCopyShader, sizeof(g_szCopyShader) - 1, "xrbridge_copy", NULL, NULL,
		"VSMain", "vs_4_0", 0, 0, &pVSBlob, &pErr);
	if (FAILED(hr))
	{
		Log("ERROR: copy VS compile: %s", pErr ? (const char*)pErr->GetBufferPointer() : "?");
		SAFE_RELEASE(pErr);
		return false;
	}
	hr = pfnCompile(g_szCopyShader, sizeof(g_szCopyShader) - 1, "xrbridge_copy", NULL, NULL,
		"PSMain", "ps_4_0", 0, 0, &pPSBlob, &pErr);
	if (FAILED(hr))
	{
		Log("ERROR: copy PS compile: %s", pErr ? (const char*)pErr->GetBufferPointer() : "?");
		SAFE_RELEASE(pErr);
		SAFE_RELEASE(pVSBlob);
		return false;
	}

	m_pDev11->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &m_pVS);
	m_pDev11->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pPS);
	SAFE_RELEASE(pVSBlob);
	SAFE_RELEASE(pPSBlob);

	D3D11_SAMPLER_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.MaxLOD = D3D11_FLOAT32_MAX;
	m_pDev11->CreateSamplerState(&sd, &m_pSampler);

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.ByteWidth = 16;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	m_pDev11->CreateBuffer(&bd, NULL, &m_pParams);

	D3D11_RASTERIZER_DESC rd;
	ZeroMemory(&rd, sizeof(rd));
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_NONE;
	rd.DepthClipEnable = TRUE;
	m_pDev11->CreateRasterizerState(&rd, &m_pRaster);

	return m_pVS && m_pPS && m_pSampler && m_pParams && m_pRaster;
}

// Picks the swapchain format. The game's pixels are already
// gamma-encoded (sRGB), so an _SRGB swapchain is preferred: then the
// compositor reads them correctly with no conversion at all.
static int64_t ChooseFormat(const std::vector<int64_t>& aFormats)
{
	const int64_t aPreferred[] = {
		DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		DXGI_FORMAT_B8G8R8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
	};
	for (size_t p = 0; p < sizeof(aPreferred) / sizeof(aPreferred[0]); ++p)
		for (size_t i = 0; i < aFormats.size(); ++i)
			if (aFormats[i] == aPreferred[p])
				return aPreferred[p];
	return aFormats.empty() ? 0 : aFormats[0];
}

static bool IsSRGB(int64_t f)
{
	return f == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || f == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
}

static DXGI_FORMAT NonSRGB(int64_t f)
{
	if (f == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) return DXGI_FORMAT_B8G8R8A8_UNORM;
	if (f == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) return DXGI_FORMAT_R8G8B8A8_UNORM;
	return (DXGI_FORMAT)f;
}

bool Bridge::Impl::CreateSwapchain(Swapchain& chain, unsigned nWidth, unsigned nHeight)
{
	XrSwapchainCreateInfo sci;
	ZeroMemory(&sci, sizeof(sci));
	sci.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
	sci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
	sci.format = m_nSwapchainFormat;
	sci.sampleCount = 1;
	sci.width = nWidth;
	sci.height = nHeight;
	sci.faceCount = 1;
	sci.arraySize = 1;
	sci.mipCount = 1;
	if (!Check(xrCreateSwapchain(m_hSession, &sci, &chain.m_hSwapchain), "xrCreateSwapchain"))
		return false;
	chain.m_nWidth = nWidth;
	chain.m_nHeight = nHeight;

	uint32_t nImages = 0;
	xrEnumerateSwapchainImages(chain.m_hSwapchain, 0, &nImages, NULL);
	std::vector<XrSwapchainImageD3D11KHR> aImages(nImages);
	for (uint32_t i = 0; i < nImages; ++i)
	{
		aImages[i].type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
		aImages[i].next = NULL;
	}
	if (!Check(xrEnumerateSwapchainImages(chain.m_hSwapchain, nImages, &nImages,
		(XrSwapchainImageBaseHeader*)aImages.data()), "xrEnumerateSwapchainImages"))
		return false;

	// Preferred: an sRGB swapchain viewed as plain UNORM, so the
	// already-encoded pixels are copied through untouched. That only
	// works if the runtime made the textures "typeless"; otherwise view
	// them as sRGB and have the shader decode first (the GPU then
	// re-encodes on write, same result).
	chain.m_aRTV.resize(nImages, NULL);
	for (uint32_t i = 0; i < nImages; ++i)
	{
		D3D11_RENDER_TARGET_VIEW_DESC rtvd;
		ZeroMemory(&rtvd, sizeof(rtvd));
		rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvd.Format = NonSRGB(m_nSwapchainFormat);
		HRESULT hr = m_pDev11->CreateRenderTargetView(aImages[i].texture, &rtvd, &chain.m_aRTV[i]);
		bool bLinearize = !IsSRGB(m_nSwapchainFormat);	// linear swapchain: decode sRGB
		if (FAILED(hr) && IsSRGB(m_nSwapchainFormat))
		{
			rtvd.Format = (DXGI_FORMAT)m_nSwapchainFormat;
			hr = m_pDev11->CreateRenderTargetView(aImages[i].texture, &rtvd, &chain.m_aRTV[i]);
			bLinearize = true;
		}
		if (FAILED(hr))
		{
			Log("ERROR: swapchain render target view failed (hr=0x%08X)", (unsigned)hr);
			return false;
		}
		chain.m_bLinearize = bLinearize;
	}
	return true;
}

bool Bridge::Impl::CreateAppSpace(const XrPosef& offset)
{
	if (m_hAppSpace != XR_NULL_HANDLE)
	{
		xrDestroySpace(m_hAppSpace);
		m_hAppSpace = XR_NULL_HANDLE;
	}
	XrReferenceSpaceCreateInfo rci;
	ZeroMemory(&rci, sizeof(rci));
	rci.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	rci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	rci.poseInReferenceSpace = offset;
	return Check(xrCreateReferenceSpace(m_hSession, &rci, &m_hAppSpace), "xrCreateReferenceSpace(app)");
}

//------------------------------------------------------------------
// Controllers (OpenXR actions)
//------------------------------------------------------------------

XrPath Bridge::Impl::Path(const char* psz)
{
	XrPath path = XR_NULL_PATH;
	xrStringToPath(m_hInstance, psz, &path);
	return path;
}

XrAction Bridge::Impl::CreateAction(const char* pszName, const char* pszLocalized, XrActionType eType, bool bBothHands)
{
	XrActionCreateInfo ci;
	ZeroMemory(&ci, sizeof(ci));
	ci.type = XR_TYPE_ACTION_CREATE_INFO;
	strncpy(ci.actionName, pszName, XR_MAX_ACTION_NAME_SIZE - 1);
	strncpy(ci.localizedActionName, pszLocalized, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
	ci.actionType = eType;
	ci.countSubactionPaths = bBothHands ? HAND_COUNT : 1;
	ci.subactionPaths = bBothHands ? m_aHandPath : &m_aHandPath[HAND_LEFT];
	XrAction hAction = XR_NULL_HANDLE;
	Check(xrCreateAction(m_hActionSet, &ci, &hAction), pszName);
	return hAction;
}

bool Bridge::Impl::CreateInput()
{
	m_aHandPath[HAND_LEFT]  = Path("/user/hand/left");
	m_aHandPath[HAND_RIGHT] = Path("/user/hand/right");

	XrActionSetCreateInfo asci;
	ZeroMemory(&asci, sizeof(asci));
	asci.type = XR_TYPE_ACTION_SET_CREATE_INFO;
	strncpy(asci.actionSetName, "gameplay", XR_MAX_ACTION_SET_NAME_SIZE - 1);
	strncpy(asci.localizedActionSetName, "Gameplay", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE - 1);
	if (!Check(xrCreateActionSet(m_hInstance, &asci, &m_hActionSet), "xrCreateActionSet"))
		return false;

	m_hGripPose		= CreateAction("grip_pose",		"Hand pose",		XR_ACTION_TYPE_POSE_INPUT,		true);
	m_hAimPose		= CreateAction("aim_pose",		"Pointer pose",		XR_ACTION_TYPE_POSE_INPUT,		true);
	m_hTrigger		= CreateAction("trigger",		"Trigger",			XR_ACTION_TYPE_FLOAT_INPUT,		true);
	m_hSqueeze		= CreateAction("squeeze",		"Grip",				XR_ACTION_TYPE_FLOAT_INPUT,		true);
	m_hStick		= CreateAction("thumbstick",	"Thumbstick",		XR_ACTION_TYPE_VECTOR2F_INPUT,	true);
	m_hStickClick	= CreateAction("thumbstick_click", "Thumbstick click", XR_ACTION_TYPE_BOOLEAN_INPUT,	true);
	m_hPrimary		= CreateAction("primary",		"A / X button",		XR_ACTION_TYPE_BOOLEAN_INPUT,	true);
	m_hSecondary	= CreateAction("secondary",		"B / Y button",		XR_ACTION_TYPE_BOOLEAN_INPUT,	true);
	m_hMenu			= CreateAction("menu",			"Menu button",		XR_ACTION_TYPE_BOOLEAN_INPUT,	false);
	m_hTriggerTouch	= CreateAction("trigger_touch",	"Finger on trigger", XR_ACTION_TYPE_BOOLEAN_INPUT,	true);
	m_hThumbTouch	= CreateAction("thumb_touch",	"Thumb resting",	XR_ACTION_TYPE_BOOLEAN_INPUT,	true);

	// Meta Quest Touch controllers (what Virtual Desktop presents).
	struct { XrAction a; const char* psz; } aBind[] = {
		{ m_hGripPose,		"/user/hand/left/input/grip/pose" },
		{ m_hGripPose,		"/user/hand/right/input/grip/pose" },
		{ m_hAimPose,		"/user/hand/left/input/aim/pose" },
		{ m_hAimPose,		"/user/hand/right/input/aim/pose" },
		{ m_hTrigger,		"/user/hand/left/input/trigger/value" },
		{ m_hTrigger,		"/user/hand/right/input/trigger/value" },
		{ m_hSqueeze,		"/user/hand/left/input/squeeze/value" },
		{ m_hSqueeze,		"/user/hand/right/input/squeeze/value" },
		{ m_hStick,			"/user/hand/left/input/thumbstick" },
		{ m_hStick,			"/user/hand/right/input/thumbstick" },
		{ m_hStickClick,	"/user/hand/left/input/thumbstick/click" },
		{ m_hStickClick,	"/user/hand/right/input/thumbstick/click" },
		{ m_hPrimary,		"/user/hand/left/input/x/click" },
		{ m_hPrimary,		"/user/hand/right/input/a/click" },
		{ m_hSecondary,		"/user/hand/left/input/y/click" },
		{ m_hSecondary,		"/user/hand/right/input/b/click" },
		{ m_hMenu,			"/user/hand/left/input/menu/click" },
		// Finger sensing (capacitive), for the hands' index finger and
		// thumb. (Kept last: if the runtime refuses them, they're dropped
		// and the rest bound again.)
		{ m_hTriggerTouch,	"/user/hand/left/input/trigger/touch" },
		{ m_hTriggerTouch,	"/user/hand/right/input/trigger/touch" },
		{ m_hThumbTouch,	"/user/hand/left/input/thumbstick/touch" },
		{ m_hThumbTouch,	"/user/hand/right/input/thumbstick/touch" },
		{ m_hThumbTouch,	"/user/hand/left/input/x/touch" },
		{ m_hThumbTouch,	"/user/hand/left/input/y/touch" },
		{ m_hThumbTouch,	"/user/hand/right/input/a/touch" },
		{ m_hThumbTouch,	"/user/hand/right/input/b/touch" },
		{ m_hThumbTouch,	"/user/hand/left/input/thumbrest/touch" },
		{ m_hThumbTouch,	"/user/hand/right/input/thumbrest/touch" },
	};
	const uint32_t kTouchBindings = 10;
	const uint32_t nBind = sizeof(aBind) / sizeof(aBind[0]);
	XrActionSuggestedBinding aSuggested[sizeof(aBind) / sizeof(aBind[0])];
	for (uint32_t i = 0; i < nBind; ++i)
	{
		aSuggested[i].action = aBind[i].a;
		aSuggested[i].binding = Path(aBind[i].psz);
	}
	XrInteractionProfileSuggestedBinding sb;
	ZeroMemory(&sb, sizeof(sb));
	sb.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
	sb.interactionProfile = Path("/interaction_profiles/oculus/touch_controller");
	sb.countSuggestedBindings = nBind;
	sb.suggestedBindings = aSuggested;
	if (XR_FAILED(xrSuggestInteractionProfileBindings(m_hInstance, &sb)))
	{
		// Without the finger sensing, then.
		Log("Controller bindings with finger sensing refused; binding without it");
		sb.countSuggestedBindings = nBind - kTouchBindings;
		if (!Check(xrSuggestInteractionProfileBindings(m_hInstance, &sb), "xrSuggestInteractionProfileBindings(touch)"))
			return false;
	}

	XrSessionActionSetsAttachInfo ai;
	ZeroMemory(&ai, sizeof(ai));
	ai.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
	ai.countActionSets = 1;
	ai.actionSets = &m_hActionSet;
	if (!Check(xrAttachSessionActionSets(m_hSession, &ai), "xrAttachSessionActionSets"))
		return false;

	for (int h = 0; h < HAND_COUNT; ++h)
	{
		XrActionSpaceCreateInfo sci;
		ZeroMemory(&sci, sizeof(sci));
		sci.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
		sci.subactionPath = m_aHandPath[h];
		sci.poseInActionSpace = IdentityPose();
		sci.action = m_hGripPose;
		Check(xrCreateActionSpace(m_hSession, &sci, &m_aGripSpace[h]), "xrCreateActionSpace(grip)");
		sci.action = m_hAimPose;
		Check(xrCreateActionSpace(m_hSession, &sci, &m_aAimSpace[h]), "xrCreateActionSpace(aim)");
	}

	Log("Controllers set up (Touch controller bindings)");
	return true;
}

void Bridge::Impl::DestroyInput()
{
	for (int h = 0; h < HAND_COUNT; ++h)
	{
		if (m_aGripSpace[h] != XR_NULL_HANDLE) { xrDestroySpace(m_aGripSpace[h]); m_aGripSpace[h] = XR_NULL_HANDLE; }
		if (m_aAimSpace[h] != XR_NULL_HANDLE)  { xrDestroySpace(m_aAimSpace[h]);  m_aAimSpace[h] = XR_NULL_HANDLE; }
	}
	if (m_hActionSet != XR_NULL_HANDLE)
	{
		xrDestroyActionSet(m_hActionSet);	// also destroys its actions
		m_hActionSet = XR_NULL_HANDLE;
	}
}

static bool LocatePose(XrSpace hSpace, XrSpace hBase, XrTime time, Pose& out)
{
	if (hSpace == XR_NULL_HANDLE)
		return false;
	XrSpaceLocation loc;
	ZeroMemory(&loc, sizeof(loc));
	loc.type = XR_TYPE_SPACE_LOCATION;
	const XrSpaceLocationFlags kValid = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;
	if (XR_FAILED(xrLocateSpace(hSpace, hBase, time, &loc)) || (loc.locationFlags & kValid) != kValid)
		return false;
	out = ToPose(loc.pose);
	return true;
}

void Bridge::Impl::UpdateInput()
{
	if (m_hActionSet == XR_NULL_HANDLE)
		return;

	bool bFocused = (m_eSessionState == XR_SESSION_STATE_FOCUSED);
	if (bFocused)
	{
		XrActiveActionSet active;
		active.actionSet = m_hActionSet;
		active.subactionPath = XR_NULL_PATH;
		XrActionsSyncInfo si;
		ZeroMemory(&si, sizeof(si));
		si.type = XR_TYPE_ACTIONS_SYNC_INFO;
		si.countActiveActionSets = 1;
		si.activeActionSets = &active;
		xrSyncActions(m_hSession, &si);
	}

	for (int h = 0; h < HAND_COUNT; ++h)
	{
		HandInput& hand = m_aHands[h];
		HandInput prev = hand;
		ZeroMemory(&hand, sizeof(hand));
		hand.grip = prev.grip;	// keep the last known poses if tracking drops
		hand.aim = prev.aim;

		hand.gripValid = LocatePose(m_aGripSpace[h], m_hAppSpace, m_FrameState.predictedDisplayTime, hand.grip);
		hand.aimValid  = LocatePose(m_aAimSpace[h],  m_hAppSpace, m_FrameState.predictedDisplayTime, hand.aim);
		hand.active = hand.gripValid || hand.aimValid;

		if (!bFocused)
			continue;	// no buttons without input focus

		XrActionStateGetInfo gi;
		ZeroMemory(&gi, sizeof(gi));
		gi.type = XR_TYPE_ACTION_STATE_GET_INFO;
		gi.subactionPath = m_aHandPath[h];

		XrActionStateFloat fs;
		ZeroMemory(&fs, sizeof(fs));
		fs.type = XR_TYPE_ACTION_STATE_FLOAT;
		gi.action = m_hTrigger;
		if (XR_SUCCEEDED(xrGetActionStateFloat(m_hSession, &gi, &fs)) && fs.isActive) hand.trigger = fs.currentState;
		gi.action = m_hSqueeze;
		if (XR_SUCCEEDED(xrGetActionStateFloat(m_hSession, &gi, &fs)) && fs.isActive) hand.squeeze = fs.currentState;

		XrActionStateVector2f vs;
		ZeroMemory(&vs, sizeof(vs));
		vs.type = XR_TYPE_ACTION_STATE_VECTOR2F;
		gi.action = m_hStick;
		if (XR_SUCCEEDED(xrGetActionStateVector2f(m_hSession, &gi, &vs)) && vs.isActive)
		{
			hand.stickX = vs.currentState.x;
			hand.stickY = vs.currentState.y;
		}

		XrActionStateBoolean bs;
		ZeroMemory(&bs, sizeof(bs));
		bs.type = XR_TYPE_ACTION_STATE_BOOLEAN;
		gi.action = m_hStickClick;
		if (XR_SUCCEEDED(xrGetActionStateBoolean(m_hSession, &gi, &bs)) && bs.isActive) hand.stickClick = bs.currentState != 0;
		gi.action = m_hPrimary;
		if (XR_SUCCEEDED(xrGetActionStateBoolean(m_hSession, &gi, &bs)) && bs.isActive) hand.primary = bs.currentState != 0;
		gi.action = m_hSecondary;
		if (XR_SUCCEEDED(xrGetActionStateBoolean(m_hSession, &gi, &bs)) && bs.isActive) hand.secondary = bs.currentState != 0;
		if (h == HAND_LEFT)
		{
			gi.action = m_hMenu;
			if (XR_SUCCEEDED(xrGetActionStateBoolean(m_hSession, &gi, &bs)) && bs.isActive) hand.menu = bs.currentState != 0;
		}
		gi.action = m_hTriggerTouch;
		if (m_hTriggerTouch != XR_NULL_HANDLE && XR_SUCCEEDED(xrGetActionStateBoolean(m_hSession, &gi, &bs)) && bs.isActive)
		{
			hand.touchValid = true;
			hand.triggerTouch = bs.currentState != 0;
		}
		gi.action = m_hThumbTouch;
		if (m_hThumbTouch != XR_NULL_HANDLE && XR_SUCCEEDED(xrGetActionStateBoolean(m_hSession, &gi, &bs)) && bs.isActive)
			hand.thumbTouch = bs.currentState != 0;
	}
}

bool Bridge::InitSession(IDirect3DDevice9Ex* pDevice, const ScreenDesc& screen)
{
	FPUGuard fpu;
	Impl& im = *m_pImpl;
	if (im.m_hInstance == XR_NULL_HANDLE || !pDevice)
		return false;

	im.m_pDev9 = pDevice;
	im.m_pDev9->AddRef();
	im.m_pDev9->CreateQuery(D3DQUERYTYPE_EVENT, &im.m_pQuery9);
	im.m_Screen = screen;

	if (!im.CreateD3D11())
		return false;

	XrGraphicsBindingD3D11KHR binding;
	ZeroMemory(&binding, sizeof(binding));
	binding.type = XR_TYPE_GRAPHICS_BINDING_D3D11_KHR;
	binding.device = im.m_pDev11;

	XrSessionCreateInfo sci;
	ZeroMemory(&sci, sizeof(sci));
	sci.type = XR_TYPE_SESSION_CREATE_INFO;
	sci.next = &binding;
	sci.systemId = im.m_SystemId;
	if (!im.Check(xrCreateSession(im.m_hInstance, &sci, &im.m_hSession), "xrCreateSession"))
		return false;

	XrReferenceSpaceCreateInfo rci;
	ZeroMemory(&rci, sizeof(rci));
	rci.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	rci.poseInReferenceSpace = IdentityPose();
	rci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	if (!im.Check(xrCreateReferenceSpace(im.m_hSession, &rci, &im.m_hLocalSpace), "xrCreateReferenceSpace(LOCAL)"))
		return false;
	rci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
	if (!im.Check(xrCreateReferenceSpace(im.m_hSession, &rci, &im.m_hViewSpace), "xrCreateReferenceSpace(VIEW)"))
		return false;
	if (!im.CreateAppSpace(IdentityPose()))
		return false;

	// Controllers. Not fatal: the game still works with mouse and keyboard.
	if (!im.CreateInput())
		Log("WARNING: controller setup failed; continuing without controllers");

	// Swapchain format.
	uint32_t nFormats = 0;
	xrEnumerateSwapchainFormats(im.m_hSession, 0, &nFormats, NULL);
	std::vector<int64_t> aFormats(nFormats);
	xrEnumerateSwapchainFormats(im.m_hSession, nFormats, &nFormats, aFormats.data());
	im.m_nSwapchainFormat = ChooseFormat(aFormats);
	Log("Swapchain format: DXGI %d", (int)im.m_nSwapchainFormat);

	// Eyes.
	for (int e = 0; e < EYE_COUNT; ++e)
	{
		unsigned w = im.m_aConfigViews[e].recommendedImageRectWidth;
		unsigned h = im.m_aConfigViews[e].recommendedImageRectHeight;
		if (!im.m_aEyeTex[e].Create(im.m_pDev9, im.m_pDev11, w, h))
			return false;
		if (!im.CreateSwapchain(im.m_aEyeChain[e], w, h))
			return false;
	}

	// Floating screen.
	if (screen.width && screen.height)
	{
		if (!im.m_ScreenTex.Create(im.m_pDev9, im.m_pDev11, screen.width, screen.height))
			return false;
		if (!im.CreateSwapchain(im.m_ScreenChain, screen.width, screen.height))
			return false;
	}

	m_bSessionCreated = true;
	Log("Session created; waiting for the headset to start it");
	return true;
}

void Bridge::Shutdown()
{
	if (!m_pImpl)
		return;
	FPUGuard fpu;
	SlowCallLog slow("Shutdown", false);
	Impl& im = *m_pImpl;

	if (im.m_hSession != XR_NULL_HANDLE && im.m_bSessionRunning)
		xrEndSession(im.m_hSession);
	im.m_bSessionRunning = false;

	for (int e = 0; e < EYE_COUNT; ++e)
	{
		im.m_aEyeChain[e].Release();
		im.m_aEyeTex[e].Release();
	}
	im.m_ScreenChain.Release();
	im.m_ScreenTex.Release();
	for (size_t i = 0; i < im.m_apLayers.size(); ++i)
	{
		im.m_apLayers[i]->m_Chain.Release();
		im.m_apLayers[i]->m_Tex.Release();
		delete im.m_apLayers[i];
	}
	im.m_apLayers.clear();

	im.DestroyInput();
	if (im.m_hAppSpace != XR_NULL_HANDLE)   { xrDestroySpace(im.m_hAppSpace);   im.m_hAppSpace = XR_NULL_HANDLE; }
	if (im.m_hViewSpace != XR_NULL_HANDLE)  { xrDestroySpace(im.m_hViewSpace);  im.m_hViewSpace = XR_NULL_HANDLE; }
	if (im.m_hLocalSpace != XR_NULL_HANDLE) { xrDestroySpace(im.m_hLocalSpace); im.m_hLocalSpace = XR_NULL_HANDLE; }
	if (im.m_hSession != XR_NULL_HANDLE)    { xrDestroySession(im.m_hSession);  im.m_hSession = XR_NULL_HANDLE; }
	if (im.m_hInstance != XR_NULL_HANDLE)   { xrDestroyInstance(im.m_hInstance); im.m_hInstance = XR_NULL_HANDLE; }

	SAFE_RELEASE(im.m_pRaster);
	SAFE_RELEASE(im.m_pParams);
	SAFE_RELEASE(im.m_pSampler);
	SAFE_RELEASE(im.m_pPS);
	SAFE_RELEASE(im.m_pVS);
	SAFE_RELEASE(im.m_pQuery11);
	SAFE_RELEASE(im.m_pCtx11);
	SAFE_RELEASE(im.m_pDev11);
	SAFE_RELEASE(im.m_pQuery9);
	SAFE_RELEASE(im.m_pDev9);
	m_bSessionCreated = false;
}

void Bridge::Impl::PollEvents()
{
	XrEventDataBuffer ev;
	for (;;)
	{
		ZeroMemory(&ev, sizeof(ev));
		ev.type = XR_TYPE_EVENT_DATA_BUFFER;
		if (xrPollEvent(m_hInstance, &ev) != XR_SUCCESS)
			break;

		switch (ev.type)
		{
		case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
		{
			const XrEventDataSessionStateChanged& sc = *(const XrEventDataSessionStateChanged*)&ev;
			m_eSessionState = sc.state;
			Log("Session state -> %d", (int)sc.state);
			if (sc.state == XR_SESSION_STATE_READY)
			{
				XrSessionBeginInfo bi;
				ZeroMemory(&bi, sizeof(bi));
				bi.type = XR_TYPE_SESSION_BEGIN_INFO;
				bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
				if (Check(xrBeginSession(m_hSession, &bi), "xrBeginSession"))
					m_bSessionRunning = true;
			}
			else if (sc.state == XR_SESSION_STATE_STOPPING)
			{
				xrEndSession(m_hSession);
				m_bSessionRunning = false;
			}
			else if (sc.state == XR_SESSION_STATE_EXITING || sc.state == XR_SESSION_STATE_LOSS_PENDING)
			{
				m_bSessionRunning = false;
				m_bQuit = true;
			}
			break;
		}
		case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
			Log("OpenXR instance loss pending");
			m_bSessionRunning = false;
			m_bQuit = true;
			break;
		case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
			// The runtime moved its own origin (e.g. the headset's
			// long-press recentre). Ours is built on top of it, so redo it.
			Log("Reference space changed (runtime recentre); recentring");
			m_bRecenterPending = true;
			break;
		default:
			break;
		}
	}
}

bool Bridge::BeginFrame()
{
	if (!m_bSessionCreated)
		return false;
	FPUGuard fpu;
	Impl& im = *m_pImpl;

	static bool s_bReportedFPU = false;
	if (!s_bReportedFPU)
	{
		s_bReportedFPU = true;
		Log("FPU precision on entry: %s", fpu.WasReduced() ? "reduced (restored to 53-bit for OpenXR calls)" : "full");
	}

	im.PollEvents();
	if (!im.m_bSessionRunning)
		return false;

	// Log the first few frames' timings in full, then only stalls.
	static unsigned s_nFrame = 0;
	bool bVerbose = (++s_nFrame <= 3);
	char szWhat[64];
	_snprintf(szWhat, sizeof(szWhat), "frame %u xrWaitFrame", s_nFrame);
	SlowCallLog slowWait(szWhat, bVerbose);

	ZeroMemory(&im.m_FrameState, sizeof(im.m_FrameState));
	im.m_FrameState.type = XR_TYPE_FRAME_STATE;
	XrFrameWaitInfo wi;
	ZeroMemory(&wi, sizeof(wi));
	wi.type = XR_TYPE_FRAME_WAIT_INFO;
	const double fWaitStart = NowMs();
	if (!im.Check(xrWaitFrame(im.m_hSession, &wi, &im.m_FrameState), "xrWaitFrame"))
		return false;
	im.m_fLastWaitMs = NowMs() - fWaitStart;

	XrFrameBeginInfo bi;
	ZeroMemory(&bi, sizeof(bi));
	bi.type = XR_TYPE_FRAME_BEGIN_INFO;
	if (!im.Check(xrBeginFrame(im.m_hSession, &bi), "xrBeginFrame"))
		return false;

	// Retried every frame until the head is tracked.
	if (im.m_bRecenterPending)
		im.m_bRecenterPending = !im.DoRecenter();

	// Eye poses and fields of view for this frame.
	XrViewLocateInfo li;
	ZeroMemory(&li, sizeof(li));
	li.type = XR_TYPE_VIEW_LOCATE_INFO;
	li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	li.displayTime = im.m_FrameState.predictedDisplayTime;
	li.space = im.m_hAppSpace;
	XrViewState vs;
	ZeroMemory(&vs, sizeof(vs));
	vs.type = XR_TYPE_VIEW_STATE;
	for (int e = 0; e < EYE_COUNT; ++e)
	{
		im.m_aViews[e].type = XR_TYPE_VIEW;
		im.m_aViews[e].next = NULL;
	}
	uint32_t nViews = 0;
	im.m_bViewsValid = false;
	if (XR_SUCCEEDED(xrLocateViews(im.m_hSession, &li, &vs, EYE_COUNT, &nViews, im.m_aViews)) &&
		(vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) &&
		(vs.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT))
	{
		im.m_bViewsValid = true;
		for (int e = 0; e < EYE_COUNT; ++e)
		{
			im.m_aEyeViews[e].pose = ToPose(im.m_aViews[e].pose);
			im.m_aEyeViews[e].fov.angleLeft  = im.m_aViews[e].fov.angleLeft;
			im.m_aEyeViews[e].fov.angleRight = im.m_aViews[e].fov.angleRight;
			im.m_aEyeViews[e].fov.angleUp    = im.m_aViews[e].fov.angleUp;
			im.m_aEyeViews[e].fov.angleDown  = im.m_aViews[e].fov.angleDown;
		}
	}

	// Head pose.
	XrSpaceLocation loc;
	ZeroMemory(&loc, sizeof(loc));
	loc.type = XR_TYPE_SPACE_LOCATION;
	if (XR_SUCCEEDED(xrLocateSpace(im.m_hViewSpace, im.m_hAppSpace, im.m_FrameState.predictedDisplayTime, &loc)) &&
		(loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT))
	{
		im.m_HeadPose = ToPose(loc.pose);
	}

	im.UpdateInput();

	return true;
}

void Bridge::Impl::WaitForD3D9()
{
	// Wait until the GPU has finished everything D3D9 has queued so far,
	// including this frame's drawing into the shared textures.
	if (!m_pQuery9)
		return;
	m_pQuery9->Issue(D3DISSUE_END);
	while (m_pQuery9->GetData(NULL, 0, D3DGETDATA_FLUSH) == S_FALSE)
		SwitchToThread();
}

void Bridge::Impl::WaitForD3D11()
{
	// Make sure D3D11 is done reading the shared textures before D3D9
	// starts drawing the next frame into them.
	if (!m_pQuery11)
		return;
	m_pCtx11->End(m_pQuery11);
	while (m_pCtx11->GetData(m_pQuery11, NULL, 0, 0) == S_FALSE)
		SwitchToThread();
}

void Bridge::Impl::Blit(SharedTexture& src, Swapchain& dst, bool bAlpha, float fUScale, float fVScale)
{
	XrSwapchainImageAcquireInfo ai;
	ZeroMemory(&ai, sizeof(ai));
	ai.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
	uint32_t nIndex = 0;
	if (!Check(xrAcquireSwapchainImage(dst.m_hSwapchain, &ai, &nIndex), "xrAcquireSwapchainImage"))
		return;

	XrSwapchainImageWaitInfo wi;
	ZeroMemory(&wi, sizeof(wi));
	wi.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
	wi.timeout = 1000000000;	// 1 s: never hang the game on the headset
	XrResult resWait = xrWaitSwapchainImage(dst.m_hSwapchain, &wi);
	if (resWait == XR_TIMEOUT_EXPIRED)
		Log("xrWaitSwapchainImage timed out; skipping this image");
	if (resWait == XR_SUCCESS)
	{
		float aParams[4] = { dst.m_bLinearize ? 1.0f : 0.0f, bAlpha ? 1.0f : 0.0f, fUScale, fVScale };
		m_pCtx11->UpdateSubresource(m_pParams, 0, NULL, aParams, 0, 0);

		D3D11_VIEWPORT vp = { 0, 0, (float)dst.m_nWidth, (float)dst.m_nHeight, 0.0f, 1.0f };
		ID3D11RenderTargetView* pRTV = dst.m_aRTV[nIndex];
		m_pCtx11->OMSetRenderTargets(1, &pRTV, NULL);
		m_pCtx11->OMSetBlendState(NULL, NULL, 0xFFFFFFFF);
		m_pCtx11->OMSetDepthStencilState(NULL, 0);
		m_pCtx11->RSSetViewports(1, &vp);
		m_pCtx11->RSSetState(m_pRaster);
		m_pCtx11->IASetInputLayout(NULL);
		m_pCtx11->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pCtx11->VSSetShader(m_pVS, NULL, 0);
		m_pCtx11->PSSetShader(m_pPS, NULL, 0);
		m_pCtx11->PSSetShaderResources(0, 1, &src.m_pSRV);
		m_pCtx11->PSSetSamplers(0, 1, &m_pSampler);
		m_pCtx11->PSSetConstantBuffers(0, 1, &m_pParams);
		m_pCtx11->Draw(3, 0);

		ID3D11ShaderResourceView* pNullSRV = NULL;
		m_pCtx11->PSSetShaderResources(0, 1, &pNullSRV);
		ID3D11RenderTargetView* pNullRTV = NULL;
		m_pCtx11->OMSetRenderTargets(1, &pNullRTV, NULL);
	}

	XrSwapchainImageReleaseInfo ri;
	ZeroMemory(&ri, sizeof(ri));
	ri.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
	Check(xrReleaseSwapchainImage(dst.m_hSwapchain, &ri), "xrReleaseSwapchainImage");
}

void Bridge::EndFrame(bool bSubmitEyes, bool bSubmitScreen)
{
	if (!m_pImpl)
		return;
	ScreenPlacement place;
	place.headLocked = false;
	place.pose = ToPose(IdentityPose());
	place.pose.position.z = -m_pImpl->m_Screen.distanceMeters;
	place.widthMeters = m_pImpl->m_Screen.widthMeters;
	place.alpha = false;
	EndFrame(bSubmitEyes, bSubmitScreen ? &place : NULL);
}

void Bridge::EndFrame(bool bSubmitEyes, const ScreenPlacement* pScreen)
{
	EndFrame(bSubmitEyes, pScreen, NULL, 0);
}

int Bridge::CreateLayer(unsigned nWidth, unsigned nHeight)
{
	if (!m_bSessionCreated || !nWidth || !nHeight)
		return -1;
	FPUGuard fpu;
	Impl& im = *m_pImpl;
	Impl::Layer* pLayer = new Impl::Layer;
	if (!pLayer->m_Tex.Create(im.m_pDev9, im.m_pDev11, nWidth, nHeight) ||
		!im.CreateSwapchain(pLayer->m_Chain, nWidth, nHeight))
	{
		Log("ERROR: could not create a %ux%u layer", nWidth, nHeight);
		pLayer->m_Chain.Release();
		pLayer->m_Tex.Release();
		delete pLayer;
		return -1;
	}
	im.m_apLayers.push_back(pLayer);
	return (int)im.m_apLayers.size() - 1;
}

IDirect3DTexture9* Bridge::GetLayerTexture(int nLayer) const
{
	if (nLayer < 0 || nLayer >= (int)m_pImpl->m_apLayers.size())
		return NULL;
	return m_pImpl->m_apLayers[nLayer]->m_Tex.m_p9;
}

// Fills an OpenXR quad layer from a placement.
static void FillQuad(XrCompositionLayerQuad& quad, const ScreenPlacement& place, XrSpace hHeadSpace,
	XrSpace hAppSpace, XrSwapchain hSwapchain, unsigned nWidth, unsigned nHeight)
{
	ZeroMemory(&quad, sizeof(quad));
	quad.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
	quad.space = place.headLocked ? hHeadSpace : hAppSpace;
	quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
	if (place.alpha)
		quad.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;	// premultiplied
	quad.subImage.swapchain = hSwapchain;
	quad.subImage.imageRect.extent.width = (int32_t)nWidth;
	quad.subImage.imageRect.extent.height = (int32_t)nHeight;
	const Pose& p = place.pose;
	quad.pose.orientation.x = p.orientation.x;
	quad.pose.orientation.y = p.orientation.y;
	quad.pose.orientation.z = p.orientation.z;
	quad.pose.orientation.w = p.orientation.w;
	quad.pose.position.x = p.position.x;
	quad.pose.position.y = p.position.y;
	quad.pose.position.z = p.position.z;
	quad.size.width = place.widthMeters;
	quad.size.height = place.widthMeters * (float)nHeight / (float)nWidth;
}

void Bridge::EndFrame(bool bSubmitEyes, const ScreenPlacement* pScreen, const LayerSubmit* pLayers, unsigned nLayersIn)
{
	if (!m_bSessionCreated || !m_pImpl->m_bSessionRunning)
		return;
	FPUGuard fpu;
	SlowCallLog slow("EndFrame", false);
	Impl& im = *m_pImpl;

	const unsigned kMaxExtra = 8;
	bool bEyes = bSubmitEyes && im.m_bViewsValid && im.m_FrameState.shouldRender;
	bool bScreen = pScreen && im.m_ScreenChain.m_hSwapchain != XR_NULL_HANDLE && im.m_FrameState.shouldRender;

	// Extra layers that exist and can be shown.
	const LayerSubmit* apExtra[kMaxExtra];
	unsigned nExtra = 0;
	for (unsigned i = 0; pLayers && i < nLayersIn && nExtra < kMaxExtra && im.m_FrameState.shouldRender; ++i)
	{
		int n = pLayers[i].layer;
		if (n >= 0 && n < (int)im.m_apLayers.size() && im.m_apLayers[n]->m_Chain.m_hSwapchain != XR_NULL_HANDLE)
			apExtra[nExtra++] = &pLayers[i];
	}

	if (bEyes || bScreen || nExtra)
	{
		im.WaitForD3D9();
		if (bEyes)
		{
			for (int e = 0; e < EYE_COUNT; ++e)
			{
				// (Only the part drawn at the render resolution.)
				SharedTexture& tex = im.m_aEyeTex[e];
				float fU = tex.m_nWidth ? (float)GetEyeWidth() / (float)tex.m_nWidth : 1.0f;
				float fV = tex.m_nHeight ? (float)GetEyeHeight() / (float)tex.m_nHeight : 1.0f;
				im.Blit(tex, im.m_aEyeChain[e], false, fU, fV);
			}
		}
		for (unsigned i = 0; i < nExtra; ++i)
		{
			Impl::Layer& layer = *im.m_apLayers[apExtra[i]->layer];
			im.Blit(layer.m_Tex, layer.m_Chain, apExtra[i]->place.alpha);
		}
		if (bScreen)
			im.Blit(im.m_ScreenTex, im.m_ScreenChain, pScreen->alpha);
		im.WaitForD3D11();
	}

	XrCompositionLayerProjectionView aProjViews[EYE_COUNT];
	XrCompositionLayerProjection layerProj;
	XrCompositionLayerQuad aExtraQuads[kMaxExtra];
	XrCompositionLayerQuad layerQuad;
	const XrCompositionLayerBaseHeader* apLayers[2 + kMaxExtra];
	uint32_t nLayers = 0;

	if (bEyes)
	{
		for (int e = 0; e < EYE_COUNT; ++e)
		{
			ZeroMemory(&aProjViews[e], sizeof(aProjViews[e]));
			aProjViews[e].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
			aProjViews[e].pose = im.m_aViews[e].pose;	// exactly the pose we rendered with
			aProjViews[e].fov = im.m_aViews[e].fov;
			aProjViews[e].subImage.swapchain = im.m_aEyeChain[e].m_hSwapchain;
			aProjViews[e].subImage.imageRect.offset.x = 0;
			aProjViews[e].subImage.imageRect.offset.y = 0;
			aProjViews[e].subImage.imageRect.extent.width = (int32_t)im.m_aEyeChain[e].m_nWidth;
			aProjViews[e].subImage.imageRect.extent.height = (int32_t)im.m_aEyeChain[e].m_nHeight;
		}
		ZeroMemory(&layerProj, sizeof(layerProj));
		layerProj.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
		layerProj.space = im.m_hAppSpace;
		layerProj.viewCount = EYE_COUNT;
		layerProj.views = aProjViews;
		apLayers[nLayers++] = (const XrCompositionLayerBaseHeader*)&layerProj;
	}

	for (unsigned i = 0; i < nExtra; ++i)
	{
		Impl::Layer& layer = *im.m_apLayers[apExtra[i]->layer];
		FillQuad(aExtraQuads[i], apExtra[i]->place, im.m_hViewSpace, im.m_hAppSpace,
			layer.m_Chain.m_hSwapchain, layer.m_Chain.m_nWidth, layer.m_Chain.m_nHeight);
		apLayers[nLayers++] = (const XrCompositionLayerBaseHeader*)&aExtraQuads[i];
	}

	if (bScreen)
	{
		FillQuad(layerQuad, *pScreen, im.m_hViewSpace, im.m_hAppSpace,
			im.m_ScreenChain.m_hSwapchain, im.m_ScreenChain.m_nWidth, im.m_ScreenChain.m_nHeight);
		apLayers[nLayers++] = (const XrCompositionLayerBaseHeader*)&layerQuad;
	}

	XrFrameEndInfo ei;
	ZeroMemory(&ei, sizeof(ei));
	ei.type = XR_TYPE_FRAME_END_INFO;
	ei.displayTime = im.m_FrameState.predictedDisplayTime;
	ei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	ei.layerCount = nLayers;
	ei.layers = nLayers ? apLayers : NULL;
	im.Check(xrEndFrame(im.m_hSession, &ei), "xrEndFrame");
}

bool Bridge::Impl::DoRecenter()
{
	// New origin = where the head is now, turned to face where the head
	// faces now (yaw only, so the floor stays level).
	//
	// Only once the headset is really showing our frames (FOCUSED or
	// VISIBLE) and the head is actively tracked. Earlier than that the
	// runtime can report a placeholder pose (the bare origin), and
	// recentring on it put the origin at the runtime's floor-level point:
	// the view then sat high above the character and the menu screen sat
	// low and off to the side.
	if (m_eSessionState != XR_SESSION_STATE_FOCUSED && m_eSessionState != XR_SESSION_STATE_VISIBLE)
		return false;

	XrSpaceLocation loc;
	ZeroMemory(&loc, sizeof(loc));
	loc.type = XR_TYPE_SPACE_LOCATION;
	const XrSpaceLocationFlags kNeeded =
		XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT |
		XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
	if (XR_FAILED(xrLocateSpace(m_hViewSpace, m_hLocalSpace, m_FrameState.predictedDisplayTime, &loc)) ||
		(loc.locationFlags & kNeeded) != kNeeded)
	{
		return false;	// head not tracked yet; try again next frame
	}

	// Forward = orientation applied to -Z.
	const XrQuaternionf& q = loc.pose.orientation;
	float fx = -(2.0f * (q.x * q.z + q.w * q.y));
	float fz = -(1.0f - 2.0f * (q.x * q.x + q.y * q.y));
	float fYaw = atan2f(-fx, -fz);

	XrPosef offset;
	offset.orientation.x = 0.0f;
	offset.orientation.y = sinf(fYaw * 0.5f);
	offset.orientation.z = 0.0f;
	offset.orientation.w = cosf(fYaw * 0.5f);
	offset.position = loc.pose.position;
	CreateAppSpace(offset);
	Log("Recentred: head at %.2f %.2f %.2f m, yaw %.1f deg", loc.pose.position.x, loc.pose.position.y,
		loc.pose.position.z, fYaw * 57.29578f);
	return true;
}

bool Bridge::ResizeScreen(unsigned nWidth, unsigned nHeight)
{
	if (!m_bSessionCreated || !nWidth || !nHeight)
		return false;
	FPUGuard fpu;
	Impl& im = *m_pImpl;
	if (nWidth == im.m_ScreenTex.m_nWidth && nHeight == im.m_ScreenTex.m_nHeight)
		return true;

	// Nothing may still be using the old texture on either side.
	im.WaitForD3D9();
	im.WaitForD3D11();
	im.m_ScreenChain.Release();
	im.m_ScreenTex.Release();
	im.m_Screen.width = nWidth;
	im.m_Screen.height = nHeight;

	if (!im.m_ScreenTex.Create(im.m_pDev9, im.m_pDev11, nWidth, nHeight) ||
		!im.CreateSwapchain(im.m_ScreenChain, nWidth, nHeight))
	{
		Log("ERROR: could not resize the screen texture to %ux%u", nWidth, nHeight);
		im.m_ScreenChain.Release();
		im.m_ScreenTex.Release();
		return false;
	}
	Log("Screen texture resized to %ux%u", nWidth, nHeight);
	return true;
}

bool Bridge::IsSessionRunning() const	{ return m_pImpl && m_pImpl->m_bSessionRunning; }
bool Bridge::ShouldQuit() const			{ return m_pImpl && m_pImpl->m_bQuit; }
unsigned Bridge::GetEyeTextureWidth() const		{ return m_pImpl->m_aEyeTex[0].m_nWidth; }
unsigned Bridge::GetEyeTextureHeight() const	{ return m_pImpl->m_aEyeTex[0].m_nHeight; }
unsigned Bridge::GetEyeWidth() const
{
	unsigned n = (unsigned)((float)m_pImpl->m_aEyeTex[0].m_nWidth * m_pImpl->m_fEyeScale + 0.5f);
	return (n > 16) ? n : 16;
}
unsigned Bridge::GetEyeHeight() const
{
	unsigned n = (unsigned)((float)m_pImpl->m_aEyeTex[0].m_nHeight * m_pImpl->m_fEyeScale + 0.5f);
	return (n > 16) ? n : 16;
}
void Bridge::SetEyeRenderScale(float fScale)
{
	m_pImpl->m_fEyeScale = (fScale < 0.5f) ? 0.5f : (fScale > 1.0f ? 1.0f : fScale);
}
float Bridge::GetEyeRenderScale() const	{ return m_pImpl->m_fEyeScale; }
double Bridge::GetLastWaitMs() const	{ return m_pImpl->m_fLastWaitMs; }
double Bridge::GetDisplayPeriodMs() const
{
	return (double)m_pImpl->m_FrameState.predictedDisplayPeriod / 1000000.0;
}
IDirect3DTexture9* Bridge::GetEyeTexture(Eye eEye) const	{ return m_pImpl->m_aEyeTex[eEye].m_p9; }
IDirect3DTexture9* Bridge::GetScreenTexture() const			{ return m_pImpl->m_ScreenTex.m_p9; }
const EyeView& Bridge::GetEyeView(Eye eEye) const			{ return m_pImpl->m_aEyeViews[eEye]; }
bool Bridge::HasValidViews() const							{ return m_pImpl->m_bViewsValid && m_pImpl->m_FrameState.shouldRender; }
const Pose& Bridge::GetHeadPose() const						{ return m_pImpl->m_HeadPose; }
void Bridge::Recenter()										{ if (m_pImpl) m_pImpl->m_bRecenterPending = true; }
const HandInput& Bridge::GetHand(Hand eHand) const			{ return m_pImpl->m_aHands[eHand]; }
bool Bridge::HasFocus() const								{ return m_pImpl && m_pImpl->m_bSessionRunning && m_pImpl->m_eSessionState == XR_SESSION_STATE_FOCUSED; }

} // namespace xrb
