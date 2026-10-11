@echo off
setlocal EnableDelayedExpansion
REM ============================================================
REM  Build the player package:  release\NOLF2-VR-<version>.zip
REM
REM  It holds a "VR" folder that players put inside their own NOLF2
REM  folder (see tools\release\README.txt). No game files go in it.
REM  The version is in the VERSION file: change it before a new release.
REM
REM  Also writes release\NOLF2-VR-<version>-symbols.zip: the .pdb files
REM  for this exact build. Keep it (don't publish it): with it, a
REM  player's crash report and .dmp can be read.
REM ============================================================
cd /d "%~dp0"
set /p VERSION=<VERSION
set "VERSION=%VERSION: =%"
if "%VERSION%"=="" (
    echo ERROR: the VERSION file is empty.
    exit /b 1
)
echo Making NOLF2 VR %VERSION%...

call "%~dp0build_vs2022.bat" || (
    echo.
    echo RELEASE FAILED: the build failed - tell Claude.
    exit /b 1
)

set "OUT=%~dp0build\OUT\Release"
set "REL=%~dp0release\NOLF2-VR-%VERSION%"
set "VRDIR=%REL%\VR"
if exist "%REL%" rmdir /S /Q "%REL%"
mkdir "%VRDIR%\gamedll" || exit /b 1
mkdir "%VRDIR%\defaults" || exit /b 1

REM --- The engine and the game's code ---
for %%F in (Lithtech.exe LTMsg.dll ServerDir.dll SndDrv.dll VRProbe.exe) do (
    copy /Y "%OUT%\%%F" "%VRDIR%\" >nul || (echo ERROR: missing %%F & exit /b 1)
)
for %%F in (CShell.dll Object.lto ClientFx.fxd CRes.dll SRes.dll) do (
    copy /Y "%OUT%\%%F" "%VRDIR%\gamedll\" >nul || (echo ERROR: missing %%F & exit /b 1)
)

REM --- The launcher, the readme, the starting gun fits ---
copy /Y "%~dp0tools\release\Play NOLF2 VR.bat" "%VRDIR%\" >nul || exit /b 1
copy /Y "%~dp0tools\release\Check VR.bat" "%VRDIR%\" >nul || exit /b 1
copy /Y "%~dp0tools\release\README.txt" "%VRDIR%\" >nul || exit /b 1
copy /Y "%~dp0tools\release\vr_guns.txt" "%VRDIR%\defaults\" >nul || exit /b 1

REM --- The Visual C++ runtime, next to the game (Microsoft allows this) ---
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -property installationPath`) do set "VSDIR=%%I"
)
set "CRTDIR="
if defined VSDIR (
    for /d %%D in ("%VSDIR%\VC\Redist\MSVC\*") do (
        if exist "%%D\x86\Microsoft.VC143.CRT\msvcp140.dll" set "CRTDIR=%%D\x86\Microsoft.VC143.CRT"
    )
)
if not defined CRTDIR (
    echo ERROR: couldn't find Visual Studio's C++ runtime files to include.
    exit /b 1
)
copy /Y "%CRTDIR%\msvcp140*.dll" "%VRDIR%\" >nul || exit /b 1
copy /Y "%CRTDIR%\vcruntime140*.dll" "%VRDIR%\" >nul || exit /b 1

REM --- D3DX9 (the DirectX SDK's redistributable) ---
set "D3DXCAB=%DXSDK_DIR%\Redist\Jun2010_d3dx9_43_x86.cab"
if not exist "%D3DXCAB%" (
    echo ERROR: "%D3DXCAB%" not found ^(the DirectX SDK June 2010^).
    exit /b 1
)
expand "%D3DXCAB%" -F:d3dx9_43.dll "%VRDIR%" >nul || exit /b 1
if not exist "%VRDIR%\d3dx9_43.dll" (
    echo ERROR: couldn't unpack d3dx9_43.dll.
    exit /b 1
)

REM --- The zips ---
set "ZIP=%~dp0release\NOLF2-VR-%VERSION%.zip"
set "SYMZIP=%~dp0release\NOLF2-VR-%VERSION%-symbols.zip"
if exist "%ZIP%" del "%ZIP%"
if exist "%SYMZIP%" del "%SYMZIP%"
powershell -NoProfile -Command "Compress-Archive -Path '%VRDIR%' -DestinationPath '%ZIP%'" || exit /b 1
powershell -NoProfile -Command "Compress-Archive -Path '%OUT%\*.pdb' -DestinationPath '%SYMZIP%'" || exit /b 1

echo.
echo Done:
echo   %ZIP%            ^(for players: upload this^)
echo   %SYMZIP%    ^(keep this, don't publish^)
endlocal
