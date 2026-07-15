# Build three DAW plugin products (Full / Scratch / Pitcher) for CLAP, LV2, and VST3.

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/PluginPitchUi.cmake)

set(_DONTFLOAT_PRODUCT_KINDS full scratch pitcher)
set(_DONTFLOAT_PRODUCT_INDEX_full 0)
set(_DONTFLOAT_PRODUCT_INDEX_scratch 1)
set(_DONTFLOAT_PRODUCT_INDEX_pitcher 2)

set(_DONTFLOAT_CLAP_FILE_full dontfloat)
set(_DONTFLOAT_CLAP_FILE_scratch dontfloat_scratch)
set(_DONTFLOAT_CLAP_FILE_pitcher dontfloat_pitcher)

set(_DONTFLOAT_LV2_BUNDLE_full dontfloat.lv2)
set(_DONTFLOAT_LV2_BUNDLE_scratch dontfloat_scratch.lv2)
set(_DONTFLOAT_LV2_BUNDLE_pitcher dontfloat_pitcher.lv2)

set(_DONTFLOAT_LV2_TTL_FILE_full dontfloat.ttl)
set(_DONTFLOAT_LV2_TTL_FILE_scratch dontfloat_scratch.ttl)
set(_DONTFLOAT_LV2_TTL_FILE_pitcher dontfloat_pitcher.ttl)

set(_DONTFLOAT_LV2_BINARY_full dontfloat)
set(_DONTFLOAT_LV2_BINARY_scratch dontfloat_scratch)
set(_DONTFLOAT_LV2_BINARY_pitcher dontfloat_pitcher)

set(_DONTFLOAT_LV2_UI_BINARY_full dontfloat_ui)
set(_DONTFLOAT_LV2_UI_BINARY_scratch dontfloat_scratch_ui)
set(_DONTFLOAT_LV2_UI_BINARY_pitcher dontfloat_pitcher_ui)

set(_DONTFLOAT_LV2_URI_full "https://github.com/ili4yov-ika/DONTFLOAT/plugins/full")
set(_DONTFLOAT_LV2_URI_scratch "https://github.com/ili4yov-ika/DONTFLOAT/plugins/scratch")
set(_DONTFLOAT_LV2_URI_pitcher "https://github.com/ili4yov-ika/DONTFLOAT/plugins/pitcher")

set(_DONTFLOAT_LV2_UI_URI_full "${_DONTFLOAT_LV2_URI_full}#ui")
set(_DONTFLOAT_LV2_UI_URI_scratch "${_DONTFLOAT_LV2_URI_scratch}#ui")
set(_DONTFLOAT_LV2_UI_URI_pitcher "${_DONTFLOAT_LV2_URI_pitcher}#ui")

set(_DONTFLOAT_LV2_NAME_full "DONTFLOAT")
set(_DONTFLOAT_LV2_NAME_scratch "DONTFLOAT Scratch")
set(_DONTFLOAT_LV2_NAME_pitcher "DONTFLOAT Pitcher")

set(_DONTFLOAT_CLAP_ID_full "com.dontfloat.full")
set(_DONTFLOAT_CLAP_ID_scratch "com.dontfloat.scratch")
set(_DONTFLOAT_CLAP_ID_pitcher "com.dontfloat.pitcher")

set(_DONTFLOAT_CLAP_NAME_full "DONTFLOAT")
set(_DONTFLOAT_CLAP_NAME_scratch "DONTFLOAT Scratch")
set(_DONTFLOAT_CLAP_NAME_pitcher "DONTFLOAT Pitcher")

set(_DONTFLOAT_VST3_NAME_full "DONTFLOAT")
set(_DONTFLOAT_VST3_NAME_scratch "DONTFLOAT Scratch")
set(_DONTFLOAT_VST3_NAME_pitcher "DONTFLOAT Pitcher")

function(_dontfloat_product_index kind out_var)
    set(${out_var} ${_DONTFLOAT_PRODUCT_INDEX_${kind}} PARENT_SCOPE)
endfunction()

