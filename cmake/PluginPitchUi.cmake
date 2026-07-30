# Shared plugin UI sources for VST3 / CLAP / LV2 product targets.

set(DONTFLOAT_PLUGIN_SHELL_SOURCES
    plugins/ui/dontfloat_qt_hosting.cpp
    plugins/ui/dontfloat_qt_hosting.h
    plugins/ui/dontfloat_plugin_editor_shell.cpp
    plugins/ui/dontfloat_plugin_editor_shell.h
    plugins/ui/dontfloat_track_tool_editor.cpp
    plugins/ui/dontfloat_track_tool_editor.h
)

set(DONTFLOAT_PLUGIN_SCRATCH_UI_SOURCES
    plugins/ui/dontfloat_scratch_editor.cpp
    plugins/ui/dontfloat_scratch_editor.h
    src/waveformview.cpp
    src/waveformcolors.cpp
    src/beatvisualizer.cpp
    src/bpmanalyzer.cpp
    src/timestretchprocessor.cpp
    src/markerengine.cpp
    src/timeutils.cpp
    src/audiofileservice.cpp
    src/rubberband_offline.cpp
    src/wavwriter.cpp
    include/waveformview.h
    include/waveformcolors.h
    include/beatvisualizer.h
    include/bpmanalyzer.h
    include/timestretchprocessor.h
    include/markerengine.h
    include/timeutils.h
    include/audiofileservice.h
    include/rubberband_offline.h
    include/wavwriter.h
)

set(DONTFLOAT_PLUGIN_FULL_ONLY_UI_SOURCES
    plugins/ui/dontfloat_full_editor.cpp
    plugins/ui/dontfloat_full_editor.h
)

set(DONTFLOAT_PLUGIN_PITCH_UI_SOURCES
    plugins/ui/dontfloat_pitch_editor.cpp
    plugins/ui/dontfloat_pitch_editor.h
    src/pitchgridwidget.cpp
    src/pianoroll_engine.cpp
    src/pitchdetector.cpp
    src/pitchcorrection.cpp
    src/keyanalyzer.cpp
    src/keyselectionmenu.cpp
    src/notepreviewplayer.cpp
    src/timestretchprocessor.cpp
    src/markerengine.cpp
    src/rubberband_offline.cpp
    include/pitchgridwidget.h
    include/pianoroll_engine.h
    include/pitchdetector.h
    include/pitchcorrection.h
    include/keyanalyzer.h
    include/keyselectionmenu.h
    include/notepreviewplayer.h
    include/timestretchprocessor.h
    include/markerengine.h
    include/rubberband_offline.h
)

function(_dontfloat_link_plugin_ui_common target_name)
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/core
    )
    target_link_libraries(${target_name} PRIVATE
        dontfloat_plugin_core
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::Concurrent
    )
    if(TARGET qm_dsp)
        target_link_libraries(${target_name} PRIVATE qm_dsp)
        target_compile_definitions(${target_name} PRIVATE USE_MIXXX_QM_DSP)
    endif()
    dontfloat_link_rubberband(${target_name})
endfunction()

function(dontfloat_link_plugin_ui target_name product_kind)
    if(product_kind STREQUAL "full")
        set(_product_index 0)
    elseif(product_kind STREQUAL "scratch")
        set(_product_index 1)
    elseif(product_kind STREQUAL "pitcher")
        set(_product_index 2)
    else()
        message(FATAL_ERROR "Unknown plugin product kind: ${product_kind}")
    endif()

    target_compile_definitions(${target_name} PRIVATE DONTFLOAT_PLUGIN_PRODUCT_INDEX=${_product_index})
    target_sources(${target_name} PRIVATE ${DONTFLOAT_PLUGIN_SHELL_SOURCES})

    if(product_kind STREQUAL "full")
        target_sources(${target_name} PRIVATE
            ${DONTFLOAT_PLUGIN_SCRATCH_UI_SOURCES}
            ${DONTFLOAT_PLUGIN_FULL_ONLY_UI_SOURCES}
            ${DONTFLOAT_PLUGIN_PITCH_UI_SOURCES}
        )
    elseif(product_kind STREQUAL "scratch")
        target_sources(${target_name} PRIVATE ${DONTFLOAT_PLUGIN_SCRATCH_UI_SOURCES})
    elseif(product_kind STREQUAL "pitcher")
        target_sources(${target_name} PRIVATE ${DONTFLOAT_PLUGIN_PITCH_UI_SOURCES})
        target_sources(${target_name} PRIVATE
            src/audiofileservice.cpp
            src/timeutils.cpp
            src/wavwriter.cpp
            include/audiofileservice.h
            include/timeutils.h
            include/wavwriter.h
        )
    endif()

    _dontfloat_link_plugin_ui_common(${target_name})
endfunction()

# Legacy alias used by older CMake snippets.
function(dontfloat_link_plugin_pitch_ui target_name)
    dontfloat_link_plugin_ui(${target_name} full)
endfunction()
