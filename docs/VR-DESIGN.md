# NOLF2 VR — design notes and comfort/QoL backlog

Living document. Decisions get recorded here as they're made; open
questions stay marked **(open)** until Tre decides them.

## Stages

| Stage | What it gives | Status |
|---|---|---|
| 0 | Engine builds from source on VS2022, runs the retail campaign | done |
| 1a | Headset plumbing (D3D9Ex -> D3D11 -> OpenXR), stand-alone probe | done |
| 1b | Game on a floating screen in the headset | done |
| 2 | Stereo 3D world + head tracking (mouse/keys still drive the body) | done (90 fps held at 3072x3264/eye) |
| 3 | Game-side camera: head drives look; mouse/stick only turns the body; bob/roll/shake tamed | done (in test) |
| 4 | Motion controllers: weapon in hand, aim from the muzzle, two-hand stabilising | |
| 5 | Locomotion and turning options (below), controller bindings | first pass in test: smooth/snap turn, head/hand-relative analog walk, bindings |
| 6 | HUD in the world (wrist / weapon), menus with a laser pointer | first pass in test: HUD fixed in front of the eyes, in-game menus on a panel, fades darken the view; laser pointer next |
| 7 | Gadgets and interactions (search, lockpick, pick up), vehicles, cutscenes | |
| later | Manual reloading (Tre: fine to skip at first) | |

## Comfort and quality-of-life backlog

Starting from a general VR checklist, adapted to what NOLF2 actually
needs. Not ordered within a group yet.

### Movement and comfort
- **Smooth locomotion** (stick). The natural fit: NOLF2's stealth is about
  pacing and sight lines.
- **Teleport (decided: Claude's call).** Plan: a quick "dash" that physically
  travels the path in a fraction of a second, so trigger volumes (scripted
  events, alarms, conversations) still fire and collision still applies, with
  the view faded or vignetted during the dash.
- **Snap turn / smooth turn**, with adjustable snap angle and smooth speed.
  Easy: the bridge already separates body heading from head direction.
- **Comfort vignette** during movement/turning, adjustable strength. Drawn in
  the eye pass.
- **Head- vs controller-relative movement** direction.
- **Vehicles (open):** the snowmobile and bike sections need their own
  treatment (seated cockpit view, vignette, stable horizon).

### Body and camera
- **Seated / standing**, with a **height calibration** step. Ties into world
  scale (`VRUnitsPerMeter`) so Cate's eye height matches yours.
- **Real crouch → game crouch** (head drops below a threshold), plus a
  **crouch toggle button** for seated play.
- **Jump button.**
- **Recentre** on a button (and automatically when tracking starts — done).
- Game camera effects (head bob, damage shake, lean roll) toned down or
  removed in VR — they cause nausea when the head isn't the one moving.

### Controls and hands
- **Dominant hand swap** (left-handed play), including where holsters and
  the wrist HUD sit.
- **Grip: hold vs toggle**, for holding weapons and gadgets.
- **Two-handed weapons** (rifles, crossbow): off-hand stabilising, optional.
- **Physics interaction: low priority.** NOLF2 has few physics props; pick-ups
  are "activate" actions. A reach-and-grab for pick-ups and doors would still
  feel good.

### Interface
- **Menus** on the floating screen with a controller laser pointer.
- **HUD in the world:** health/armor on the wrist, ammo on the weapon,
  objectives/messages on a small panel that follows the gaze lazily.
- **Subtitles/conversation choices** (NOLF2 has dialogue choices) on a panel
  in front, not stuck to the face.
- **Cutscenes (decided):** in-engine cutscenes play in 3D around you by default,
  with an option to show them on the floating screen instead. Pre-rendered
  movies (Bink) always play on the floating screen.

## Technical decisions so far

- Engine: open Jupiter EX source (bwrsandman/lithtech) + retail-level fix,
  Direct3D 9Ex renderer, 32-bit.
- Headset path: OpenXR via a private D3D11 device sharing textures with D3D9Ex.
  VDXR (Virtual Desktop) is the target runtime; it supports 32-bit apps.
- The bridge restores full FPU precision around every OpenXR call (LithTech
  runs the FPU at 24 bits).
- World scale: `VRUnitsPerMeter` = 60. Measured: the player is 106 units tall
  with the eye 94 units above the feet; a ~1.58 m eye height gives ~60.
- Recentre only once the head is actively tracked and the session is visible
  (an early placeholder pose put the view high above the character). The
  runtime's own recentre is followed. Interim recentre key: Home.
- Game <-> engine VR interface: `sdk/inc/ltvr.h`. Lithtech.exe exports
  `LTVR_GetState / SetOrigin / SetBinding / ClearBindings / Recenter`;
  the game finds them with GetProcAddress, so the game DLLs still load
  in the flat-screen engine. The game (`VRMgr.cpp`) owns the layout;
  the engine turns bound controller buttons into game commands during
  input processing, so they behave exactly like keys.
- Crash reports: any crash writes `run\crash_<date>_<time>.txt` (readable
  call stack) and a matching `.dmp` for Visual Studio.

## Controller layout (first pass, for testing)

| Control | In play | In menus |
|---|---|---|
| Left stick | walk (push past 80% to run) | navigate |
| Right stick left/right | smooth turn (snap optional) | navigate |
| Right stick up / down | jump / crouch toggle | navigate |
| Right trigger | fire | select |
| Right grip | next weapon (until the hand ring exists) | |
| Right A / B | next ammo / holster | select / back |
| Right stick click | reload | |
| Left trigger | activate (use, talk, search) | select |
| Left X | mission objectives | |
| Left Y or menu button | pause menu | back |
| Home key (keyboard) | recentre | recentre |

Console settings (type in the console or add `+Name value` to the launch
line): `VRSnapTurn 0/1`, `VRTurnSpeed 150` (degrees per second),
`VRSnapAngle 45`, `VRMoveWithHand 0/1`, `VRUnitsPerMeter 60`,
`VRHudDistance 2` (metres), `VRHudAngle 60` (how wide the HUD looks, degrees),
`VRScreenDistance 2`, `VRScreenAngle 62` (menus and the floating screen).

## Interface in the headset (first pass)

- Everything the game draws after the 3D world (HUD, subtitles, dialogue
  choices, in-game menus, cursor, console) is drawn into a transparent
  layer instead of the monitor picture, and shown over the world.
- While playing it sits fixed in front of the eyes. In-game menus, popups
  and message boxes appear on a panel where you're looking and stay put.
- Screen fades darken the whole headset view; the letterbox bars are off.
- Later: health/ammo move to the wrist or the gun, and the crosshair goes
  away once aiming comes from the gun (stage 4).
