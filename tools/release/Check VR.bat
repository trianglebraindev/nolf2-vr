@echo off
REM Checks that your headset and its OpenXR runtime (Virtual Desktop,
REM SteamVR, Meta Quest Link...) can be reached, without starting the game.
REM Details go to xrbridge.log in this folder.
cd /d "%~dp0"
"%~dp0VRProbe.exe"
echo.
echo VR check finished (code %errorlevel%). Details: xrbridge.log
pause
