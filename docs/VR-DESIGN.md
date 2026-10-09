# NOLF2 VR — design notes and comfort/QoL backlog

Living document. Decisions get recorded here as they're made; open
questions stay marked **(open)** until Tre decides them.

## Stages

| Stage | What it gives | Status |
|---|---|---|
| 0 | Engine builds from source on VS2022, runs the retail campaign | done |
| 1a | Headset plumbing (D3D9Ex -> D3D11 -> OpenXR), stand-alone probe | done |
| 1b | Game on a floating screen in the headset | done |
| 2 | Stereo 3D world + head tracking (mouse/keys still drive the body) | in progress |
| 3 | Game-side camera: head drives look; mouse/stick only turns the body; bob/roll/shake tamed | |
| 4 | Motion controllers: weapon in hand, aim from the muzzle, two-hand stabilising | |
| 5 | Locomotion and turning options (below), controller bindings | |
| 6 | HUD in the world (wrist / weapon), menus with a laser pointer | |
| 7 | Gadgets and interactions (search, lockpick, pick up), vehicles, cutscenes | |
| later | Manual reloading (Tre: fine to skip at first) | |

## Comfort and quality-of-life backlog

Starting from a general VR checklist, adapted to what NOLF2 actually
needs. Not ordered within a group yet.

### Movement and comfort
- **Smooth locomotion** (stick). The natural fit: NOLF2's stealth is about
  pacing and sight lines.
- **Teleport (open).** Gold standard for comfort, but in NOLF2 it can skip
  trigger volumes (scripted events, alarms, conversations fire when you walk
  through them) and breaks stealth timing. Options to weigh: a fast "dash"
  that still travels the path, or teleport that sweeps through triggers.
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
- **Cutscenes (open):** in-engine cutscenes could play in 3D around you, or on
  the floating screen (more comfortable when the camera moves on its own).

## Technical decisions so far

- Engine: open Jupiter EX source (bwrsandman/lithtech) + retail-level fix,
  Direct3D 9Ex renderer, 32-bit.
- Headset path: OpenXR via a private D3D11 device sharing textures with D3D9Ex.
  VDXR (Virtual Desktop) is the target runtime; it supports 32-bit apps.
- The bridge restores full FPU precision around every OpenXR call (LithTech
  runs the FPU at 24 bits).
- World scale: `VRUnitsPerMeter` console variable, default 60 pending
  calibration from the player's measured size (`run/vr_game.log`).
