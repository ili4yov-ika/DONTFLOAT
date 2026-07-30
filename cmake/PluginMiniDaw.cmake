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
endfunction()
