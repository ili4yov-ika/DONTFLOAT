# Rubber Band Library v4 — single-file build (GPL-2.0-or-later).
# Исходники: thirdparty/rubberband (тег v4.0.0).
#   git clone --depth 1 --branch v4.0.0 https://github.com/breakfastquay/rubberband.git thirdparty/rubberband

set(DONTFLOAT_RUBBERBAND_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rubberband" CACHE PATH
    "Корень исходников Rubber Band Library")

if(NOT EXISTS "${DONTFLOAT_RUBBERBAND_ROOT}/single/RubberBandSingle.cpp")
    message(FATAL_ERROR
        "Rubber Band не найден: ${DONTFLOAT_RUBBERBAND_ROOT}/single/RubberBandSingle.cpp\n"
        "Выполните:\n"
        "  git clone --depth 1 --branch v4.0.0 "
        "https://github.com/breakfastquay/rubberband.git thirdparty/rubberband")
endif()

if(NOT TARGET rubberband_single)
    add_library(rubberband_single STATIC
        "${DONTFLOAT_RUBBERBAND_ROOT}/single/RubberBandSingle.cpp"
    )

    target_include_directories(rubberband_single SYSTEM PUBLIC
        "${DONTFLOAT_RUBBERBAND_ROOT}"
    )

    target_compile_definitions(rubberband_single PUBLIC RUBBERBAND_STATIC)

    set_target_properties(rubberband_single PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )

    if(MSVC)
        target_compile_definitions(rubberband_single PRIVATE _USE_MATH_DEFINES NOMINMAX)
        target_compile_options(rubberband_single PRIVATE
            /wd4244 /wd4267 /wd4996 /wd4018 /wd4389
        )
    else()
        target_compile_options(rubberband_single PRIVATE
            -Wno-sign-compare -Wno-unused-parameter
        )
    endif()

    message(STATUS "Rubber Band Library enabled: ${DONTFLOAT_RUBBERBAND_ROOT}")
endif()

function(dontfloat_link_rubberband target_name)
    if(NOT TARGET rubberband_single)
        message(FATAL_ERROR "dontfloat_link_rubberband: rubberband_single не создан")
    endif()
    target_link_libraries(${target_name} PRIVATE rubberband_single)
endfunction()
