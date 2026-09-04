@echo off
chcp 65001 >nul
REM DONTFLOAT Windows installer build script
REM Requires: CMake, Qt6, NSIS
REM
REM Builds app + DAW plugins only (no tests / mini-DAW / plugin_tester).
REM After cmake --install, deploys Qt next to each plugin impl/UI:
REM   Qt6*.dll + platforms\qwindows.dll (required - hosts look next to the
REM   host EXE; ensureQtApplication points Qt at the plugin module dir and
REM   starts a Win32 timer so Qt events pump inside VST3/LV2 without exec()).
REM VST3: auto-detects C:\SDKs\vst3sdk; if SDK present, missing VST3 artifacts
REM are a hard error (do not ship installer without them).

setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."
set "BUILD_DIR=%PROJECT_ROOT%\build"
set "INSTALL_DIR=%BUILD_DIR%\install"
set "NSIS_SCRIPT=%SCRIPT_DIR%nsis_installer.nsi"

echo ========================================
echo DONTFLOAT - Windows Installer Builder
echo ========================================
echo.

set "MAKENSIS="
if exist "C:\Program Files (x86)\NSIS\makensis.exe" (
    set "MAKENSIS=C:\Program Files (x86)\NSIS\makensis.exe"
) else if exist "C:\Program Files\NSIS\makensis.exe" (
    set "MAKENSIS=C:\Program Files\NSIS\makensis.exe"
) else (
    REM Delayed expansion is required here: inside a block percent expansion
    REM happens at parse time - before `where` runs - and reads a stale exit code
    where makensis >nul 2>&1
    if !ERRORLEVEL! EQU 0 (
        for /f "delims=" %%i in ('where makensis 2^>nul') do set "MAKENSIS=%%i" & goto :makensis_ok
    )
)
:makensis_ok
if "%MAKENSIS%"=="" (
    echo [ERROR] NSIS ^(makensis^) not found!
    echo Install NSIS: https://nsis.sourceforge.io/Download
    echo Or add NSIS to PATH
    exit /b 1
)

set "CMAKE_CMD="
if exist "C:\Program Files\CMake\bin\cmake.exe" (
    set "CMAKE_CMD=C:\Program Files\CMake\bin\cmake.exe"
) else if exist "C:\Program Files (x86)\CMake\bin\cmake.exe" (
    set "CMAKE_CMD=C:\Program Files (x86)\CMake\bin\cmake.exe"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
    set "CMAKE_CMD=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
    set "CMAKE_CMD=C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
    set "CMAKE_CMD=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
) else (
    where cmake >nul 2>&1
    if !ERRORLEVEL! EQU 0 set "CMAKE_CMD=cmake"
)
if "%CMAKE_CMD%"=="" (
    echo [ERROR] CMake not found! Add CMake to PATH or install it.
    exit /b 1
)

REM Generate logo.ico for EXE icon if missing
if not exist "%PROJECT_ROOT%\resources\icons\logo.ico" (
    echo [0/5] Generating logo.ico from logo.svg...
    python "%SCRIPT_DIR%svg_to_ico.py" 2>nul
    if not exist "%PROJECT_ROOT%\resources\icons\logo.ico" (
        echo [WARN] logo.ico not created - EXE will use default icon. Install Inkscape + Pillow.
    )
)

echo [1/5] CMake configure...
cd /d "%PROJECT_ROOT%"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

REM Plugins ON; skip mini-DAW / plugin_tester so installer build does not compile tests/hosts.
REM CLAP and LV2 are off: only VST3 with ARA ships
set "PLUGIN_CMAKE_ARGS=-DDONTFLOAT_BUILD_PLUGINS=ON -DDONTFLOAT_BUILD_CLAP=OFF -DDONTFLOAT_BUILD_LV2=OFF -DDONTFLOAT_BUILD_VST3=ON -DDONTFLOAT_BUILD_MINI_DAW=OFF -DDONTFLOAT_BUILD_PLUGIN_TESTER=OFF"

REM Auto-detect Steinberg SDK if env not set (same default as CMakeLists.txt).
if not defined DONTFLOAT_VST3_SDK_ROOT (
    if exist "C:\SDKs\vst3sdk\CMakeLists.txt" set "DONTFLOAT_VST3_SDK_ROOT=C:\SDKs\vst3sdk"
)
if defined DONTFLOAT_VST3_SDK_ROOT (
    echo [INFO] VST3 SDK: %DONTFLOAT_VST3_SDK_ROOT%
    "%CMAKE_CMD%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="%INSTALL_DIR%" !PLUGIN_CMAKE_ARGS! "-DDONTFLOAT_VST3_SDK_ROOT=%DONTFLOAT_VST3_SDK_ROOT:"=%" "%PROJECT_ROOT%"
) else (
    echo [WARN] DONTFLOAT_VST3_SDK_ROOT not set and C:\SDKs\vst3sdk missing - VST3 plugins will NOT be built.
    "%CMAKE_CMD%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="%INSTALL_DIR%" !PLUGIN_CMAKE_ARGS! "%PROJECT_ROOT%"
)
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configure failed!
    exit /b 1
)