function(dontfloat_add_clap_product kind)
    _dontfloat_product_index(${kind} _index)
    set(_target dontfloat_${kind}_clap)
    add_library(${_target} MODULE
        plugins/clap/dontfloat_clap_plugin_impl.cpp
        plugins/clap/clap_minimal.h
    )
    target_compile_features(${_target} PRIVATE cxx_std_17)
    target_compile_definitions(${_target} PRIVATE DONTFLOAT_PLUGIN_PRODUCT_INDEX=${_index})
    target_include_directories(${_target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/clap
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/core
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/ui
    )
    target_link_libraries(${_target} PRIVATE dontfloat_plugin_core)
    set_target_properties(${_target} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${_DONTFLOAT_CLAP_FILE_${kind}}"
        SUFFIX ".clap"
    )
    dontfloat_link_plugin_ui(${_target} ${kind})

    set(_smoke clap_${kind}_smoke_test)
    add_executable(${_smoke}
        plugins/clap/tests/clap_product_smoke_test.cpp
        plugins/clap/dontfloat_clap_plugin_impl.cpp
    )
    target_compile_features(${_smoke} PRIVATE cxx_std_17)
    target_compile_definitions(${_smoke} PRIVATE DONTFLOAT_PLUGIN_PRODUCT_INDEX=${_index})
    target_include_directories(${_smoke} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/clap
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/core
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/ui
    )
    target_link_libraries(${_smoke} PRIVATE dontfloat_plugin_core)
    dontfloat_link_plugin_ui(${_smoke} ${kind})
    if(WIN32 AND DEFINED QT_ROOT_DIR)
        add_test(NAME ${_smoke}
            COMMAND ${CMAKE_COMMAND}
                -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
                -DCONFIG=$<CONFIG>
                -DTARGET_NAME=${_smoke}
                -DQT_ROOT=${QT_ROOT_DIR}
                -DWORKING_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DCMAKE_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}
                -DCMAKE_COMMAND=${CMAKE_COMMAND}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQtTest.cmake
        )
    else()
        add_test(NAME ${_smoke} COMMAND ${_smoke})
    endif()
    set_tests_properties(${_smoke} PROPERTIES
        LABELS "plugins;clap;${kind}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    install(TARGETS ${_target}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/clap
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/clap
        OPTIONAL
    )
endfunction()

