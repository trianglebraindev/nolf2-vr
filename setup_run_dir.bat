@echo off
setlocal

REM ============================================================
REM  Prepares the "run" folder the source-built engine runs from.
REM
REM  The engine runs with run\ as its working folder, NOT the
REM  retail folder, because:
REM   - the game prefers a loose "Game\ClientFX.fxd" in the working
REM     folder over its own, and the retail folder has the RETAIL
REM     one (unpacked by the NOLF2 tools). Loading the retail effects
REM     DLL alongside our game code crashes at the main menu.
REM   - the engine writes settings, profiles and saves into its
REM     working folder; this keeps all of that out of the retail
REM     install.
REM
REM  The big retail files are HARD LINKS (no copy, no extra disk
REM  space, read-only use) and Movies/Custom are folder junctions.
REM  Neither needs admin rights. The retail install is not changed.
REM ============================================================

set "NOLF2DIR=D:\Games\No One Lives Forever 2"
set "RUNDIR=%~dp0run"

if not exist "%NOLF2DIR%\GAME.REZ" (
    echo ERROR: retail NOLF2 not found at "%NOLF2DIR%"
    exit /b 1
)

if not exist "%RUNDIR%" mkdir "%RUNDIR%"
if not exist "%RUNDIR%\gamedll" mkdir "%RUNDIR%\gamedll"

for %%F in (GAME.REZ GAME2.REZ SOUND.REZ Update_v1x3.rez Engine.REZ binkw32.dll) do (
    if not exist "%RUNDIR%\%%F" (
        mklink /H "%RUNDIR%\%%F" "%NOLF2DIR%\%%F" >nul
        if errorlevel 1 (echo ERROR: could not link %%F & exit /b 1)
    )
)

for %%D in (Movies Custom) do (
    if not exist "%RUNDIR%\%%D" (
        mklink /J "%RUNDIR%\%%D" "%NOLF2DIR%\%%D" >nul
        if errorlevel 1 (echo ERROR: could not link the %%D folder & exit /b 1)
    )
)

REM Player profiles live in run\Profiles. New profiles are built from the
REM retail Default.prf template, which the game reads as a loose file.
if not exist "%RUNDIR%\Profiles" mkdir "%RUNDIR%\Profiles"
if not exist "%RUNDIR%\Profiles\Default.prf" copy /Y "%NOLF2DIR%\Profiles\Default.prf" "%RUNDIR%\Profiles\Default.prf" >nul

if not exist "%RUNDIR%\autoexec.cfg" copy /Y "%NOLF2DIR%\autoexec.cfg" "%RUNDIR%\autoexec.cfg" >nul

exit /b 0
