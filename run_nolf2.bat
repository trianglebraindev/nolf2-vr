@echo off
setlocal

REM ============================================================
REM  Runs the source-built engine against the retail NOLF2 data.
REM
REM  - The retail install is only READ, never modified.
REM  - Our built game DLLs are copied into run\gamedll and loaded
REM    as the last resource folder, so they replace the retail ones.
REM  - Settings are kept in run\ (not the retail autoexec.cfg).
REM ============================================================

set "NOLF2DIR=D:\Games\No One Lives Forever 2"
set "OUTDIR=%~dp0build\OUT\Release"
set "RUNDIR=%~dp0run"

if not exist "%OUTDIR%\Lithtech.exe" (
    echo ERROR: build\OUT\Release\Lithtech.exe not found. Run build_vs2022.bat first.
    exit /b 1
)
if not exist "%NOLF2DIR%\GAME.REZ" (
    echo ERROR: retail NOLF2 not found at "%NOLF2DIR%"
    exit /b 1
)

if not exist "%RUNDIR%\gamedll" mkdir "%RUNDIR%\gamedll"
for %%F in (CShell.dll Object.lto CRes.dll SRes.dll ClientFx.fxd) do (
    copy /Y "%OUTDIR%\%%F" "%RUNDIR%\gamedll\" >nul || (echo ERROR: could not copy %%F & exit /b 1)
)

REM First run only: start from the retail settings.
if not exist "%RUNDIR%\autoexec.cfg" copy /Y "%NOLF2DIR%\autoexec.cfg" "%RUNDIR%\autoexec.cfg" >nul

echo Starting NOLF2 (source build)...
cd /d "%NOLF2DIR%"
"%OUTDIR%\Lithtech.exe" -windowtitle "NOLF2 (source build)" -config "%RUNDIR%\autoexec.cfg" -display "%RUNDIR%\display.cfg" -rez Game.rez -rez Game2.rez -rez Sound.rez -rez custom -rez Update_v1x3.rez -rez "%RUNDIR%\gamedll" +Windowed 1 +ScreenWidth 1280 +ScreenHeight 720 +BitDepth 32
set "RESULT=%errorlevel%"
echo Engine exited with code %RESULT%
endlocal & exit /b %RESULT%