function(dontfloat_add_lv2_product kind)
    _dontfloat_product_index(${kind} _index)
    set(_bundle_dir "${CMAKE_CURRENT_BINARY_DIR}/plugins/lv2/${_DONTFLOAT_LV2_BUNDLE_${kind}}")
    set(_lv2_target dontfloat_${kind}_lv2)
    set(_lv2_ui_target dontfloat_${kind}_lv2_ui)

    add_library(${_lv2_target} MODULE
        plugins/lv2/dontfloat_lv2_plugin_impl.cpp
        plugins/lv2/lv2_minimal.h
    )
    target_compile_features(${_lv2_target} PRIVATE cxx_std_17)
    target_compile_definitions(${_lv2_target} PRIVATE DONTFLOAT_PLUGIN_PRODUCT_INDEX=${_index})
    target_link_libraries(${_lv2_target} PRIVATE dontfloat_plugin_core)
    target_include_directories(${_lv2_target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/lv2
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/core
    )
    set_target_properties(${_lv2_target} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${_DONTFLOAT_LV2_BINARY_${kind}}"
        ARCHIVE_OUTPUT_NAME "${_lv2_target}_import"
        LIBRARY_OUTPUT_DIRECTORY "${_bundle_dir}"
        RUNTIME_OUTPUT_DIRECTORY "${_bundle_dir}"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${_bundle_dir}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${_bundle_dir}"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${_bundle_dir}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${_bundle_dir}"
        LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${_bundle_dir}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${_bundle_dir}"
        LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${_bundle_dir}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${_bundle_dir}"
    )

    add_library(${_lv2_ui_target} MODULE
        plugins/lv2/dontfloat_lv2_ui_impl.cpp
        plugins/lv2/lv2_minimal.h
    )
    target_compile_features(${_lv2_ui_target} PRIVATE cxx_std_17)
    target_compile_definitions(${_lv2_ui_target} PRIVATE DONTFLOAT_PLUGIN_PRODUCT_INDEX=${_index})
    target_include_directories(${_lv2_ui_target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/lv2
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/core
    )
    set_target_properties(${_lv2_ui_target} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${_DONTFLOAT_LV2_UI_BINARY_${kind}}"
        ARCHIVE_OUTPUT_NAME "${_lv2_ui_target}_import"
        LIBRARY_OUTPUT_DIRECTORY "${_bundle_dir}"
        RUNTIME_OUTPUT_DIRECTORY "${_bundle_dir}"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${_bundle_dir}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${_bundle_dir}"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${_bundle_dir}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${_bundle_dir}"
        LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${_bundle_dir}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${_bundle_dir}"
        LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${_bundle_dir}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${_bundle_dir}"
    )
    dontfloat_link_plugin_ui(${_lv2_ui_target} ${kind})

    if(WIN32)
        set(_dontfloat_lv2_binary_ext ".dll")
        set(_lv2_ui_type "ui:WindowsUI")
    elseif(APPLE)
        set(_dontfloat_lv2_binary_ext ".dylib")
        set(_lv2_ui_type "ui:CocoaUI")
    else()
        set(_dontfloat_lv2_binary_ext ".so")
        set(_lv2_ui_type "ui:X11UI")
    endif()

    set(LV2_BINARY_EXT "${_dontfloat_lv2_binary_ext}")
    set(LV2_URI "${_DONTFLOAT_LV2_URI_${kind}}")
    set(LV2_UI_URI "${_DONTFLOAT_LV2_UI_URI_${kind}}")
    set(LV2_NAME "${_DONTFLOAT_LV2_NAME_${kind}}")
    set(LV2_BINARY "${_DONTFLOAT_LV2_BINARY_${kind}}")
    set(LV2_UI_BINARY "${_DONTFLOAT_LV2_UI_BINARY_${kind}}")
    set(LV2_TTL_FILE "${_DONTFLOAT_LV2_TTL_FILE_${kind}}")
    set(LV2_UI_TYPE "${_lv2_ui_type}")

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/lv2/templates/manifest.ttl.in
        ${_bundle_dir}/manifest.ttl
        @ONLY
    )
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/lv2/templates/plugin.ttl.in
        ${_bundle_dir}/${LV2_TTL_FILE}
        @ONLY
    )

    set(_lv2_smoke lv2_${kind}_smoke_test)
    add_executable(${_lv2_smoke}
        plugins/lv2/tests/lv2_product_smoke_test.cpp
        plugins/lv2/dontfloat_lv2_plugin_impl.cpp
    )
    target_compile_features(${_lv2_smoke} PRIVATE cxx_std_17)
    target_compile_definitions(${_lv2_smoke} PRIVATE DONTFLOAT_PLUGIN_PRODUCT_INDEX=${_index})
    target_link_libraries(${_lv2_smoke} PRIVATE dontfloat_plugin_core)
    target_include_directories(${_lv2_smoke} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/lv2
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/core
    )
    add_test(NAME ${_lv2_smoke} COMMAND ${_lv2_smoke})
    set_tests_properties(${_lv2_smoke} PROPERTIES
        LABELS "plugins;lv2;${kind}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    set(_lv2_ui_smoke lv2_${kind}_ui_smoke_test)
    add_executable(${_lv2_ui_smoke}
        plugins/lv2/tests/lv2_product_ui_smoke_test.cpp
        plugins/lv2/dontfloat_lv2_ui_impl.cpp
    )
    target_compile_features(${_lv2_ui_smoke} PRIVATE cxx_std_17)
    target_compile_definitions(${_lv2_ui_smoke} PRIVATE DONTFLOAT_PLUGIN_PRODUCT_INDEX=${_index})
    target_include_directories(${_lv2_ui_smoke} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/lv2
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/core
    )
    dontfloat_link_plugin_ui(${_lv2_ui_smoke} ${kind})
    if(WIN32 AND DEFINED QT_ROOT_DIR)
        add_test(NAME ${_lv2_ui_smoke}
            COMMAND ${CMAKE_COMMAND}
                -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
                -DCONFIG=$<CONFIG>
                -DTARGET_NAME=${_lv2_ui_smoke}
                -DQT_ROOT=${QT_ROOT_DIR}
                -DWORKING_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DCMAKE_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}
                -DCMAKE_COMMAND=${CMAKE_COMMAND}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQtTest.cmake
        )
    else()
        add_test(NAME ${_lv2_ui_smoke} COMMAND ${_lv2_ui_smoke})
    endif()
    set_tests_properties(${_lv2_ui_smoke} PROPERTIES
        LABELS "plugins;lv2;${kind};ui"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    install(TARGETS ${_lv2_target}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/lv2/${_DONTFLOAT_LV2_BUNDLE_${kind}}
        RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}/lv2/${_DONTFLOAT_LV2_BUNDLE_${kind}}
        OPTIONAL
    )
    install(TARGETS ${_lv2_ui_target}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/lv2/${_DONTFLOAT_LV2_BUNDLE_${kind}}
        RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}/lv2/${_DONTFLOAT_LV2_BUNDLE_${kind}}
        OPTIONAL
    )
    install(FILES
        ${_bundle_dir}/manifest.ttl
        ${_bundle_dir}/${LV2_TTL_FILE}
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/lv2/${_DONTFLOAT_LV2_BUNDLE_${kind}}
        OPTIONAL
    )