echo [2/5] Building app + plugins ^(not tests / mini-DAW^)...
REM Stub targets pull *_impl via add_dependencies.
REM CLAP and LV2 are off (see PLUGIN_CMAKE_ARGS above): their targets are not
REM generated at all, and naming them here failed the build outright.
set "BUILD_TARGETS=DONTFLOAT"
if defined DONTFLOAT_VST3_SDK_ROOT (
    set "BUILD_TARGETS=!BUILD_TARGETS! dontfloat_full_vst3 dontfloat_scratch_vst3 dontfloat_pitcher_vst3"
) else (
    echo [WARN] Skipping VST3 build targets
)
"%CMAKE_CMD%" --build . --config Release --target !BUILD_TARGETS!
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    exit /b 1
)

echo [3/5] Installing to %INSTALL_DIR%...
REM cmake --install only adds and overwrites. Without wiping the tree first,
REM files from an older build (renamed plugin, dropped format, Qt upgrade)
REM survive here and get packed into the installer.
if exist "%INSTALL_DIR%" rmdir /s /q "%INSTALL_DIR%"
"%CMAKE_CMD%" --install . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Install failed!
    exit /b 1
)

echo [4/5] Copying Qt dependencies ^(windeployqt^)...
if not exist "%INSTALL_DIR%\bin\DONTFLOAT.exe" (
    echo [ERROR] DONTFLOAT.exe not found in %INSTALL_DIR%\bin\
    exit /b 1
)
set "WINDEPLOYQT="
if exist "C:\Qt\6.9.3\msvc2022_64\bin\windeployqt.exe" set "WINDEPLOYQT=C:\Qt\6.9.3\msvc2022_64\bin\windeployqt.exe"
REM Older Qt only as a fallback - the plain second `if exist` overwrote 6.9.3 with 6.8.3
if not defined WINDEPLOYQT (
    if exist "C:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe" set "WINDEPLOYQT=C:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe"
)
if not defined WINDEPLOYQT (
    where windeployqt >nul 2>&1
    if !ERRORLEVEL! EQU 0 (
        for /f "delims=" %%i in ('where windeployqt 2^>nul') do set "WINDEPLOYQT=%%i" & goto :wdq_ok
    )
)
:wdq_ok
if not defined WINDEPLOYQT (
    echo [ERROR] windeployqt not found - installer would ship plugins without Qt and DAWs show empty UI
    exit /b 1
)

"%WINDEPLOYQT%" --release "%INSTALL_DIR%\bin\DONTFLOAT.exe"
if %ERRORLEVEL% NEQ 0 echo [WARN] windeployqt finished with error for app

REM Qt next to DAW plugins. Stub loads *.impl.dll via LoadLibraryEx(ALTERED_SEARCH_PATH).
REM platforms\qwindows.dll MUST sit next to the impl - otherwise REAPER shows:
REM   "no Qt platform plugin could be initialized"
set "QT_ROOT_DIR="
for %%I in ("%WINDEPLOYQT%") do set "QT_BIN=%%~dpI"
for %%I in ("%QT_BIN%..") do set "QT_ROOT_DIR=%%~fI"

