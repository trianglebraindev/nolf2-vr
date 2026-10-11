@echo off
setlocal
REM ============================================================
REM  NOLF2 VR - start the game in VR.
REM
REM  This "VR" folder goes inside your No One Lives Forever 2
REM  folder (next to GAME.REZ). Your original game isn't changed:
REM  VR keeps its own settings, saves and logs in this folder.
REM ============================================================
cd /d "%~dp0"
set "GAMEDIR=.."

REM --- The game's own files (from your copy of NOLF2, patched to 1.3) ---
set "MISSING="
for %%F in (GAME.REZ GAME2.REZ SOUND.REZ Engine.REZ) do (
    if not exist "%GAMEDIR%\%%F" set "MISSING=%%F"
)
if defined MISSING (
    echo.
    echo  Can't find the game: %MISSING% isn't in the folder above this one.
    echo.
    echo  Put this "VR" folder inside your No One Lives Forever 2 folder,
    echo  next to GAME.REZ, and start it again. See README.txt.
    echo.
    pause
    exit /b 1
)
if not exist "%GAMEDIR%\Update_v1x3.rez" (
    echo.
    echo  NOLF2 needs the 1.3 patch: Update_v1x3.rez isn't in your game folder.
    echo  Install the official 1.3 update, then start this again. See README.txt.
    echo.
    pause
    exit /b 1
)

REM --- First run: a few small things from your game folder ---
REM (The engine looks for these next to itself; the movies by folder name.)
if not exist "Engine.REZ" copy /Y "%GAMEDIR%\Engine.REZ" "Engine.REZ" >nul
if not exist "binkw32.dll" if exist "%GAMEDIR%\binkw32.dll" copy /Y "%GAMEDIR%\binkw32.dll" "binkw32.dll" >nul
if not exist "Movies" if exist "%GAMEDIR%\Movies" mklink /J "Movies" "%GAMEDIR%\Movies" >nul
if not exist "Profiles" mkdir "Profiles"
if not exist "Profiles\Default.prf" if exist "%GAMEDIR%\Profiles\Default.prf" copy /Y "%GAMEDIR%\Profiles\Default.prf" "Profiles\Default.prf" >nul
if not exist "autoexec.cfg" if exist "%GAMEDIR%\autoexec.cfg" copy /Y "%GAMEDIR%\autoexec.cfg" "autoexec.cfg" >nul
REM (How each gun sits in your hand: the tuned starting fits.)
if not exist "vr_guns.txt" copy /Y "defaults\vr_guns.txt" "vr_guns.txt" >nul

set "CUSTOM="
if exist "%GAMEDIR%\Custom" set "CUSTOM=-rez %GAMEDIR%\Custom"

start "" "%~dp0Lithtech.exe" -vr -windowtitle "NOLF2 VR" -rez %GAMEDIR%\Game.rez -rez %GAMEDIR%\Game2.rez -rez %GAMEDIR%\Sound.rez %CUSTOM% -rez %GAMEDIR%\Update_v1x3.rez -rez gamedll +Windowed 1 +ScreenWidth 1440 +ScreenHeight 1080 +BitDepth 32 +DisableHardwareCursor 1 %*
endlocal