endfunction()

function(dontfloat_ensure_vst3_sdk)
    if(TARGET sdk OR TARGET dontfloat_vst3_sdk)
        return()
    endif()
    if(NOT DONTFLOAT_VST3_SDK_ROOT OR NOT EXISTS "${DONTFLOAT_VST3_SDK_ROOT}/CMakeLists.txt")
        return()
    endif()
    if(CMAKE_VERSION VERSION_LESS "3.25.0")
        message(WARNING "VST3 SDK requires CMake 3.25+ (have ${CMAKE_VERSION}). Skipping VST3 plugin targets.")
        return()
    endif()
    set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
    add_subdirectory("${DONTFLOAT_VST3_SDK_ROOT}" "${CMAKE_BINARY_DIR}/_deps/vst3sdk" EXCLUDE_FROM_ALL)
endfunction()

function(dontfloat_add_vst3_product kind)
    dontfloat_ensure_vst3_sdk()
    if(NOT TARGET sdk)
        return()
    endif()

    _dontfloat_product_index(${kind} _index)
    set(_target dontfloat_${kind}_vst3)
    add_library(${_target} MODULE
        plugins/vst3/dontfloat_vst3_plugin_impl.cpp
    )
    target_compile_features(${_target} PRIVATE cxx_std_17)
    target_compile_definitions(${_target} PRIVATE
        DONTFLOAT_HAS_VST3_SDK
        DONTFLOAT_PLUGIN_PRODUCT_INDEX=${_index}
    )
    target_include_directories(${_target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/core
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/vst3
        ${DONTFLOAT_VST3_SDK_ROOT}
    )
    set_target_properties(${_target} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${_DONTFLOAT_VST3_NAME_${kind}}"
        SUFFIX ".vst3"
    )
    target_link_libraries(${_target} PRIVATE sdk)
    dontfloat_link_plugin_ui(${_target} ${kind})

    install(TARGETS ${_target}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/vst3
        RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}/vst3
        OPTIONAL
    )
endfunction()

function(dontfloat_add_all_plugin_products)
    foreach(_kind IN LISTS _DONTFLOAT_PRODUCT_KINDS)
        if(DONTFLOAT_BUILD_CLAP)
            dontfloat_add_clap_product(${_kind})
        endif()
        if(DONTFLOAT_BUILD_LV2)
            dontfloat_add_lv2_product(${_kind})
        endif()
        if(DONTFLOAT_BUILD_VST3)
            dontfloat_add_vst3_product(${_kind})
        endif()
    endforeach()
endfunction()