call :deploy_plugin_qt "%INSTALL_DIR%\lib\clap" "%INSTALL_DIR%\lib\clap\dontfloat_clap.impl.dll" "CLAP"
if errorlevel 1 exit /b 1
call :deploy_plugin_qt "%INSTALL_DIR%\lib\lv2\dontfloat.lv2" "%INSTALL_DIR%\lib\lv2\dontfloat.lv2\dontfloat_ui.impl.dll" "LV2 Full"
if errorlevel 1 exit /b 1
call :deploy_plugin_qt "%INSTALL_DIR%\lib\lv2\dontfloat_scratch.lv2" "%INSTALL_DIR%\lib\lv2\dontfloat_scratch.lv2\dontfloat_scratch_ui.impl.dll" "LV2 Scratch"
if errorlevel 1 exit /b 1
call :deploy_plugin_qt "%INSTALL_DIR%\lib\lv2\dontfloat_pitcher.lv2" "%INSTALL_DIR%\lib\lv2\dontfloat_pitcher.lv2\dontfloat_pitcher_ui.impl.dll" "LV2 Pitcher"
if errorlevel 1 exit /b 1
if defined DONTFLOAT_VST3_SDK_ROOT (
    call :deploy_plugin_qt "%INSTALL_DIR%\lib\vst3\DONTFLOAT.vst3\Contents\x86_64-win" "%INSTALL_DIR%\lib\vst3\DONTFLOAT.vst3\Contents\x86_64-win\DONTFLOAT.vst3.impl.dll" "VST3 Full" required
    if errorlevel 1 exit /b 1
    call :deploy_plugin_qt "%INSTALL_DIR%\lib\vst3\DONTFLOAT Scratch.vst3\Contents\x86_64-win" "%INSTALL_DIR%\lib\vst3\DONTFLOAT Scratch.vst3\Contents\x86_64-win\DONTFLOAT Scratch.vst3.impl.dll" "VST3 Scratch" required
    if errorlevel 1 exit /b 1
    call :deploy_plugin_qt "%INSTALL_DIR%\lib\vst3\DONTFLOAT Pitcher.vst3\Contents\x86_64-win" "%INSTALL_DIR%\lib\vst3\DONTFLOAT Pitcher.vst3\Contents\x86_64-win\DONTFLOAT Pitcher.vst3.impl.dll" "VST3 Pitcher" required
    if errorlevel 1 exit /b 1
    if not exist "%INSTALL_DIR%\lib\vst3\DONTFLOAT.vst3\Contents\x86_64-win\DONTFLOAT.vst3" (
        echo [ERROR] VST3 stub missing after install - NSIS would ship empty VST3 section
        exit /b 1
    )
) else (
    echo [WARN] VST3 section will be empty in the installer ^(no SDK^)
)

echo [5/5] Creating NSIS installer...
cd /d "%SCRIPT_DIR%"
pushd "%INSTALL_DIR%" 2>nul
if %ERRORLEVEL% EQU 0 (
    set "INSTALL_ABS=%CD%"
    popd
) else (
    set "INSTALL_ABS=%INSTALL_DIR%"
)
REM Version comes from project(... VERSION ...) so it cannot drift
set "PRODUCT_VERSION="
for /f "tokens=3" %%v in ('findstr /b /c:"project(DONTFLOAT VERSION" "%PROJECT_ROOT%\CMakeLists.txt"') do set "PRODUCT_VERSION=%%v"
if "%PRODUCT_VERSION%"=="" (
    echo [ERROR] Version not found in CMakeLists.txt
    exit /b 1
)
echo Version: %PRODUCT_VERSION%
"%MAKENSIS%" /DBUILD_DIR="%INSTALL_ABS%" /DPRODUCT_VERSION=%PRODUCT_VERSION% "%NSIS_SCRIPT%"
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] NSIS installer creation failed!
    exit /b 1
)

echo.
echo ========================================
echo Done! Installer: %SCRIPT_DIR%DONTFLOAT_Setup.exe
echo ========================================

endlocal
exit /b 0

REM ---------------------------------------------------------------------------
REM Deploy Qt next to a plugin binary; fail if Qt6Core / qwindows missing.
REM Args: %1=PLUGIN_DIR  %2=PLUGIN_BINARY  %3=LABEL  [%4=required]
REM ---------------------------------------------------------------------------
:deploy_plugin_qt
set "DEP_DIR=%~1"
set "DEP_BIN=%~2"
set "DEP_LABEL=%~3"
set "DEP_REQUIRED=%~4"
if not exist "%DEP_BIN%" (
    if /I "%DEP_REQUIRED%"=="required" (
        echo [ERROR] %DEP_LABEL%: required binary missing: %DEP_BIN%
        echo         Rebuild plugins / check CMAKE_INSTALL_PREFIX layout.
        exit /b 1
    )
    echo   [skip] %DEP_LABEL%: %DEP_BIN% not installed
    exit /b 0
)
echo   Deploying Qt for %DEP_LABEL%...
"%CMAKE_CMD%" -DQT_ROOT="%QT_ROOT_DIR%" -DPLUGIN_DIR="%DEP_DIR%" -DPLUGIN_BINARY="%DEP_BIN%" -P "%PROJECT_ROOT%\cmake\DeployPluginQt.cmake"
if errorlevel 1 (
    echo [ERROR] DeployPluginQt.cmake failed for %DEP_LABEL%
    exit /b 1
)
if not exist "%DEP_DIR%\Qt6Core.dll" (
    echo [ERROR] Qt deploy failed for %DEP_LABEL%: Qt6Core.dll missing in %DEP_DIR%
    echo         Without Qt next to the plugin, DAWs show an empty UI / fail to load.
    exit /b 1
)
if not exist "%DEP_DIR%\platforms\qwindows.dll" (
    echo [ERROR] Qt platform plugin missing: %DEP_DIR%\platforms\qwindows.dll
    echo         REAPER error: "no Qt platform plugin could be initialized"
    exit /b 1
)
echo   OK %DEP_LABEL%: Qt6Core.dll + platforms\qwindows.dll
exit /b 0