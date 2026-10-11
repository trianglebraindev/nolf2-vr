# NOLF2 VR

A virtual reality conversion of **No One Lives Forever 2: A Spy in H.A.R.M.'s Way**
(Monolith, 2002), built to feel as close to a native VR game as possible:
full 6DoF head tracking, motion controllers with the gun in your hand, aiming
where the gun points, and the game's own interface brought into the headset.
It is not a "head-aimed" injector mod. It's a source-level port built on the
open Jupiter EX (LithTech) engine release and the NOLF2 game code.

> **Private repository.** The NOLF2 game code is covered by Monolith's
> original SDK license, and the licensing of the engine release is unclear.
> Don't make this repository public. No retail game data is stored here:
> you need your own retail copy of NOLF2, patched to 1.3.

## What works

- **Stereo 3D with 6DoF head tracking**, at the headset's own frame rate
  (tested at 90 Hz on Quest 3 through Virtual Desktop / VDXR), with true
  world scale.
- **Motion controllers**: walk where you look (or where your left hand
  points), smooth or snap turning, all game actions on the controllers,
  menus navigable with the sticks.
- **The gun in your hand**: the game's own weapon models, at real size and
  depth in your right hand, with a per-weapon fit you can adjust in the
  headset. Shots leave from your hand along the barrel.
- **Hands**: forearms trimmed to gloved hands; the left hand follows your left
  controller and opens and closes with the left grip; grip the weapon with
  your left hand for a steadier two-handed aim.
- **Crosshair on the target**: the game's own crosshair, accuracy spread
  included, placed on whatever the gun points at. Searching, picking up and
  using things work where the crosshair is.
- **Weapon ring**: hold the right grip, reach for a weapon, let go.
- **Interface in the headset**: the HUD, subtitles, conversation choices,
  in-game menus and the console are drawn into their own layer. Health and
  armour sit on your left wrist like a watch (turn it toward you to read
  it) and ammo on the gun; the rest of the HUD
  stays in front of you, menus appear on a panel where you're looking,
  screen fades darken your whole view, and the main menus and movies play on
  a big floating screen.
