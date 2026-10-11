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
| Right A / B | drop the magazine (manual reloading) or next ammo (game original) / holster | select / back |
| Right stick click | next ammo (manual reloading) or reload (game original) | |
| Left grip over your left shoulder | take a magazine (manual reloading, gun without one); push it into the gun to reload | |
| Left trigger | activate (use, talk, search) | select |
| Left grip + left trigger | snapshot (the trigger doesn't activate while the grip is held) | |
| Left X | mission objectives | |
| Left Y or menu button | pause menu | back |
| Left stick click | adjust gun; while looking through a scope, adjust the scope; while holding a magazine, adjust how it's held (again: done, saves) | |
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
  default. Clicked while looking through a scope, it adjusts the scope
  instead: the left stick moves the scope's line (and so the lens picture)
  up / down / left / right, saved as that weapon's last two numbers
  (scope up / right, cm, on top of `VRScopeHeight`).
- Gun size: the hands are the same size in every gun model (0.38 units
  wrist to middle knuckle, vr_models.log), so the guns' own proportions
  decide how big they look. The Sterling and silenced Sterling are
  modelled about as long breech-to-muzzle as the AK-47 (a much bigger
  gun) and looked "cannon-like" (Tre), so their built-in fit starts at
  size 8.5 instead of 12. The hands are part of each gun's model, so that
  shrank them too ("a little silly", Tre): the wrist bones are now grown
  by VRGunScale / the gun's size (CVRMgr::GetHandScale, in the hands'
  node control; the fingers hang off the wrist so they follow), keeping
  the hands one size whatever any gun's size.
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
- Left hand free: every weapon model has its own copy of Cate's left hand,
  posed for that item (resting finger curl 15..110 degrees, wrist bend
  5..35 degrees across models), so the free hand ignores the weapon's
  animation and uses the model's resting pose instead, placed the same way
  for every model (FindHandRest / GetFreeHandTransform): the hand's frame
  from its joints (wrist -> middle knuckle, pointer -> pinky knuckle, palm)
  is put on the left controller's grip pose (knuckles = -Up, across the
  knuckles = -Forward, palm = +Right), the wrist `VRWristBack` cm (7)
  behind the middle of the handle and `VRHandPalm` cm (3.5) toward the back
  of the hand. The forearm comes along rigidly (then trimmed to a cuff).
  Every finger bone (and the helper bones on the wrist) hangs off its
  parent as at rest, so the chain stays joined (an early version placed
  each joint at its own resting spot, which tore the fingers apart once a
  knuckle bent). The fingers then follow the left grip (CurlFinger): a
  slight relaxed curl let go, a fist squeezed; each finger bends about a
  hinge across its knuckles (base bone x palm), each joint set to an angle
  from the base bone (pointer..pinky: knuckle 8..75, middle joint 12..90,
  end joint 8..55 degrees; thumb 5..30, 5..45), turning about the hinge
  only so fingers never twist.
