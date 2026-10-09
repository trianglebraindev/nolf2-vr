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

// True if -vr was given on the command line.
bool d3d_VR_Requested();

// Before the device is created: connects to the headset and checks it
// is on the adapter the engine is about to use. Returns true if VR can
// go ahead.
bool d3d_VR_PreDeviceCreate(uint32 iAdapter);

// After the device is created: creates the headset session and the
// textures shared with it.
void d3d_VR_PostDeviceCreate(IDirect3DDevice9Ex* pDevice, uint32 nScreenWidth, uint32 nScreenHeight);

// Before the device is released.
void d3d_VR_Term();

// True while VR is up and running.
bool d3d_VR_IsActive();

// Called once per frame, just before Present: sends the finished frame
// to the headset.
void d3d_VR_OnSwapBuffers();

#endif // __D3D_VR_H__