- **Crash reports**: a crash writes a readable report and a minidump to
  `run\`.

- **VR settings in the game**: Options > VR (turning, walking direction,
  world size, HUD distance and size, wrist and gun read-outs, hands or arms).

- **Scopes**: raise a scoped weapon (or the camera gadget) to your eye and
  the lens shows the magnified view. If a scope's picture doesn't sit on
  the scope, click the left stick while looking through it and move it
  with the left stick (saved per weapon).

- **Reloading by hand** (default; Options > VR > Reloading: Manual / Game
  original): when the gun's empty, A drops the magazine, reach over your
  left shoulder and grip to take a new one, and push it into the gun.
  Guns without a separate magazine in their model (the shotgun) reload by
  bringing your left hand to the gun. Fired empty, a gun then needs
  cocking: grip the slide / cocking handle with your left hand and pull
  it back (the shotgun: pump it back and forward).

- **Swords**: swing the katana or tulwar and it hits what the blade passes
  through (the trigger still does the game's own slash). How hard a swing
  has to be, or off: Options > VR > Sword swings.

- **Snapshots**: left grip + left trigger saves what the headset shows to
  `run\Snapshots`; the monitor shows the same picture.

- **Comfort vignette** (off by default): the edges of the view darken
  while you move or turn with the sticks. Options > VR > Comfort vignette
  (Off / Light / Medium / Strong) and Vignette when (moving, turning or
  both).

Still to come (see `docs/VR-DESIGN.md`): height and real crouch, vehicles,
cutscene options.

## Requirements

- Retail **NOLF2 patched to 1.3**, by default at `D:\Games\No One Lives Forever 2`
  (change `NOLF2DIR` in `setup_run_dir.bat` if yours is elsewhere)
- A PC VR headset with an **OpenXR** runtime that supports Direct3D 11 and
  32-bit programs. Developed on Quest 3 with Virtual Desktop and VDXR.
- **Visual Studio 2022** with *Desktop development with C++*, *C++ CMake tools*
  and *C++ ATL*
- **DirectX SDK (June 2010)**. The S1023 error at the end of its installer
  is harmless.

## Building and playing

| Command | What it does |
|---|---|
| `update.bat` | Pull the latest changes from GitHub, then build |
| `build_vs2022.bat` | Build only (`clean` as an argument rebuilds from scratch) |
| `run_nolf2_vr.bat` | Play in VR |
| `run_nolf2.bat` | Play on the monitor, windowed |
| `run_vrprobe.bat` | Headset test: a floating test picture, no game |

The game runs from the `run\` folder, never from the retail install. It's
made by `setup_run_dir.bat` with links to the retail data, so the retail
install stays untouched.

## Controls

| Control | In play | In menus |
|---|---|---|
| Left stick | walk (push fully to run) | navigate |
| Right stick left / right | turn | navigate |
| Right stick up / down | jump / crouch toggle | navigate |
| Right trigger | fire | select |
| Right grip (hold) | weapon ring: reach for a weapon, let go | |
| Right A / B | drop the magazine (manual reloading) or next ammo (game original) / holster | select / back |
| Right stick click | next ammo (manual reloading) or reload (game original) | |
| Left grip over your left shoulder | take a magazine (manual reloading, gun without one); push it into the gun to reload | |
| Left trigger | use, search, pick up (at the crosshair) | select |
| Left grip + left trigger | snapshot of what the headset shows (`run\Snapshots`) | |
| Left grip | on the weapon: hold it with both hands; elsewhere: pick up / drop a body | |
| Left X | mission objectives | |
| Left Y or menu button | pause menu | back |
| Left stick click | adjust the gun's fit in your hand (looking through a scope: adjust the scope; holding a magazine: adjust how it's held) | |
| Conversation choices | right stick up / down, right trigger or A to pick, B to say nothing | |
| Home key (keyboard) | recentre | recentre |

## Settings

The everyday ones are on the **Options > VR** page in the game. Everything
below is kept in `run\vr_settings.txt` (written when you leave that page),
which you can also edit by hand: one `Name value` per line, read when the
game starts. Values in that file win over the same setting given on the
launch line in `run_nolf2_vr.bat` as `+Name value`.

| Setting | Default | What it does |
|---|---|---|
| `VRUnitsPerMeter` | 54.5 | world scale (lower = everything bigger); "World size" on the VR page, where 110% = 54.5 and 100% = 60 |
| `VRSnapTurn` | 0 | 1 = snap turning |
| `VRTurnSpeed` / `VRSnapAngle` | 150 / 45 | smooth turn speed (degrees/s), snap step (degrees) |
| `VRMoveWithHand` | 0 | 1 = walk where the left controller points |
| `VRHudDistance` / `VRHudAngle` | 2 / 60 | HUD distance (m) and width (degrees) |
| `VRHudFollow` / `VRHudFollowAngle` | 1 / 35 | HUD glides back after you turn this far; 0 = locked to view |
| `VRScreenDistance` / `VRScreenAngle` | 2 / 62 | menus and the floating screen |
| `VRGunScale` | 12 | size of the gun in your hand |
| `VRArms` | 0 | 1 = the game's full arms instead of hands |
| `VRWristBack` / `VRHandPalm` | 7 / 3.5 | the free left hand: how far the wrist sits behind the middle of the controller's handle, and how far the hand sits off the handle toward its back (cm) |
| `VRWristHud` / `VRGunHud` | 1 / 1 | 0 = health / ammo back on the HUD panel |
| `VRWristHudWatch` | 1 | 1 = health shows as you turn your wrist toward you; 0 = always shown, facing you |
| `VRWristHudX/Y/Z`, `VRWristHudSize` | -3 / 0 / -9, 9 | where health sits on the left wrist (cm), its width (cm) |
| `VRGunHudX/Y/Z`, `VRGunHudSize` | -6 / -2 / 6, 10 | where ammo floats by the gun (cm), its width (cm) |
| `VRScope` | 1 | 0 = no looking through scopes |
| `VRScopeHeight` | 8 | how far (cm) above the aim line a scope's own line runs |
| `VRScopeEye` | 5 | how close (cm) an eye must be to the scope's line to look through it |
| `VRScopeDist` / `VRScopeSize` | 8 / 5.5 | how far in front of that eye the lens sits, and how wide it looks (cm) |
| `VRScopeZoom` | 4 | magnification of a basic scope; stronger scopes are 2x and 4x that |
| `VRReload` | 1 | 1 = reload by hand (A drops the magazine, one from over your left shoulder); 0 = the game's reload on the right stick click |
| `VRSwing` / `VRSwingSpeed` | 1 / 2.5 | swinging a sword hits (0 = trigger only); how fast the blade's tip must move, m/s |
| `VRVignette` / `VRVignetteWhen` | 0 / 3 | comfort vignette 0 off, 1 light, 2 medium, 3 strong; when: 1 moving, 2 turning, 3 both |
| `VRMirror` | 1 | the monitor shows what the headset shows (scope, crosshair, watch and all); 0 = the game's flat camera |

Files the game writes in `run\`:

| File | What it is |
|---|---|
| `vr_settings.txt` | your VR settings (Options > VR, or edit by hand) |
| `vr_guns.txt` | each weapon's fit in your hand, its scope offset and how its magazine is held (from the adjust mode) |
| `xrbridge.log`, `vr_game.log`, `vr_models.log` | headset, game and model logs |
| `crash_*.txt`, `crash_*.dmp` | crash reports |
| `Snapshots\vr_*.png` | snapshots (left grip + left trigger) |

## How it works

- **Engine** (`runtime/`): the Direct3D 9 renderer runs on Direct3D 9Ex so
  its frames can be shared with the headset. When `-vr` is given,
  `runtime/render_a/src/sys/d3d/d3d_vr.cpp` draws the main view once per
  eye with each eye's own pose and field of view, catches the interface
  drawn after the world in a transparent layer, places the gun's
  "really close" space in the world, and exports a small plain-C interface
  to the game (`sdk/inc/ltvr.h`).
- **Headset bridge** (`vr/xrbridge/`): OpenXR through a private Direct3D 11
  device that reads the shared textures. It submits the eyes, the
  interface layer and small "spot" layers (crosshair, weapon ring), and
  reads the controllers.
- **Game** (`NOLF2/ClientShellDLL/`): `VRMgr.cpp` turns headset and
  controller state into the camera, movement, turning, button bindings,
  the gun's placement, the hands (on the models' skeletons, no model
  edits), the weapon ring and the gun fit. Small hooks elsewhere hand it
  the camera, weapon, crosshair, targeting and interface.

Design notes, decisions and the roadmap: [`docs/VR-DESIGN.md`](docs/VR-DESIGN.md).

## Changes to the original engine source (besides VR)

- **Retail levels load**: world vertices are read in the retail 44-byte
  layout (`d3d_renderblock.cpp`; fix credit: the MatzeLoCal macOS port).
- **Builds with VS2022 and the June 2010 DirectX SDK**: DirectX paths,
  library names, missing standard includes, `winres.h` in place of MFC's
  `afxres.h`.
- **DirectMusic** (the adaptive music): headers from the Wine project
  (LGPL 2.1, see `runtime/compat/directmusic/README.txt`) and the interface
  IDs in `runtime/kernel/src/sys/win/dmusic_guids.cpp`.
- **Direct3D 9Ex**: no managed texture pool, so textures upload through a
  staging copy and the few small lockable ones are dynamic textures.

## Credits

- *No One Lives Forever 2* and its game code: Monolith Productions.
- The engine source: the GPL release of the Jupiter EX engine (LithTech PC
  Enterprise Edition build 69), via the
  [lithtech project](https://github.com/bwrsandman/lithtech), whose history
  this repository continues.
- OpenXR SDK: the Khronos Group (Apache 2.0), in `external/OpenXR-SDK`.
- DirectMusic headers: the Wine project (LGPL 2.1).

This is an unofficial fan project, not affiliated with Monolith, Warner
Bros. or Activision.
