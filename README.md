# NOLF2 VR

A virtual reality conversion of **No One Lives Forever 2: A Spy in H.A.R.M.'s Way**
(Monolith, 2002), built to feel as close to a native VR game as possible:
full 6DoF head tracking, motion controllers with the gun in your hand,
aiming where the gun points, and the game's own interface brought into the
headset. It isn't a head-aimed injector mod: it's a source-level port built
on the Jupiter EX (LithTech) engine source and Monolith's NOLF2 game code.

> **Unofficial fan project.** No game data is included or stored here: you
> need your own copy of NOLF2, patched to 1.3. Not affiliated with
> Monolith, Warner Bros. or Activision.

## Playing

### What you need

- Windows 10 or 11, and a PC VR headset with an **OpenXR** runtime
  (Virtual Desktop, SteamVR or Meta Quest Link) set as the active one.
  Developed and tested on Quest 3 with Virtual Desktop (VDXR).
- **No One Lives Forever 2, patched to 1.3.** This mod was built on the
  English (UK) CD release with the official 1.3 patch; any retail copy
  updated to 1.3 should work. Copies are available from
  [My Abandonware](https://www.myabandonware.com/game/no-one-lives-forever-2-a-spy-in-h-a-r-m-s-way-3nv#download).

### Installing

1. Download the latest `NOLF2-VR-<version>.zip` from
   [Releases](../../releases) and unzip it. You get a folder called `VR`.
2. Put the `VR` folder inside your NOLF2 folder, next to `GAME.REZ`
   (for example `C:\Games\No One Lives Forever 2\VR`).
3. Start your VR runtime and put the headset on.
4. Double-click **`Play NOLF2 VR.bat`** in the `VR` folder.

Nothing else needs installing: the Visual C++ runtime and the one DirectX
file the game needs come with it. Your normal NOLF2 isn't changed; the VR
version keeps its own settings, saves and logs in the `VR` folder.

Once in: stand (or sit) as you'll play, look ahead, and hold both stick
clicks for a second to set your height. Settings are in **Options > VR**.

### If something goes wrong

- **`Check VR.bat`** tests the headset connection without the game.
- Logs are in the `VR` folder: `vr_game.log`, `xrbridge.log`, and after a
  crash `crash_<date>_<time>.txt` and `.dmp`. Please attach them when you
  report a problem.

## Features

- **Real VR**: stereo 3D with 6DoF head tracking at the headset's own frame
  rate, true world scale (adjustable), standing or seated play, crouching
  for real, and height calibration with one button hold.
- **The gun in your hand**: the game's own weapon models at real size,
  aiming where the gun points; each weapon's fit is tuned and adjustable in
  the headset. The crosshair sits on whatever the gun points at, and
  searching, picking up and using things work there.
- **Hands**: gloved hands; the free hand follows its controller, with the
  index finger and thumb sensed separately; grip the weapon with both hands
  for a steadier aim.
- **Reloading by hand**: drop the magazine, take a fresh one from over your
  shoulder, push it in; rack the slide or work the shotgun's pump when you
  run dry. (Or the game's own reload.)
- **Throwing by hand**: grenades, the banana, coins, shuriken: the harder
  you throw, the further they go. The bear trap and the Angry Kitty are set
  down where you put them.
- **Swords** hit what the blade passes through.
- **Scopes**: raise a scoped weapon (or the camera gadget) to your eye and
  look through a magnified lens.
- **Weapon ring**: hold the grip, reach for a weapon, let go.
- **Interface in the headset**: health and armour on your wrist like a
  watch, ammo on the gun, the HUD and subtitles on a floating panel, menus
  and conversations driven by the controllers.
- **The snowmobile**: drive with both hands on the handlebars (or the
  stick); on the scripted rides, look and turn freely.
- **Cutscenes** in 3D around you, or on a flat screen; the opening credits
  always play on the screen, framed as intended.
- **Comfort and accessibility**: smooth or snap turning, an optional
  comfort vignette, left-handed play (gun, hands and every button swap
  sides, with mirrored models), and a choice of walking stick.
- **Cheats page** with level select, for jumping to any level.
- **Stability**: fixes for a crash on exit and an occasional crash while
  loading a level, and readable crash reports.

## Controls

| Control | In play | In menus |
|---|---|---|
| Left stick | walk (push fully to run) | navigate |
| Right stick left / right | turn | navigate |
| Right stick up / down | jump / crouch toggle | navigate |
| Crouch for real (standing) | Cate crouches once your head is 25 cm below your set height | |
| Both stick clicks (hold 1 s) | set your height: your head now is Cate standing, facing forward | same |
| Right trigger | fire | select |
| Right grip (hold) | weapon ring: reach for a weapon, let go | |
| Right A / B | drop the magazine (manual reloading) or next ammo (game original) / holster | select / back |
| Right stick click | next ammo (manual reloading) or reload (game original) | |
| Left grip over your left shoulder | take a magazine (manual reloading, gun without one); push it into the gun to reload | |
| Left trigger | use, search, pick up (at the crosshair) | select |
| Left grip | on the weapon: hold it with both hands; elsewhere: pick up / drop a body | |
| Left X | mission objectives | |
| Left Y or menu button | pause menu | back |
| Left stick click | adjust the gun's fit in your hand (looking through a scope: adjust the scope; holding a magazine: adjust how it's held) | |
| Conversation choices | right stick up / down, right trigger or A to pick, B to say nothing | |
| Home key (keyboard) | recentre | recentre |

Left-handed (Options > VR > Gun hand: Left): every row swaps sides (fire
on the left trigger, magazines from over your right shoulder, health on
your right wrist, and so on); the gun and hands are drawn left-handed. The
pause menu stays on the left menu button. Which stick walks is its own
setting (Options > VR > Walk with).

## Settings

The everyday ones are on the **Options > VR** page in the game. Everything
below is kept in `vr_settings.txt` (in the `VR` folder; `run\` for a
source build), written when you leave that page. You can also edit it by
hand: one `Name value` per line, read when the game starts.

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
| `VRThrow` | 1 | 1 = throw by hand (hold the trigger, swing, let go); 0 = the game's own throw |
| `VRThrowSpeed` | 5 | how fast a throw (m/s, at the hand) goes the game's own distance; faster goes further |
| `VRLeftHanded` | 0 | 1 = the gun in your left hand: the gun, the free hand and every button swap sides |
| `VRWalkStick` | 0 | the stick you walk with: 0 = left, 1 = right (the other turns) |
| `VRCutsceneFlat` | 0 | cutscenes: 0 = in 3D around you; 1 = on a flat screen in front of you |
| `VRVehicleTilt` | 0 | riding the snowmobile: 0 = the horizon stays level; 1 = the view banks and climbs with it |
| `VRSeated` | 0 | 0 = standing (crouch for real); 1 = seated (the stick crouches). Options > VR > Play position |
| `VRCrouchDepth` | 25 | standing: how far (cm) your head drops below your set height before Cate crouches; 0 = stick only |
| `VRSwing` / `VRSwingSpeed` | 1 / 2.5 | swinging a sword hits (0 = trigger only); how fast the blade's tip must move, m/s |
| `VRVignette` / `VRVignetteWhen` | 0 / 3 | comfort vignette 0 off, 1 light, 2 medium, 3 strong; when: 1 moving, 2 turning, 3 both |
| `VRMirror` | 1 | the monitor shows what the headset shows (scope, crosshair, watch and all); 0 = the game's flat camera |

Files the game writes (in the `VR` folder, or `run\` for a source build):

| File | What it is |
|---|---|
| `vr_settings.txt` | your VR settings (Options > VR, or edit by hand) |
| `vr_guns.txt` | each weapon's fit in your hand, its scope offset and how its magazine is held (from the adjust mode) |
| `xrbridge.log`, `vr_game.log`, `vr_models.log` | headset, game and model logs |
| `crash_*.txt`, `crash_*.dmp` | crash reports |

## Building from source

### Requirements

- Your own NOLF2 patched to 1.3, by default at
  `D:\Games\No One Lives Forever 2` (change `NOLF2DIR` in
  `setup_run_dir.bat` if yours is elsewhere).
- **Visual Studio 2022** with *Desktop development with C++*, *C++ CMake
  tools* and *C++ ATL*.
- **DirectX SDK (June 2010)**. The S1023 error at the end of its installer
  is harmless.

### Commands

| Command | What it does |
|---|---|
| `update.bat` | Pull the latest changes, then build (the game must be closed) |
| `build_vs2022.bat` | Build only (`clean` as an argument rebuilds from scratch) |
| `run_nolf2_vr.bat` | Play in VR |
| `run_nolf2.bat` | Play on the monitor, windowed |
| `run_vrprobe.bat` | Headset test: a floating test picture, no game |
| `make_release.bat` | Build and package a release (see below) |

A source build runs from the `run\` folder, made by `setup_run_dir.bat`
with links to your NOLF2 data, so your install stays untouched.

### Making a release

`make_release.bat` builds, then makes `release\NOLF2-VR-<version>.zip`
(the version is in the `VERSION` file): one `VR` folder holding the engine
and its DLLs, the game code (`gamedll`), the launcher
(`tools\release\Play NOLF2 VR.bat`), `Check VR.bat`, the player readme
(`tools\release\README.txt`), the Visual C++ runtime, `d3dx9_43.dll`, and
the tuned starting gun fits (`tools\release\vr_guns.txt`, copied to
`vr_guns.txt` on first run). No game files.

The launcher runs the engine from the `VR` folder with `-rez ..\Game.rez`
and so on. On first run it copies `Engine.REZ`, `binkw32.dll`, the profile
and `autoexec.cfg` from the game folder and links `Movies`.

It also makes `release\NOLF2-VR-<version>-symbols.zip`, the `.pdb` files
of that exact build. Keep it with the release (it needn't be published):
it's what turns a player's crash `.dmp` into function names and lines.

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
- **A crash on exit**: object references left by the unloaded game DLL are
  cut loose before the objects are removed (`clientmgr.cpp`).
- **An occasional crash loading a level**: a race inside Windows'
  DirectMusic while music loads; its service thread is held still while
  it's parked, for the load (`ltdirectmusic_impl.cpp`).

## Credits

- *No One Lives Forever 2*, its game code (from Monolith's SDK) and the
  LithTech Jupiter EX engine: **Monolith Productions**.
- The open-source engine project this repository continues
  ([lithtech](https://github.com/bwrsandman/lithtech)):
  - **James Lacey** (2012): started it; one build for the engine and NOLF2,
    modern Visual Studio, first playable.
  - **Kyle Repinski (MWisBest)** (2015): Visual Studio 2015 and 64-bit
    support, warning clean-up, zlib update, tools building.
  - **Sandy Carter (bwrsandman)** (2019): other compilers and Linux; hosts
    the project.
- The fix for retail levels loading: the **MatzeLoCal** macOS port.
- OpenXR SDK and loader: **the Khronos Group** (Apache 2.0), in
  `external/OpenXR-SDK`, including **jsoncpp** (MIT).
- DirectMusic headers: **the Wine project** (LGPL 2.1).
- **zlib**: Jean-loup Gailly and Mark Adler.
- The Visual C++ runtime and D3DX9 in the release: Microsoft
  redistributables.

This is an unofficial, free fan project, not affiliated with Monolith,
Warner Bros. or Activision.
