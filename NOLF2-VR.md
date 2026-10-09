# NOLF2 VR

A VR conversion of *No One Lives Forever 2: A Spy in H.A.R.M.'s Way* with
6DoF head and motion-controller tracking, built on the open Jupiter EX
(LithTech) engine source and the NOLF2 game code.

**Private repository.** The NOLF2 game code is covered by Monolith's original
SDK license, and the licensing of the engine release is unclear. Do not make
this repository public. No retail game data is stored here; you need your own
retail NOLF2 install (patched to 1.3).

## Requirements

- Visual Studio 2022 with *Desktop development with C++*, *C++ CMake tools*
  and *C++ ATL*
- DirectX SDK (June 2010) — the installer's S1023 error at the end is harmless
- Retail NOLF2 1.3 at `D:\Games\No One Lives Forever 2`
  (change `NOLF2DIR` in `setup_run_dir.bat` if it lives elsewhere)

## Everyday use

| Command | What it does |
|---|---|
| `update.bat` | Pull the latest changes from GitHub, then build |
| `build_vs2022.bat` | Build only (`clean` as an argument rebuilds from scratch) |
| `run_nolf2.bat` | Play the built game, windowed |
| `run_nolf2_vr.bat` | Play in VR (stereo world; menus on a floating screen) |
| `run_vrprobe.bat` | Headset plumbing test (floating test picture) |
| F5 in Visual Studio (`build\Lithtech.sln`, Release) | Play under the debugger |

The game runs from the `run\` folder, never from the retail install. See
`setup_run_dir.bat` for why and how.

## What changed from the original engine source

- **Retail level loading**: world vertices are read in the retail 44-byte
  layout (`d3d_renderblock.cpp`; fix credit: the MatzeLoCal macOS port).
- **Builds on VS2022 + DirectX SDK June 2010**: DirectX paths, library names,
  missing standard includes, `winres.h` in place of MFC's `afxres.h`.
- **Direct3D 9Ex**: the renderer creates a 9Ex device (needed to share frames
  with the headset). 9Ex has no managed texture pool, so textures are uploaded
  through a staging copy (`d3d_TransferTexture`) and the few small lockable
  ones become dynamic textures (`d3d_device_wrapper.h`).
- **VR hooks**: `runtime/render_a/src/sys/d3d/d3d_vr.cpp`, switched on by `-vr`.
- **DirectMusic** (the adaptive music): headers from the Wine project
  (LGPL 2.1, see `runtime/compat/directmusic/README.txt`) and the interface
  IDs in `runtime/kernel/src/sys/win/dmusic_guids.cpp`.

Design notes and the VR comfort/QoL backlog: `docs/VR-DESIGN.md`.

Credits: the original engine source is from the bwrsandman/lithtech project,
whose history this repository continues.
