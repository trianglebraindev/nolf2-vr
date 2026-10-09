@echo off
REM ============================================================
REM  Headset plumbing test: shows a floating test screen in VR.
REM  Connect the Quest through Virtual Desktop (VDXR) first.
REM  The log is written to run\xrbridge.log.
REM ============================================================
if not exist "%~dp0build\OUT\Release\VRProbe.exe" (
    echo ERROR: VRProbe.exe not built yet. Run update.bat or build_vs2022.bat first.
    exit /b 1
)
if not exist "%~dp0run" mkdir "%~dp0run"
cd /d "%~dp0run"
"%~dp0build\OUT\Release\VRProbe.exe"
echo VR probe exited with code %errorlevel%
