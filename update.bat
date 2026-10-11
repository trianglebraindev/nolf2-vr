@echo off
REM ============================================================
REM  Get the latest changes from GitHub, then build.
REM  This is the one command to run after Claude pushes a change.
REM ============================================================
cd /d "%~dp0"
REM The game must be closed: the build copies its files into run\, and
REM Windows won't let them be replaced while the game has them open.
tasklist /FI "IMAGENAME eq Lithtech.exe" 2>nul | find /I "Lithtech.exe" >nul
if not errorlevel 1 (
    echo.
    echo The game is still running ^(Lithtech.exe^). Close it - check Task
    echo Manager if you can't see it - then run update.bat again.
    exit /b 1
)
echo Getting the latest changes from GitHub...
git pull --ff-only
if errorlevel 1 (
    echo.
    echo UPDATE FAILED - tell Claude what the message above says.
    exit /b 1
)
call "%~dp0build_vs2022.bat"
