NOLF2 VR  -  a VR mod for No One Lives Forever 2
=================================================

Play NOLF2 in VR: head tracking with 6DoF, motion controls, guns in your
hands, reloading and throwing by hand, a wrist HUD, and more. Made for the
Quest 3 over Virtual Desktop; any PC VR headset with an OpenXR runtime
should work.

This is an unofficial fan project. It contains no game files: you need
your own copy of No One Lives Forever 2.


WHAT YOU NEED
-------------
- Windows 10 or 11 and a PC VR setup with an OpenXR runtime (Virtual
  Desktop, SteamVR, or Meta Quest Link), set as the active OpenXR runtime.
- No One Lives Forever 2, patched to version 1.3. This mod was built on
  the English (UK) CD release with the official 1.3 patch; any retail copy
  updated to 1.3 should work. Copies are available from:
  https://www.myabandonware.com/game/no-one-lives-forever-2-a-spy-in-h-a-r-m-s-way-3nv#download
- The Microsoft DirectX End-User Runtime is NOT needed: the one file it
  would add is included.


INSTALLING
----------
1. Unzip. You get a folder called "VR".
2. Put the "VR" folder inside your NOLF2 folder, next to GAME.REZ.
   For example:  C:\Games\No One Lives Forever 2\VR
3. Start your VR runtime (e.g. connect Virtual Desktop) and put the
   headset on.
4. Double-click "Play NOLF2 VR.bat" in the VR folder.

Your normal NOLF2 is not changed. The VR version keeps its own settings,
saves and logs in the VR folder.


SETTINGS
--------
In the game: Options > VR (turning, comfort, height, seated or standing,
HUD, left-handed play, reloading and throwing style, cutscenes and more)
and Options > Cheats (including level select).

Stand (or sit) as you'll play, then hold both stick clicks for a second to
set your height.


IF SOMETHING GOES WRONG
-----------------------
- "Check VR.bat" tests your headset connection without the game.
- Logs are in the VR folder: vr_game.log, xrbridge.log, and after a crash
  crash_<date>_<time>.txt and .dmp. Please include them when you report a
  problem.


CREDITS
-------
- No One Lives Forever 2, its game code (Monolith's SDK) and the LithTech
  Jupiter EX engine: Monolith Productions.
- The open-source engine project this builds on: James Lacey, Kyle
  Repinski (MWisBest) and Sandy Carter (bwrsandman).
- The fix for retail levels loading: the MatzeLoCal macOS port.
- OpenXR loader: the Khronos Group (Apache 2.0), with jsoncpp (MIT).
- DirectMusic headers: the Wine project (LGPL 2.1). zlib: Jean-loup Gailly
  and Mark Adler.
- The Microsoft Visual C++ runtime and D3DX9 files are Microsoft
  redistributables.
- NOLF2 VR: Tre (Triangle Brain Studio), with Claude.

This is an unofficial, free fan project, not affiliated with Monolith,
Warner Bros. or Activision.
