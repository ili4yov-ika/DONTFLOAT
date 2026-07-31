# Deploy Qt runtime next to plugin binaries (Windows).
# Usage from build_windows_installer.bat after cmake --install:
#   cmake -DQT_ROOT=... -DPLUGIN_DIR=... [-DPLUGIN_BINARY=...] -P cmake/DeployPluginQt.cmake
#
# Hosts (REAPER, etc.) load our stub then *.impl.dll in-process. Qt looks for
# platforms/qwindows.dll relative to the host EXE unless the plugin sets
# QT_PLUGIN_PATH (see plugins/ui/dontfloat_qt_hosting.cpp). Either way the
# installer MUST ship platforms\qwindows.dll next to the impl module.

if(NOT WIN32)
    return()
endif()

if(NOT DEFINED QT_ROOT OR QT_ROOT STREQUAL "")
    message(FATAL_ERROR "DeployPluginQt: QT_ROOT is required")
endif()
if(NOT DEFINED PLUGIN_DIR OR PLUGIN_DIR STREQUAL "")
    message(FATAL_ERROR "DeployPluginQt: PLUGIN_DIR is required")
endif()
if(NOT EXISTS "${PLUGIN_DIR}")
    message(WARNING "DeployPluginQt: PLUGIN_DIR does not exist: ${PLUGIN_DIR}")
    return()
endif()

set(_windeployqt "${QT_ROOT}/bin/windeployqt.exe")
if(NOT EXISTS "${_windeployqt}")
    message(FATAL_ERROR "DeployPluginQt: windeployqt not found at ${_windeployqt}")
endif()

# Prefer an explicit binary if provided; otherwise pick first .clap/.vst3/.dll in the dir.
set(_binary "")
if(DEFINED PLUGIN_BINARY AND EXISTS "${PLUGIN_BINARY}")
    set(_binary "${PLUGIN_BINARY}")
else()
    file(GLOB _candidates
        "${PLUGIN_DIR}/*.clap"
        "${PLUGIN_DIR}/*.vst3"
        "${PLUGIN_DIR}/*_ui.impl.dll"
        "${PLUGIN_DIR}/*.impl.dll"
        "${PLUGIN_DIR}/*_ui.dll"
        "${PLUGIN_DIR}/*.dll"
    )
    list(LENGTH _candidates _count)
    if(_count EQUAL 0)
        message(WARNING "DeployPluginQt: no plugin binary in ${PLUGIN_DIR}")
        return()
    endif()
    list(GET _candidates 0 _binary)
endif()

message(STATUS "Deploying Qt for plugin: ${_binary} -> ${PLUGIN_DIR}")
execute_process(
    COMMAND "${_windeployqt}"
        --release
        --no-translations
        --no-compiler-runtime
        --dir "${PLUGIN_DIR}"
        "${_binary}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "windeployqt failed (${_rc}) for ${_binary}:\n${_err}\n${_out}")
endif()

if(NOT EXISTS "${PLUGIN_DIR}/Qt6Core.dll")
    message(FATAL_ERROR "DeployPluginQt: Qt6Core.dll missing in ${PLUGIN_DIR} after windeployqt")
endif()
if(NOT EXISTS "${PLUGIN_DIR}/platforms/qwindows.dll")
    message(FATAL_ERROR
        "DeployPluginQt: platforms/qwindows.dll missing in ${PLUGIN_DIR}\n"
        "Without it hosts report: no Qt platform plugin could be initialized")
endif()

message(STATUS "DeployPluginQt OK: Qt6Core.dll + platforms/qwindows.dll in ${PLUGIN_DIR}")
