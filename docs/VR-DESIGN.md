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
| 4 | Motion controllers: weapon in hand, aim from the muzzle, two-hand stabilising | first pass in test: gun in the right hand, shots from the hand along the gun, crosshair on the target |
| 5 | Locomotion and turning options (below), controller bindings | first pass in test: smooth/snap turn, head/hand-relative analog walk, bindings |
| 6 | HUD in the world (wrist / weapon), menus with a laser pointer | first pass in test: health on the left wrist, ammo on the gun, rest of the HUD on a lazy panel, in-game menus on a panel, fades darken the view; laser pointer next |
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
- World scale: default `VRUnitsPerMeter` 54.5 ("110%" world size; Tre
  plays at 120% and picked 110% as the default). It started at 60 from
  measuring: the player is 106 units tall with the eye 94 units above the
  feet, and a ~1.58 m eye height gives ~60; in play the world felt a
  little small at that.
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
| Right grip (hold) | weapon ring: move your hand to a weapon, let go to switch (let go in the middle to cancel) | |
| Right A / B | next ammo / holster | select / back |
| Right stick click | reload | |
| Left trigger | activate (use, talk, search) | select |
| Left X | mission objectives | |
| Left Y or menu button | pause menu | back |
| Left stick click | adjust gun (again: done, saves) | |
| Left grip (hand on the weapon) | hold the gun with both hands | |
| Conversation choices | right stick up/down highlights, right trigger or A picks, B says nothing | |
| Home key (keyboard) | recentre | recentre |

Console settings (type in the console or add `+Name value` to the launch
line): `VRSnapTurn 0/1`, `VRTurnSpeed 150` (degrees per second),
`VRSnapAngle 45`, `VRMoveWithHand 0/1`, `VRUnitsPerMeter 54.5`,
`VRHudDistance 2` (metres), `VRHudAngle 60` (how wide the HUD looks, degrees),
`VRScreenDistance 2`, `VRScreenAngle 62` (menus and the floating screen),
`VRHudFollow 1` (0 = HUD locked to your view), `VRHudFollowAngle 35`.

Gun in the hand (tune while testing; the log `run\vr_game.log` lists each
weapon's numbers): `VRGunScale 12` (size), `VRGunHandX 12`, `VRGunHandY -18`,
`VRGunHandZ 30` (cm from the eye where the normal view holds the gun;
changing them slides the gun in your hand), `VRGunPitch 0` (degrees,
positive = barrel down).

## The gun (first pass)

- The game draws its gun, muzzle flash and attachments in "really close"
  space: a miniature space around the camera with its own projection.
  In the headset the engine places that space in the world around the
  camera, scaled up by `VRGunScale`, and draws it with each eye's view.
- The game treats its normal view as "the gun held at a natural spot"
  (`VRGunHand*`) and moves that picture from the natural spot to the right
  controller (grip position, aim direction).
- Shots leave from the hand along the gun. The crosshair (with the game's
  accuracy spread) is drawn into its own layer, placed on whatever the gun
  is aimed at, at a constant apparent size.
- Activation (search, pick up, doors, pick lock) goes along the gun's aim,
  where the crosshair is (Tre), with the action text beside the crosshair.
- Adjust gun (left stick click while playing): the sticks slide, tilt and
  resize the current gun relative to the hand while the aim stays on the
  controller's pointing ray, so you line the sights up with the crosshair.
  Saved per weapon to `run\vr_guns.txt`; once good, it becomes the shipped
  default.
- The engine's console key is compiled out in this ("final") build, so
  settings go on the launch line (`+Name value`) or through the in-game
  adjust mode.

## Hands (first pass)

Goal (Tre): hands separate like Half-Life: Alyx / Titanfall 2 VR, done in
code rather than by editing models where possible.

- Every first-person model shares one skeleton (`run\vr_models.log`):
  `armL > wristL > fingers`, `forearmR > wristR > fingers`, and the
  weapon's own bones under the root. Pieces are generic (arms and gloves
  together), so it's done with bone hooks (node control functions).
