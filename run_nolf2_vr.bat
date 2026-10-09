@echo off
REM ============================================================
REM  Runs the game in VR (currently: the normal game shown on a
REM  large floating screen in the headset).
REM  Connect the Quest through Virtual Desktop (VDXR) first.
REM  VR log: run\xrbridge.log
REM ============================================================
call "%~dp0run_nolf2.bat" -vr +ScreenWidth 1920 +ScreenHeight 1080 %*
