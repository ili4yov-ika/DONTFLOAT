# Развёртывание Qt DLL рядом с exe на Windows (windeployqt).
# Без этого GUI-приложение падает при запуске без Qt в PATH.

function(dontfloat_deploy_qt target)
    if(NOT WIN32)
        return()
    endif()

    if(NOT WINDEPLOYQT_EXECUTABLE)
        find_program(WINDEPLOYQT_EXECUTABLE
            NAMES windeployqt windeployqt.exe
            HINTS
                "${QT_ROOT_DIR}/bin"
                "${Qt6_DIR}/../../../bin"
        )
    endif()

    if(NOT WINDEPLOYQT_EXECUTABLE)
        message(WARNING "windeployqt not found; ${target} needs Qt bin on PATH to run")
        return()
    endif()

    if(MINGW)
        # MinGW: не передавать --debug/--release — windeployqt сам читает PE-зависимости.
        # Явный --debug ломает деплой плагинов (нет qwindowsd.dll в Qt for MinGW).
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-compiler-runtime
                "$<TARGET_FILE:${target}>"
            COMMENT "windeployqt (MinGW): ${target}"
            VERBATIM
        )
    else()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                "$<IF:$<CONFIG:Debug>,--debug,--release>"
                "$<TARGET_FILE:${target}>"
            COMMENT "windeployqt (MSVC): ${target}"
            VERBATIM
        )
    endif()
endfunction()