- Hands only (`VRArms 0`, default): once a wrist is placed, its forearm
  bone is shortened along its length to a cuff at the wrist (thickness
  kept, so no spike), leaving the gloved hand with the game's finger
  animation. Sleeves on the `cuff` sockets stay hidden.
- Left hand free: the left wrist follows the left controller, and the
  fingers follow the left grip (CurlFinger): a slight relaxed curl let go,
  a fist squeezed. The gun animation's finger pose is replaced while the
  hand is free. Each finger bends toward the palm (+X of the left
  controller) about an axis across its knuckles; each joint gets an angle
  from the finger's base bone (pointer..pinky: knuckle 8..80, middle joint
  12..95, end joint 8..65 degrees; thumb 5..30, 5..45), so the pose is the
  same whatever the animation was doing. Bone directions come from the
  model's resting pose (GetBindPoseNodeTransform). Holding the gun with
  both hands, the fingers keep the game's own grip.
- Two-hand grip: left grip with the left hand within 20 cm of where Cate's
  left hand is on the weapon. The hand goes back onto the weapon and the
  gun turns about the right hand so the line between the hands keeps its
  direction (steadier aim, aim follows). Let go to free it. Each grab also
  teaches how the hand sits on the controller (saved to
  `run\vr_lefthand.txt`). Only the hand's turn is learnt; the wrist always
  sits `VRWristBack` cm (7) behind the controller, so the hand turns about
  your hand. Hand poses are kept as full bone matrices, since the left
  arm's bones may be mirrored.
- Bone matrices handed to node controls are in the object's own space
  (the engine starts the skeleton from the object's position and
  rotation), i.e. really-close space for the gun: not relative to the gun.
  Getting that wrong made the free left hand orbit the right one.
- A weapon whose model doesn't load gets the engine's stand-in
  (`models\default.ltb`); on a monitor it sits unseen at the camera, in VR
  it was a giant grey arrow when holstered. Hidden in VR; logged in
  `vr_game.log` ("wants model ..., loaded ...").
- Bones the game has shrunk to nothing (e.g. arms while holstered) are left
  alone: undoing a collapsed matrix blew the arms up into huge flat shapes.
- Note: `LTVector::Cross` returns b x a; VR code writes the cross product
  out.
- Next: open/close the free left hand (grip/trigger curl the fingers), tool animations
  (lock pick etc.), what the left hand does with the trigger and grip.

## Interface in the headset (first pass)

- Everything the game draws after the 3D world (HUD, subtitles, dialogue
  choices, in-game menus, cursor, console) is drawn into a transparent
  layer instead of the monitor picture, and shown over the world.
- While playing it stays put in front of you at eye height and glides back
  around when you turn your head well away. In-game menus, popups
  and message boxes appear on a panel where you're looking and stay put.
- Screen fades darken the whole headset view; the letterbox bars are off.
- The game runs at 1440x1080 (4:3) in VR (run_nolf2_vr.bat). Its menus
  were made for 4:3: menu items are placed by the width ratio in both
  directions (CLTGUICtrl::SetScale(GetXRatio())), so on a 16:9 size the
  bottom quarter of every menu page falls off the picture (Quit, the last
  options). 4:3 also keeps the HUD art from being stretched sideways.
  Picking a widescreen size in Display options brings the problem back.

## Wrist and gun HUD (first pass)

- Health and armour sit on the back of your left wrist like a watch: the
  panel lies along the wrist (turned with the controller) and fades in as
  you turn it toward you (fully shown within about 35 degrees, gone past
  about 60). Ammo floats by the left side of the gun, always facing you. The rest of the HUD (subtitles,
  choices, objectives, pickups...) stays on the lazy panel.
