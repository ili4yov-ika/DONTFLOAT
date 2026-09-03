# Подключение ARA 2 SDK (Audio Random Access, Celemony).
#
# ARA даёт плагину произвольный доступ ко всему звуку дорожки и общую с хостом
# модель документа: анализ идёт без проигрывания, а разметку (ноты, темп,
# тональность) хост и плагины читают друг у друга. Все экземпляры плагина в
# одном проекте DAW работают с одним document controller — на этом строится
# показ нот соседней дорожки референсом.
#
# SDK берётся из DONTFLOAT_ARA_SDK_ROOT (по умолчанию C:/SDKs/ARA_SDK, как у
# VST3). Нет SDK — цели ARA просто не собираются, остальной проект не страдает.

if(TARGET ARA_PlugIn_Library)
    return()
endif()

if(DONTFLOAT_ARA_SDK_ROOT STREQUAL "" AND DEFINED ENV{DONTFLOAT_ARA_SDK_ROOT})
    set(DONTFLOAT_ARA_SDK_ROOT "$ENV{DONTFLOAT_ARA_SDK_ROOT}" CACHE PATH "Path to ARA 2 SDK" FORCE)
elseif(DONTFLOAT_ARA_SDK_ROOT STREQUAL "" AND EXISTS "C:/SDKs/ARA_SDK/ARA_Library/CMakeLists.txt")
    set(DONTFLOAT_ARA_SDK_ROOT "C:/SDKs/ARA_SDK" CACHE PATH "Path to ARA 2 SDK" FORCE)
endif()

set(DONTFLOAT_ARA_AVAILABLE OFF CACHE INTERNAL "ARA 2 SDK found")

if(DONTFLOAT_ARA_SDK_ROOT STREQUAL "")
    message(STATUS "ARA 2: SDK не задан (DONTFLOAT_ARA_SDK_ROOT) — цели ARA пропущены")
    return()
endif()
if(NOT EXISTS "${DONTFLOAT_ARA_SDK_ROOT}/ARA_Library/CMakeLists.txt")
    message(WARNING "ARA 2: в ${DONTFLOAT_ARA_SDK_ROOT} нет ARA_Library — цели ARA пропущены")
    return()
endif()

# ARA_Library требует CMake 3.19+; у нас минимум 3.16, поэтому проверяем явно
if(CMAKE_VERSION VERSION_LESS 3.19)
    message(WARNING "ARA 2: нужен CMake 3.19+, найден ${CMAKE_VERSION} — цели ARA пропущены")
    return()
endif()

set(ARA_API_DIR "${DONTFLOAT_ARA_SDK_ROOT}/ARA_API" CACHE PATH "ARA_API directory" FORCE)
add_subdirectory("${DONTFLOAT_ARA_SDK_ROOT}/ARA_Library" "${CMAKE_BINARY_DIR}/_deps/ara_library" EXCLUDE_FROM_ALL)

if(NOT TARGET ARA_PlugIn_Library)
    message(WARNING "ARA 2: ARA_PlugIn_Library не собралась — цели ARA пропущены")
    return()
endif()

set(DONTFLOAT_ARA_AVAILABLE ON CACHE INTERNAL "ARA 2 SDK found" FORCE)
message(STATUS "ARA 2 SDK: ${DONTFLOAT_ARA_SDK_ROOT}")

# ======================
# Слой ARA самого DONTFLOAT: document controller и разбор нот поверх ARA-доступа
# к сэмплам. Линкуется в обёртки форматов (VST3/CLAP) и в тесты.
function(dontfloat_add_ara_library)
    if(TARGET dontfloat_ara)
        return()
    endif()
    add_library(dontfloat_ara STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/ara/dontfloat_ara_document_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/ara/dontfloat_ara_document_controller.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pitchdetector.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/pitchdetector.h
    )
    # Слой ARA линкуется в модули плагинов — нужен -fPIC, как и остальным
    # статическим библиотекам в этой цепочке (см. dontfloat_plugin_core)
    set_target_properties(dontfloat_ara PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_compile_features(dontfloat_ara PUBLIC cxx_std_17)
    target_include_directories(dontfloat_ara PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/core
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/ara
    )
    target_link_libraries(dontfloat_ara PUBLIC
        ARA_PlugIn_Library
        dontfloat_plugin_core
        Qt6::Core
    )
endfunction()
