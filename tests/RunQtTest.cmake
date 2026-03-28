# Запуск Qt-теста под CTest на Windows: подставляет Qt bin в PATH и plugins.
# Вызывается из CMakeLists: cmake -P RunQtTest.cmake с -DBINARY_DIR=... -DTARGET_NAME=... и т.д.

cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED BINARY_DIR OR NOT DEFINED TARGET_NAME OR NOT DEFINED QT_ROOT OR NOT DEFINED WORKING_DIR)
    message(FATAL_ERROR "RunQtTest.cmake: требуются BINARY_DIR, TARGET_NAME, QT_ROOT, WORKING_DIR")
endif()
if(NOT DEFINED CMAKE_COMMAND)
    message(FATAL_ERROR "RunQtTest.cmake: требуется CMAKE_COMMAND")
endif()

if(NOT DEFINED CMAKE_EXECUTABLE_SUFFIX)
    set(CMAKE_EXECUTABLE_SUFFIX "")
endif()

set(_exe "")

if(DEFINED CONFIG AND NOT CONFIG STREQUAL "")
    set(_try "${BINARY_DIR}/${CONFIG}/${TARGET_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
    if(EXISTS "${_try}")
        set(_exe "${_try}")
    endif()
endif()

if(_exe STREQUAL "")
    foreach(_cfg Debug Release RelWithDebInfo MinSizeRel)
        set(_try "${BINARY_DIR}/${_cfg}/${TARGET_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
        if(EXISTS "${_try}")
            set(_exe "${_try}")
            break()
        endif()
    endforeach()
endif()

if(_exe STREQUAL "")
    set(_try "${BINARY_DIR}/${TARGET_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
    if(EXISTS "${_try}")
        set(_exe "${_try}")
    endif()
endif()

if(_exe STREQUAL "" OR NOT EXISTS "${_exe}")
    message(FATAL_ERROR "RunQtTest.cmake: не найден исполняемый файл ${TARGET_NAME} в ${BINARY_DIR}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "PATH=${QT_ROOT}/bin;$ENV{PATH}"
        "QT_PLUGIN_PATH=${QT_ROOT}/plugins"
        "${_exe}"
    WORKING_DIRECTORY "${WORKING_DIR}"
    RESULT_VARIABLE _rc
)

if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "RunQtTest.cmake: тест завершился с кодом ${_rc}: ${_exe}")
endif()