- They are the game's own HUD pieces (CHUDHealth, CHUDAmmo), unchanged:
  each works out the part of the screen it draws in (CHUDVRRect, built in
  its Update). In VR, CHUDMgr::Render hands them to
  CVRMgr::RenderHudItem, which asks the engine for a spare screen-sized
  canvas (LTVR_BeginLayerFromScreen), lets the item draw as usual, and the
  engine copies that part of the canvas into a small layer
  (LTVR_LAYER_WRIST / LTVR_LAYER_GUN, 512 x 256), as large as fits.
- If the hand isn't tracked (or the setting is 0) the piece goes back on
  the panel.
- Engine side: LTVR_SetLayerPose places a spot layer turned as the game
  says (instead of facing you) and partly see-through; the fade is done by
  scaling the layer's colour and coverage just before it's handed over.
- Settings: `VRWristHud` 1, `VRWristHudWatch` 1 (0 = always shown, facing
  you), `VRWristHudX/Y/Z` -3 / 0 / -9 cm from the left grip (right, up,
  forward along the controller; -X is the back of the hand),
  `VRWristHudSize` 9 cm;
  `VRGunHud` 1, `VRGunHudX/Y/Z` -6 / -2 / 6 cm from the right hand along
  the gun, `VRGunHudSize` 10 cm.
- The crosshair's layer is 1024 px square so the action text ("Talk",
  "Pick up"...) fits; the text sits where the game's 640 x 480 layout puts
  it relative to the crosshair (the game's own maths, scaled by the width
  ratio, pushed it off the layer on widescreen).

## Roadmap (after the first full play session, 20 min)

Working: stereo + 6DoF, controllers, smooth/snap turn, interface layer,
menus on a panel, gun in hand with per-weapon fit, aim from the gun,
crosshair on target, activation at the crosshair, hands only, free left
hand, two-hand grip. Holster = weapon "Holster", which has no model (the
engine stand-in is hidden in VR).

Next, in a suggested order:
1. Weapon ring around the right hand (first pass done: hold right grip).
2. HUD on the wrist (health / armour) and on the gun (ammo); subtitles and
   choices on a lazy panel (first pass done).
3. VR settings menu in the game's options (console is compiled out):
   first pass done, Options > VR (see below). Gun size stays with the
   in-headset adjust mode (it's per weapon).
4. ~~Laser pointer for menus.~~ Dropped for now: Tre finds the stick
   menu control solid.
5. ~~Recentre on a controller button~~ (not needed: holding the Meta
   button recentres through the headset); open/close left hand (first
   pass done: follows the left grip).
6. Scopes / zoom / binoculars in VR; tool animations (lock pick etc.).
Later: melee swinging (Tre wants it), comfort vignette, height
calibration and real crouch, vehicles, cutscene options, the shutdown
hang.

## VR settings page (first pass)

- Options > VR (TO2/ScreenVR.cpp): turning (smooth / snap), smooth turn
  speed, snap step, walk toward (head / left hand), world size (percent;
  100% = 60 game units per metre), HUD distance, HUD size, HUD stays put,
  health on wrist (off / like a watch / always), ammo on gun, arms (hands
  only / full arms), reset to defaults. Changes apply at once.
- Kept in `run\vr_settings.txt` (every VR setting, including the ones not
  on the page, so they can be edited by hand); read at start-up by
  CVRMgr::LoadSettings, written by CVRMgr::SaveSettings when you leave the
  page.
- The page's screen name is "ScreenGame" (ScreenEnum.h), so it uses the
  Game options page's layout entries (frame, fonts, positions, background)
  without needing new entries in the game's layout file. Help lines are
  the page's own text (GetHelpString override).

## Weapon ring (first pass)

- Hold the right grip: every weapon and gadget you can switch to (the
  game's own list and order) fans out in a ring centred on your hand,
  facing you, 36 cm across; more than 10 items use an inner and an outer
  ring. Move your hand toward one (nearest wins; the middle is a dead
  zone) and let go to switch. The game's own icons: "normal" for the
  highlighted one, "unselected" for the rest; its short name in the
  middle.
- Drawn into a spot layer (LTVR_LAYER_HAND); the crosshair is the other
  spot layer. The ring moves with you (kept relative to the camera).

