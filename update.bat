@echo off
REM ============================================================
REM  Get the latest changes from GitHub, then build.
REM  This is the one command to run after Claude pushes a change.
REM ============================================================
cd /d "%~dp0"
echo Getting the latest changes from GitHub...
git pull --ff-only
if errorlevel 1 (
    echo.
    echo UPDATE FAILED - tell Claude what the message above says.
    exit /b 1
)
call "%~dp0build_vs2022.bat"
