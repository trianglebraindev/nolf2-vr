//------------------------------------------------------------------
//
//  MODULE  : D3D_VR.H
//
//  PURPOSE : Hooks between the Direct3D 9 renderer and the VR bridge
//            (vr/xrbridge). VR is switched on with -vr on the command
//            line; without it none of this does anything.
//
//------------------------------------------------------------------

#ifndef __D3D_VR_H__
#define __D3D_VR_H__

struct SceneDesc;

// True if -vr was given on the command line.
bool d3d_VR_Requested();

// Before the device is created: connects to the headset and checks it
// is on the adapter the engine is about to use. Returns true if VR can
// go ahead.
bool d3d_VR_PreDeviceCreate(uint32 iAdapter);

// After the device is created: creates the headset session and the
// textures shared with it.
void d3d_VR_PostDeviceCreate(IDirect3DDevice9Ex* pDevice, uint32 nScreenWidth, uint32 nScreenHeight);

// After the device has been reset to a new screen size (the game changed
// resolution): the interface layer has to match the new size.
void d3d_VR_OnModeChange(uint32 nScreenWidth, uint32 nScreenHeight);

// Before the device is released.
void d3d_VR_Term();

// True while VR is up and running.
bool d3d_VR_IsActive();

// Called from d3d_RenderScene before the normal (monitor) pass. If this
// scene is the frame's main view of the world, renders it once per eye
// into the headset's textures.
void d3d_VR_RenderEyes(SceneDesc* pDesc);

// Called from d3d_RenderScene after the normal pass. If the eyes were
// drawn for that scene, the rest of the frame's drawing (the interface)
// goes to the headset's interface layer.
void d3d_VR_AfterRenderScene();

// Called from the engine's input processing: controller buttons that the
// game has bound to commands (LTVR_SetBinding) switch those commands on.
void d3d_VR_ApplyCommands(uint8* pCommands, uint32 nCommands);

// Called once per frame, just before Present: sends the finished frame
// to the headset (3D eyes if they were drawn this frame, otherwise the
// flat screen), then waits for the headset's next frame slot. That wait
// is what paces the game to the headset.
void d3d_VR_OnSwapBuffers();

#endif // __D3D_VR_H__
