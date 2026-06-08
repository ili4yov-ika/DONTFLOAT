# Поиск Qt6 и общие настройки платформы (macOS / Linux).

if(APPLE)
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
    endif()
    message(STATUS "macOS deployment target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")

    set(_dontfloat_qt_hints "")
    foreach(_qt_hint IN ITEMS
        "$ENV{CMAKE_PREFIX_PATH}"
        "$ENV{QT_ROOT_DIR}"
        "$ENV{HOME}/Qt/6.9.3/macos"
        "$ENV{HOME}/Qt/6.8.3/macos"
        "/opt/homebrew/opt/qt@6"
        "/opt/homebrew/opt/qt"
        "/usr/local/opt/qt@6"
        "/usr/local/opt/qt"
    )
        if(_qt_hint AND EXISTS "${_qt_hint}/lib/cmake/Qt6")
            list(APPEND _dontfloat_qt_hints "${_qt_hint}")
        endif()
    endforeach()

    if(_dontfloat_qt_hints)
        list(REMOVE_DUPLICATES _dontfloat_qt_hints)
        list(PREPEND CMAKE_PREFIX_PATH ${_dontfloat_qt_hints})
        message(STATUS "macOS Qt hints: ${_dontfloat_qt_hints}")
    endif()
    unset(_dontfloat_qt_hints)
    unset(_qt_hint)
endif()

if(UNIX AND NOT APPLE)
    foreach(_qt_hint IN ITEMS
        "$ENV{CMAKE_PREFIX_PATH}"
        "$ENV{QT_ROOT_DIR}"
        "$ENV{HOME}/Qt/6.9.3/gcc_64"
        "$ENV{HOME}/Qt/6.8.3/gcc_64"
    )
        if(_qt_hint AND EXISTS "${_qt_hint}/lib/cmake/Qt6")
            list(PREPEND CMAKE_PREFIX_PATH "${_qt_hint}")
            message(STATUS "Linux Qt hint: ${_qt_hint}")
            break()
        endif()
    endforeach()
    unset(_qt_hint)
endif()
