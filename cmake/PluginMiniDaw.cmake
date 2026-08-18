# Minimal in-process plugin hosts ("mini-DAW") for each format and product kind.
#
# Each target loads an audio file (default tests/midi/test_1.wav), instantiates
# the DONTFLOAT plugin for its product, streams the file through the plugin, and
# can host the plugin editor (--gui). Depends on helpers from PluginProducts.cmake
# (_dontfloat_product_index) and PluginPitchUi.cmake (dontfloat_link_plugin_ui).

# Create one mini-DAW executable for a given format/product and register a
# headless CTest that runs it on tests/midi/test_1.wav.
function(_dontfloat_add_mini_daw_target target kind format_tag)
    # Remaining args: extra source files (the format host main + wrapper impl).
    set(_sources ${ARGN})
    _dontfloat_product_index(${kind} _index)

    add_executable(${target} ${_sources})
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_compile_definitions(${target} PRIVATE DONTFLOAT_PLUGIN_PRODUCT_INDEX=${_index})
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/mini_daw
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/clap
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/lv2
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/core
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(${target} PRIVATE dontfloat_plugin_core)
    # Pulls in Qt (Widgets/Multimedia/Concurrent), the product editor sources,
    # AudioFileService and WavWriter, qm-dsp and Rubber Band.
    dontfloat_link_plugin_ui(${target} ${kind})

    # Headless CTest: run the mini-DAW on test_1.wav for a few seconds.
    set(_run_args --headless --seconds 4 --no-output
        --input ${CMAKE_CURRENT_SOURCE_DIR}/tests/midi/test_1.wav)
    if(WIN32 AND DEFINED QT_ROOT_DIR)
        add_test(NAME ${target}
            COMMAND ${CMAKE_COMMAND}
                -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
                -DCONFIG=$<CONFIG>
                -DTARGET_NAME=${target}
                -DQT_ROOT=${QT_ROOT_DIR}
                -DWORKING_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DCMAKE_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}
                -DCMAKE_COMMAND=${CMAKE_COMMAND}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQtTest.cmake
        )
    else()
        add_test(NAME ${target} COMMAND ${target} ${_run_args})
        set_tests_properties(${target} PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
    set_tests_properties(${target} PROPERTIES
        LABELS "plugins;mini-daw;${format_tag};${kind}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endfunction()

function(dontfloat_add_mini_daw kind)
    if(DONTFLOAT_BUILD_CLAP)
        _dontfloat_add_mini_daw_target(mini_daw_clap_${kind} ${kind} clap
            tools/mini_daw/clap_mini_daw.cpp
            plugins/clap/dontfloat_clap_plugin_impl.cpp
            plugins/clap/clap_minimal.h
        )
    endif()

    if(DONTFLOAT_BUILD_LV2)
        _dontfloat_add_mini_daw_target(mini_daw_lv2_${kind} ${kind} lv2
            tools/mini_daw/lv2_mini_daw.cpp
            plugins/lv2/dontfloat_lv2_plugin_impl.cpp
            plugins/lv2/lv2_minimal.h
        )
    endif()

    if(DONTFLOAT_BUILD_VST3)
        # Core-session host (realtime VST3 module requires the Steinberg SDK).
        _dontfloat_add_mini_daw_target(mini_daw_vst3_${kind} ${kind} vst3
            tools/mini_daw/vst3_mini_daw.cpp
        )
    endif()
endfunction()

function(dontfloat_add_all_mini_daws)
    foreach(_kind IN LISTS _DONTFLOAT_PRODUCT_KINDS)
        dontfloat_add_mini_daw(${_kind})
    endforeach()
    dontfloat_add_mini_daw_gui()
endfunction()

# Единое GUI-окно мини-DAW (макет MARKDOWN/example_window_minidaw.svg).
#
# В отличие от headless-целей выше, плагин здесь не влинкован: формат и редакция
# выбираются в выпадающих списках, а модуль грузится в рантайме (LoadLibrary),
# как это делает настоящая DAW. Поэтому цель не зависит ни от одного продукта.
function(dontfloat_add_mini_daw_gui)
    if(NOT WIN32)
        # Рантайм-загрузка модулей реализована через LoadLibrary
        return()
    endif()

    add_executable(dontfloat_mini_daw WIN32
        tools/mini_daw/mini_daw_gui_main.cpp
        tools/mini_daw/mini_daw_window.cpp
        tools/mini_daw/mini_daw_window.h
        tools/mini_daw/mini_daw_player.cpp
        tools/mini_daw/mini_daw_player.h
        tools/mini_daw/mini_daw_plugin_host.cpp
        tools/mini_daw/mini_daw_plugin_host.h
        tools/plugin_tester/plugin_host_probe.cpp
        tools/plugin_tester/plugin_host_probe.h
        src/audiofileservice.cpp
        # Огибающая волны и тактовая сетка дорожки — те же, что в приложении
        src/pianoroll_engine.cpp
        # Тестовый хост — в оформлении DONTFLOAT (как главное окно)
        plugins/ui/dontfloat_plugin_theme.cpp
        plugins/ui/dontfloat_plugin_theme.h
    )
    set_target_properties(dontfloat_mini_daw PROPERTIES
        AUTOMOC ON
        WIN32_EXECUTABLE TRUE
    )
    target_compile_features(dontfloat_mini_daw PRIVATE cxx_std_17)
    target_include_directories(dontfloat_mini_daw PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/mini_daw
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/plugin_tester
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/clap
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/lv2
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/core
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(dontfloat_mini_daw PRIVATE
        dontfloat_plugin_core
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Multimedia
    )
    target_compile_definitions(dontfloat_mini_daw PRIVATE
        "DONTFLOAT_PLUGIN_BUILD_ROOT=\"${CMAKE_BINARY_DIR}\""
    )

    # VST3-хостинг: sdk_hosting даёт загрузчик модулей и host-классы, но
    # платформенный module_win32.cpp хост подключает сам (так же в примерах SDK)
    if(DONTFLOAT_BUILD_VST3)
        dontfloat_ensure_vst3_sdk()
    endif()
    if(TARGET sdk_hosting)
        # Загрузчик модулей SDK не используем: LoadLibrary + GetPluginFactory
        # делаем сами (кэш модулей общий с CLAP/LV2, см. mini_daw_plugin_host.cpp)
        target_include_directories(dontfloat_mini_daw PRIVATE ${DONTFLOAT_VST3_SDK_ROOT})
        target_link_libraries(dontfloat_mini_daw PRIVATE sdk_hosting)
        target_compile_definitions(dontfloat_mini_daw PRIVATE DONTFLOAT_HAS_VST3_SDK)
    else()
        message(STATUS "mini-DAW: VST3 SDK недоступен — VST3-хостинг выключен")
    endif()
    if(MSVC)
        target_link_options(dontfloat_mini_daw PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
    endif()
    dontfloat_deploy_qt(dontfloat_mini_daw)

    # Самопроверка того же рантайм-пути без окна: декод → загрузка модуля →
    # редактор в скрытое нативное окно → блоки через process().
    foreach(_test_format clap lv2 vst3)
        if((_test_format STREQUAL "clap" AND NOT DONTFLOAT_BUILD_CLAP)
           OR (_test_format STREQUAL "lv2" AND NOT DONTFLOAT_BUILD_LV2)
           OR (_test_format STREQUAL "vst3" AND NOT TARGET sdk_hosting))
            continue()
        endif()
        foreach(_test_kind IN LISTS _DONTFLOAT_PRODUCT_KINDS)
            set(_test_name mini_daw_gui_${_test_format}_${_test_kind})
            add_test(NAME ${_test_name}
                COMMAND dontfloat_mini_daw --selftest
                    --format ${_test_format}
                    --product ${_test_kind}
                    --seconds 2
                    --input ${CMAKE_CURRENT_SOURCE_DIR}/tests/midi/test_1.wav
            )
            set_tests_properties(${_test_name} PROPERTIES
                LABELS "plugins;mini-daw;gui;${_test_format};${_test_kind}"
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                ENVIRONMENT "DONTFLOAT_PLUGIN_BUILD_ROOT=${CMAKE_BINARY_DIR}"
            )
        endforeach()
    endforeach()
endfunction()
