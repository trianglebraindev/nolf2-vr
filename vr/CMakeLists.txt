# ---------------------------------------------------------------------------
# NOLF2 VR: the OpenXR bridge and its stand-alone test program.
# ---------------------------------------------------------------------------

# LIB_XRBridge: D3D9Ex -> D3D11 -> OpenXR. Knows nothing about the game.
add_library(LIB_XRBridge STATIC
	xrbridge/xrbridge.cpp
	xrbridge/xrbridge.h)
target_include_directories(LIB_XRBridge PUBLIC xrbridge)
target_link_libraries(LIB_XRBridge PUBLIC OpenXR::openxr_loader d3d9 d3d11 dxgi)

# VRProbe: shows a test picture on a floating screen in the headset.
add_executable(EXE_VRProbe WIN32 probe/vrprobe.cpp)
target_link_libraries(EXE_VRProbe LIB_XRBridge)
set_target_properties(EXE_VRProbe PROPERTIES OUTPUT_NAME VRProbe)
