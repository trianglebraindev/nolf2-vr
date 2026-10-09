@echo off
setlocal

REM ============================================================
REM  NOLF2 VR project - engine + game build (VS2022, Win32)
REM
REM    build_vs2022.bat          -> Release build
REM    build_vs2022.bat clean    -> delete the build folder first, then build
REM
REM  Screen shows progress. The full log goes to build_log.txt.
REM  Finished files land in build\OUT\Release (exe) and the
REM  game DLL folders under build\NOLF2.
REM ============================================================

cd /d "%~dp0"

if /I "%~1"=="clean" (
    if exist build (
        echo Deleting old build folder...
        rmdir /S /Q build
    )
)

if not defined DXSDK_DIR (
    echo ERROR: DXSDK_DIR is not set. The DirectX SDK June 2010 is not installed,
    echo or this window was opened before it was installed. Open a new window.
    exit /b 1
)

set "CMAKE="
where cmake >nul 2>nul && set "CMAKE=cmake"
if not defined CMAKE (
    for %%E in (Community Professional Enterprise BuildTools) do (
        if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%E\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
            set "CMAKE=%ProgramFiles%\Microsoft Visual Studio\2022\%%E\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        )
    )
)
if not defined CMAKE (
    echo ERROR: CMake not found. Re-run the Visual Studio Installer and make sure
    echo "C++ CMake tools for Windows" is ticked.
    exit /b 1
)

call "%~dp0setup_run_dir.bat" || exit /b 1

echo.
echo [1/2] Configuring...  (details in build_configure_log.txt)
"%CMAKE%" -S . -B build -G "Visual Studio 17 2022" -A Win32 -DCMAKE_POLICY_VERSION_MINIMUM=3.5 > build_configure_log.txt 2>&1
if errorlevel 1 (
    echo.
    echo CONFIGURE FAILED - tell Claude, it will read build_configure_log.txt
    exit /b 1
)

echo [2/2] Building... text will scroll. Lots of "warning" lines are normal.
echo.
"%CMAKE%" --build build --config Release -- /m /nologo /v:minimal "/flp:logfile=%~dp0build_log.txt;verbosity=normal" "/flp1:logfile=%~dp0build_errors.txt;errorsonly"
set "RESULT=%errorlevel%"

echo.
if "%RESULT%"=="0" (
    echo ============================================================
    echo  BUILD SUCCEEDED
    echo ============================================================
) else (
    echo ============================================================
    echo  BUILD FAILED - tell Claude, it will read build_log.txt
    echo ============================================================
)
endlocal & exit /b %RESULT%
