@echo off
setlocal

REM ============================================================
REM  Runs the source-built engine on the retail NOLF2 data.
REM  Everything happens inside the run\ folder (see setup_run_dir.bat).
REM  build_vs2022.bat copies fresh game modules into run\gamedll.
REM  Anything after the script name is passed on to the engine
REM  (later settings win, e.g. "run_nolf2.bat +ScreenWidth 1920").
REM ============================================================

set "OUTDIR=%~dp0build\OUT\Release"

if not exist "%OUTDIR%\Lithtech.exe" (
    echo ERROR: build\OUT\Release\Lithtech.exe not found. Run build_vs2022.bat first.
    exit /b 1
)

call "%~dp0setup_run_dir.bat" || exit /b 1

echo Starting NOLF2 (source build)...
cd /d "%~dp0run"
"%OUTDIR%\Lithtech.exe" -windowtitle "NOLF2 (source build)" -rez Game.rez -rez Game2.rez -rez Sound.rez -rez Custom -rez Update_v1x3.rez -rez gamedll +Windowed 1 +ScreenWidth 1280 +ScreenHeight 720 +BitDepth 32 %*
set "RESULT=%errorlevel%"
echo Engine exited with code %RESULT%
endlocal & exit /b %RESULT%