- The resting hand used is the Katana's for every weapon (Tre's pick: the
  nicest open hand). CVRMgr::LoadReferenceHand loads the Katana's
  first-person model once as an invisible object, reads its skeleton's
  resting pose by bone name, and removes it; GetRestPose then answers with
  those for every model (falling back to the model's own). The skin of
  every weapon's arm is the same mesh, so the same bone poses give the same
  hand.
- The thumb folds across the palm toward the little finger (and a little
  into the palm), its base swinging over too (0..25 degrees), instead of
  bending straight into the palm.
- One-handed items hide the left arm with animation keys ("hide model
  piece"): measured, StunGrenade and Compact Codebreaker hide
  polySurface10 (left arm), Crossbow hides polySurface7. In VR both arm
  pieces are kept visible (CVRMgr::KeepPieceVisible); every hide request
  is logged to vr_game.log as "Animation hides piece: weapon / piece".
  Holding the gun with both hands, the hand and fingers are the game's own.
  (The old learnt hand offset, vr_lefthand.txt, is no longer used.)
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
- The watch shows once its face is within about 50 degrees of facing you
  and hides past about 60 (no fade: a partly faded layer came out as a
  black box).
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
6. Scopes / zoom in VR (first pass done, see "Scopes"); tool animations
   (lock pick etc.) look right (Tre).
7. Fixes from weapon testing: Sterlings too big (smaller default), sniper
   scope off its tube (per-weapon scope adjust), laser dots / kitty eyes
   adrift (see "Effects on the gun"). (No binoculars in NOLF2: OVM_BINOC is
   defined but never used.)
8. Melee swinging (done: "feels great", Tre).
9. Comfort vignette (done; Tre has VR legs, it's for others).
10. Menus: the parked mouse cursor no longer steals the selection (see
    "Menus: the mouse steps aside").
11. Reloading by hand (first pass, see "Manual reloading").
Later: height calibration and real crouch, vehicles, cutscene options,
the shutdown hang.

## VR settings page (first pass)

- Options > VR (TO2/ScreenVR.cpp): turning (smooth / snap), smooth turn
  speed, snap step, walk toward (head / left hand), world size (percent;
  100% = 60 game units per metre), HUD distance, HUD size, HUD stays put,
  health on wrist (off / like a watch / always), ammo on gun, arms (hands
  only / full arms), reset to defaults, and "Testing: all weapons and
  gear" (runs the game's own "guns", "gear" and "ammo" cheats; single
  player, in a mission; the cheats' usual T-key message box is awkward in
  the headset). Changes apply at once.
- Kept in `run\vr_settings.txt` (every VR setting, including the ones not
  on the page, so they can be edited by hand); read at start-up by
  CVRMgr::LoadSettings, written by CVRMgr::SaveSettings when you leave the
  page.
- The page's screen name is "ScreenGame" (ScreenEnum.h), so it uses the
  Game options page's layout entries (frame, fonts, positions, background)
  without needing new entries in the game's layout file. Help lines are
  the page's own text (GetHelpString override).

## Scopes (first pass)

- Tre's picks: raise the scope to your eye to look through it (no button),
  and the magnified view shows only in the lens (the rest of your view
  stays normal).
- Any weapon whose scope mod has a zoom level (and the camera gadget):
  CVRMgr::UpdateScope, run each frame once the gun is placed. The scope's
  line runs along the aim line (which the adjust mode lines each gun up
  with) but `VRScopeHeight` cm (8) above it: the scope sits on top of the
  gun (measured on the crossbow: looking through it, the eye is 7 - 10 cm
  off the aim line). It zooms when an eye (either; each 3.2 cm from the
  head's centre) is within `VRScopeEye` cm (5) of the scope's line, behind
  the hand (up to 80 cm),
  and the gun points within about 30 degrees of where you look
  (CPlayerMgr::VR_SetZoom: the zoom level and its sounds only; the gun
  stays in your hand, the camera's FOV isn't touched, no 2D scope or camera
  frame overlays); it lets go a little further out (hysteresis). The lens
  sits on the scope's line `VRScopeDist` cm (8) in front of that eye. The
  scope's view looks from the lens toward where the shot lands (first
  solid thing along the aim line), so the reticle is true at any range. (First try
  put the eyepiece at a fixed spot above the hand and needed it within
  12 cm of the eye: with a crossbow at your face your hand is further than
  that, so it never came on.) vr_game.log says once per weapon whether it
  has a scope ("Scope: ...") and, while the gun is raised but not through
  the scope, why ("Scope check: ...").
- Magnification: the scope's view is the angle the lens takes up in your
  view (from `VRScopeSize` and `VRScopeDist`) divided by `VRScopeZoom` (4)
  times 1 / 2 / 4 for the game's zoom levels 1 / 2 / 3, so a level-1 scope
  (the crossbow's) is 4x. (First try used the game's own zoom angles,
  20 / 7 / 2 degrees: only about 1.4x on the lens, "the same view I
  already have".)
- Engine (LTVR_SetScopeView): one more view of the world, from the
  eyepiece along the gun, at that angle, drawn
  right after the eyes into LTVR_LAYER_SCOPE (512 x 512), leaving out
  really-close objects (gun, hands, overlays: g_bVRSkipReallyClose in
  tagnodes.cpp). Then the layer's alpha is set solid and a 256 x 256 mask
  (made once) multiplies it: alpha = round lens with a soft edge, colour =
  fine cross hairs with thicker posts toward the rim and a darker rim. The
  layer is placed at the eyepiece, turned with the gun, `VRScopeSize` cm
  (5.5) wide. (Quest screenshots don't show it: the lens is a separate
  layer the headset adds on top.)
- While scoped the world crosshair is hidden (the reticle replaces it) and
  the flat game's scope sway (which would turn your whole body) is off.
- Per weapon: the sniper rifle's scope is part of its model (no "scope"
  socket or bone, vr_models.log), so nothing in the game data says where
  its tube is. Each weapon's fit has a scope offset, set by eye with the
  scope adjust mode (left stick click while looking through it). While
  adjusting, the scope stays up wherever your eye is. The sniper's
  built-in offset is measured: sighting down its tube (the "Scope check"
  log lines while it wouldn't engage), the eye sat 14.4 cm up (+-1) and
  3 - 6 cm left of the aim line over 15 samples in two sessions, so it's
  6.4 cm up and 4.1 cm left of the crossbow's line.
- The monitor and snapshots: the headset puts the layers (scope lens,
  watch, ammo, crosshair, HUD) together itself, so neither the game's
  flat camera on the monitor nor the Quest's own captures had them. With
  `VRMirror` 1 (default) the monitor shows the middle of the left eye
  (the monitor's shape, 3/4 of the eye's height, centred on straight
  ahead) with every layer drawn on where the headset puts it: each
  layer's corners are taken through the left eye's pose and field of
  view, drawn in perspective, premultiplied, in the headset's order
  (VR_ComposeMirror). Left grip + left trigger (Tre's pick) saves that
  picture at the eye's own sharpness as `run\Snapshots\vr_<date>_<time>.png`
  (LTVR_TakeSnapshot, D3DXSaveSurfaceToFile; the file name goes to
  xrbridge.log, "Snapshot taken" with the weapon to vr_game.log). While
  the left grip is held the left trigger is unbound from "activate", and
  the "saved" message waits a few frames so it isn't in the picture.
- Which eye: the left, except while looking through a scope, when it's
  the eye at the scope (the game knows which, LTVR_SetMirrorEye). Tre's
  first scoped snapshot (left eye) had the lens edge-on at the right
  edge, because he sights with his right eye.
- Cat eyes (angry kitty) on target after the effects fix (Tre); the
  sniper scope's measured offset "felt good, lines up" (Tre).

## Manual reloading (first pass)

- Tre's design (after Titanfall 2 VR), on by default with "Game original"
  as the other choice (Options > VR > Reloading): A drops the magazine; a
  left grip with the hand over the left shoulder takes a fresh one; pushed
  into the gun it reloads. Leftover rounds in a dropped magazine go back
  to the reserve. Guns without a separate magazine: the same gesture, no
  magazine shown. In manual mode the right stick click is "next ammo"
  (A's old job); in game original the buttons are as before.
- Which guns (CClientWeapon::VR_IsReloadGun): more than one round a load,
  not a gadget, and a "clip" bone or a muzzle ("Flash"/"flash" socket) in
  the model. Logged once per weapon: "Reload: <weapon> reloads by hand /
  uses the game's reload (...)". From vr_models.log the "clip" bone is in
  the Beretta, Tommy gun, Sterling and silenced Sterling; the AK-47 and
  sniper rifle have unnamed bones (joint1 / joint2) that might be theirs.
- Game logic: the game fires on total ammo, not the magazine, and refills
  the magazine itself the moment it empties (DecrementAmmo) or when a
  weapon is re-enabled. In manual mode those refills are held off, and the
  trigger is blocked (a click; a one-time hint) while the magazine is out
  or empty. Dropping sets the gun's load to 0 (the total is unchanged, so
  the rounds stay in reserve); inserting is the game's instant reload,
  ReloadClip(no animation, forced, tell the server). The server refills
  its own count when it empties, so nothing changes there. Switching
  weapons still refills an empty gun (the game's own select).
- Visuals (guns with a "clip" bone): the gun's clip bone is shrunk to
  nothing while its magazine is out (a node control). The magazine in
  your hand and the one falling away are copies of the gun model with
  every bone shrunk to nothing except the clip bone, which is held at its
  rest pose, so the magazine sits at a known spot in the model: placed so
  that spot is at your left hand (turned as the hand: it hangs below the
  fist as in a gun held in that hand), or in the world where it left the
  gun (falling with gravity and a little tumble until it hits the ground,
  gone after 5 s). Clip-out / clip-in sounds are the weapon's own reload
  sounds 1 and 2.
- Insert: the held magazine within 10 cm of the gun's clip bone; without a
  clip bone, the left hand within 15 cm of the right.
- First test (Tre): works on most guns, but (1) the magazine in the hand
  hung at a fixed spot in front of the face, (2) the AK-47 and sniper
  showed nothing, (3) the ammo read-out kept the old count after a drop,
  (4) the infinite-ammo Tommy gun showed no count, (5) the shotgun
  wouldn't drop. Fixes:
  (1) Bone matrices in a node control already carry the object's own
  placement (the free left hand sets its wrist straight in really-close
  space), so setting the magazine bone to its rest pose dropped the
  object's placement. The copies now build the bone's matrix directly:
  skin x rest pose, the skin taking the rest pose's magazine point to the
  hand (really-close space) or to the falling point (world, times the
  gun's size).
  (2) Models without a "clip" bone: a hidden copy is stepped through the
  game's own "Reload" animation (24 steps) and the bone (not a hand's,
  arm's or the gun's) that travels furthest relative to the gun bone is
  the magazine, if it travels at least 0.4 model units (about a hand's
  width). Logged: "Reload: <weapon> - bones moving in its reload ...".
  (3) The HUD only redraws when told: drop and insert queue kHUDAmmo.
  (4) The game hides the count for infinite-ammo guns; with reloading by
  hand they show the magazine's count.
  (5) A could only drop while the weapon was idle; now anything but
  reloading / switching / put away, and a refusal is logged with the
  weapon's state.
- Second test (Tre): (1) faint white streaks either side of the gun when
  firing, (2) the AK-47's magazine turned oddly in the hand, (3) the
  shotgun "dropped" its pump. Measured / fixed:
  (1) Tracers start at the shot's fire position, which in VR is the
  controller's aim ray: inside the gun, right by your eyes, so each eye
  saw it in a different place. For your own first-person shots they now
  start at the gun's muzzle (CWeaponFX::CalcFirePos asks the weapon,
  IClientWeaponBase::VR_GetMuzzlePos); the shot itself is unchanged.
  (2) The copies were turned by the magazine bone's rest pose, which on
  the AK-47 is turned quite differently from how it sits in the gun. Now
  the bone's turn in the gun as you hold it is recorded while it's in,
  and the hand / falling copies use that (bone matrix = hand or gun turn
  x that turn, at the hand / falling point).
  (3) The reload sampling found (vr_game.log): AK-47 "trigger" 1.35 units,
  1.17 of it down (its magazine); sniper "joint2" 1.29, 1.24 down (its
  magazine); shotgun "trigger" 0.41 straight back (the pump). A magazine
  must now leave mostly downward (down >= half its travel), so the
  shotgun has none: nothing drops, and (Tre) the hand holds a stand-in,
  the Beretta's magazine.
- Third test (Tre): streaks gone, shotgun better; the Beretta's magazine
  is held right but the AK-47's and Sterling's lie flat across the hand.
  Holding a magazine "as it sits in the gun" only suits the Beretta (its
  magazine is in the pistol grip, along the fist); the AK-47's sits ahead
  of the trigger guard and the Sterling's goes in from the side. Now each
  magazine's long axis is turned to point straight down out of the fist,
  as the Beretta's does: the long axis is the way it leaves the gun in
  the reload animation (a magazine slides out along its length), now
  followed for "clip" bones too (VR_FindMagBone; logged as "leaves along
  (x y z)"), and VR_SetMagAlign turns that onto straight down (model
  space as the gun is held). The falling copy still keeps the gun's turn.
- Fourth test (Tre): worse; the Beretta's was now wrong too. The reload
  animation's most-moved direction isn't the magazine's length (the hand
  swings it about), so that guess is withdrawn. Back to "as it sits in the
  gun" (right for the Beretta), plus a per-weapon magazine hold set by eye:
  click the left stick while holding a magazine (ADJUST MAGAZINE): left
  stick slides it (right / forward), right grip + right stick raises it,
  right stick turns / tilts it, A / B roll it; the grip can be let go
  while adjusting, and it isn't loaded into the gun meanwhile. Saved per
  weapon as six more numbers in vr_guns.txt (slide cm, turn / tilt / roll
  degrees, about the hand's axes); Tre's settings for the AK-47,
  Sterling, Tommy gun and sniper become the built-in defaults (Tre's
  workup: held upright like a handle, the top just above the fist).
- The shoulder (CVRMgr::IsLeftHandAtShoulder): left of the head's middle,
  no further forward than the face, from about chest height up, within
  half a metre, in a frame turned by the head's heading only. While the
  gun wants a magazine, a left grip there doesn't grab the gun or pick up
  a body.

## Menus: the mouse steps aside

- Problem (Tre): in menus the mouse cursor sat in the middle of the screen
  (the game keeps it centred during play) and the item under it kept
  taking the selection from the sticks. Cause: the menus re-check "what's
  under the cursor" without the mouse moving: CBaseScreen::ForceMouseUpdate
  whenever a screen gets focus, and Windows' mouse-move messages that
  repeat the same position.
- Fix, no laser needed: in VR only a mouse that really moves (3+ pixels
  since the last message, CVRMgr::NoteMouseMove, called from
  CInterfaceMgr::OnMouseMove) picks items, and ForceMouseUpdate does
  nothing unless the mouse is in use. Any controller menu input
  (CVRMgr::SendKey) hands the menus back to the controllers and hides the
  cursor (CCursorMgr::Update); moving the mouse shows it again.

## Comfort vignette (first pass)

- Tre's picks: off by default; Options > VR > Comfort vignette Off / Light
  / Medium / Strong, and Vignette when: moving, turning, or both.
- Game (CVRMgr::UpdateVignette): the amount is the walk stick's push
  (GetMoveVector) and/or how hard you smooth turn (GetSmoothTurnAmount,
  the right stick as turning reads it), whichever is more; eased in over
  about 0.15 s and out over 0.4 s. Moving your real body and snap turns
  never bring it on; nor does the adjust mode or the weapon ring.
- Engine (LTVR_SetVignette, LTVR_VERSION 9; VR_VignetteEyes): a soft black
  ring drawn over each eye's world before the layers (HUD, crosshair, scope
  stay clear), centred on that eye's straight ahead, fully dark at 60 / 45
  / 32 degrees out (light / medium / strong) and clear to about half that.
  The monitor mirror shows it too.
- The VR settings page is now 16 rows and these menus don't scroll: after
  building, if the rows run past the page's bottom they're closed up evenly
  to fit (logged: "VR settings page: ...").

## Melee (first pass)

- Tre's picks: a real swing hits AND the trigger still does the game's
  own slash ("give players options"); the weapon's own damage whatever
  the speed; during a real swing the blade stays where your hand has it
  (no slash animation).
- Which weapons: ammo doing DT_SWORD or DT_MELEE damage (a VECTOR ammo),
  with a blade in the model: the "gun" bone (hilt) to the "MotionTrail"
  socket (tip), about 58 cm on the katana and tulwar (vr_models.log). The
  log says "Swing: <weapon> swings (blade N cm ...)" once per weapon.
- Each frame (CClientWeapon::VR_UpdateSwing) the hilt and tip are put in
  the world (model space -> the gun's placement in really-close space ->
  the world, as for the muzzle flash). When the tip moves faster than
  `VRSwingSpeed` (2.5 m/s; Options > VR > Sword swings: Off / Light 1.5 /
  Normal 2.5 / Hard 4), four points along the blade (35 / 60 / 80 / 100%
  of the way to the tip) are swept from last frame's place to this
  frame's, with the targeting's filters (not you, your hit box or the gun).
- A touch sends the game's own melee shot (VR_SwingFire: the fire sound
  and MID_WEAPON_FIRE, as CClientWeapon::Fire without the spread) from
  15 cm before the touch along the blade's motion; the server's own trace
  (the weapon's range) does the damage, sound and marks. In single player
  the client does no hit test of its own (only against a remote server),
  so the server's trace is what counts.
- Only while the weapon is idle (not mid-slash, reloading or switching),
  and no sooner than the weapon's fire delay (at least 0.3 s) after the
  last hit: the server refuses shots faster than m_nFireDelay.
- Hits on walls count too (the game's clang and marks). Logged: "Swing:
  <weapon> hit a character / a body / the world / an object (tip speed,
  where along the blade)".

## Effects on the gun

- Effects fixed to a socket or bone of the first-person model (the angry
  kitty's eyes on L_Eye / R_Eye, laser dots, glows: ClientFX with follow
  type UP_PVSOCKETATTACH / UP_PVNODEATTACH) took the socket's position in
  the model's own space and added only the model's position, not its
  rotation (ClientFxDLL BaseFx.cpp). Right while the gun never turns
  relative to the camera (the flat game); in VR it turns with your hand,
  so the effects hung at an unturned offset and drifted as you looked
  around. They now take the socket's full transform, as attached effects
  on other models always did.

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

