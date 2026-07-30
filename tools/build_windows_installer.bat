@echo off
chcp 65001 >nul
REM DONTFLOAT Windows installer build script
REM Requires: CMake, Qt6, NSIS

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
    where makensis >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
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
    if %ERRORLEVEL% EQU 0 set "CMAKE_CMD=cmake"
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

set "PLUGIN_CMAKE_ARGS=-DDONTFLOAT_BUILD_PLUGINS=ON -DDONTFLOAT_BUILD_CLAP=ON -DDONTFLOAT_BUILD_LV2=ON -DDONTFLOAT_BUILD_VST3=ON"
if defined DONTFLOAT_VST3_SDK_ROOT (
    "%CMAKE_CMD%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="%INSTALL_DIR%" !PLUGIN_CMAKE_ARGS! "-DDONTFLOAT_VST3_SDK_ROOT=%DONTFLOAT_VST3_SDK_ROOT:"=%" "%PROJECT_ROOT%"
) else (
    echo [INFO] DONTFLOAT_VST3_SDK_ROOT is not set; VST3 target will be skipped.
    "%CMAKE_CMD%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="%INSTALL_DIR%" !PLUGIN_CMAKE_ARGS! "%PROJECT_ROOT%"
)
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configure failed!
    exit /b 1
)

echo [2/5] Building...
"%CMAKE_CMD%" --build . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    exit /b 1
)

echo [3/5] Installing to %INSTALL_DIR%...
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
if exist "C:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe" set "WINDEPLOYQT=C:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe"
if defined WINDEPLOYQT (
    "%WINDEPLOYQT%" --release "%INSTALL_DIR%\bin\DONTFLOAT.exe"
    if %ERRORLEVEL% NEQ 0 echo [WARN] windeployqt finished with error for app

    REM Qt рядом с DAW-плагинами — без этого host не загружает модули
    set "QT_ROOT_DIR="
    for %%I in ("%WINDEPLOYQT%") do set "QT_BIN=%%~dpI"
    for %%I in ("%QT_BIN%..") do set "QT_ROOT_DIR=%%~fI"

    if exist "%INSTALL_DIR%\lib\clap\dontfloat_clap.impl.dll" (
        echo   Deploying Qt for CLAP plugins...
        "%CMAKE_CMD%" -DQT_ROOT="%QT_ROOT_DIR%" -DPLUGIN_DIR="%INSTALL_DIR%\lib\clap" -DPLUGIN_BINARY="%INSTALL_DIR%\lib\clap\dontfloat_clap.impl.dll" -P "%PROJECT_ROOT%\cmake\DeployPluginQt.cmake"
    )
    if exist "%INSTALL_DIR%\lib\lv2\dontfloat.lv2\dontfloat_ui.impl.dll" (
        echo   Deploying Qt for LV2 Full UI...
        "%CMAKE_CMD%" -DQT_ROOT="%QT_ROOT_DIR%" -DPLUGIN_DIR="%INSTALL_DIR%\lib\lv2\dontfloat.lv2" -DPLUGIN_BINARY="%INSTALL_DIR%\lib\lv2\dontfloat.lv2\dontfloat_ui.impl.dll" -P "%PROJECT_ROOT%\cmake\DeployPluginQt.cmake"
    )
    if exist "%INSTALL_DIR%\lib\lv2\dontfloat_scratch.lv2\dontfloat_scratch_ui.impl.dll" (
        echo   Deploying Qt for LV2 Scratch UI...
        "%CMAKE_CMD%" -DQT_ROOT="%QT_ROOT_DIR%" -DPLUGIN_DIR="%INSTALL_DIR%\lib\lv2\dontfloat_scratch.lv2" -DPLUGIN_BINARY="%INSTALL_DIR%\lib\lv2\dontfloat_scratch.lv2\dontfloat_scratch_ui.impl.dll" -P "%PROJECT_ROOT%\cmake\DeployPluginQt.cmake"
    )
    if exist "%INSTALL_DIR%\lib\lv2\dontfloat_pitcher.lv2\dontfloat_pitcher_ui.impl.dll" (
        echo   Deploying Qt for LV2 Pitcher UI...
        "%CMAKE_CMD%" -DQT_ROOT="%QT_ROOT_DIR%" -DPLUGIN_DIR="%INSTALL_DIR%\lib\lv2\dontfloat_pitcher.lv2" -DPLUGIN_BINARY="%INSTALL_DIR%\lib\lv2\dontfloat_pitcher.lv2\dontfloat_pitcher_ui.impl.dll" -P "%PROJECT_ROOT%\cmake\DeployPluginQt.cmake"
    )
    if exist "%INSTALL_DIR%\lib\vst3\DONTFLOAT.vst3\Contents\x86_64-win\DONTFLOAT.vst3.impl.dll" (
        echo   Deploying Qt for VST3 Full...
        "%CMAKE_CMD%" -DQT_ROOT="%QT_ROOT_DIR%" -DPLUGIN_DIR="%INSTALL_DIR%\lib\vst3\DONTFLOAT.vst3\Contents\x86_64-win" -DPLUGIN_BINARY="%INSTALL_DIR%\lib\vst3\DONTFLOAT.vst3\Contents\x86_64-win\DONTFLOAT.vst3.impl.dll" -P "%PROJECT_ROOT%\cmake\DeployPluginQt.cmake"
    )
    if exist "%INSTALL_DIR%\lib\vst3\DONTFLOAT Scratch.vst3\Contents\x86_64-win\DONTFLOAT Scratch.vst3.impl.dll" (
        echo   Deploying Qt for VST3 Scratch...
        "%CMAKE_CMD%" -DQT_ROOT="%QT_ROOT_DIR%" -DPLUGIN_DIR="%INSTALL_DIR%\lib\vst3\DONTFLOAT Scratch.vst3\Contents\x86_64-win" -DPLUGIN_BINARY="%INSTALL_DIR%\lib\vst3\DONTFLOAT Scratch.vst3\Contents\x86_64-win\DONTFLOAT Scratch.vst3.impl.dll" -P "%PROJECT_ROOT%\cmake\DeployPluginQt.cmake"
    )
    if exist "%INSTALL_DIR%\lib\vst3\DONTFLOAT Pitcher.vst3\Contents\x86_64-win\DONTFLOAT Pitcher.vst3.impl.dll" (
        echo   Deploying Qt for VST3 Pitcher...
        "%CMAKE_CMD%" -DQT_ROOT="%QT_ROOT_DIR%" -DPLUGIN_DIR="%INSTALL_DIR%\lib\vst3\DONTFLOAT Pitcher.vst3\Contents\x86_64-win" -DPLUGIN_BINARY="%INSTALL_DIR%\lib\vst3\DONTFLOAT Pitcher.vst3\Contents\x86_64-win\DONTFLOAT Pitcher.vst3.impl.dll" -P "%PROJECT_ROOT%\cmake\DeployPluginQt.cmake"
    )
) else (
    echo [WARN] windeployqt not found - installer may lack Qt DLLs for app and plugins
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
"%MAKENSIS%" /DBUILD_DIR="%INSTALL_ABS%" "%NSIS_SCRIPT%"
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] NSIS installer creation failed!
    exit /b 1
)

echo.
echo ========================================
echo Done! Installer: %SCRIPT_DIR%DONTFLOAT_Setup.exe
echo ========================================

endlocal
