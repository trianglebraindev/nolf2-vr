@echo off
REM ============================================================
REM  Runs the game in VR: the 3D world in stereo with head tracking;
REM  menus and movies on a large floating screen.
REM  World scale: add e.g. +VRUnitsPerMeter 55 to the line below.
REM  Connect the Quest through Virtual Desktop (VDXR) first.
REM  Controller layout and VR settings: docs\VR-DESIGN.md
REM  Logs: run\xrbridge.log (headset), run\vr_game.log (game side)
REM  If it crashes: run\crash_*.txt explains where; send that file.
REM ============================================================
REM  1440x1080 (4:3): the game's menus were made for 4:3 screens. On a
REM  widescreen size they scale the page up by the width, so the bottom
REM  of every menu (Quit, the last options) falls off the picture.
REM  The headset view itself is not affected by this size.
REM  +DisableHardwareCursor: the normal mouse pointer is drawn by Windows on
REM  top of the desktop, so it never reaches the headset. The game's own
REM  software cursor is drawn into the picture itself.
call "%~dp0run_nolf2.bat" -vr +ScreenWidth 1440 +ScreenHeight 1080 +DisableHardwareCursor 1 %*
