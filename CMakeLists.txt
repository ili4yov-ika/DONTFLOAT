cmake_minimum_required(VERSION 3.16)

# Единственное место, где живёт номер версии: и приложение, и плагины (CLAP,
# VST3, ARA) берут его из сгенерированного dontfloat_version.h
project(DONTFLOAT VERSION 0.1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(GNUInstallDirs)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/dontfloat_version.h.in
    ${CMAKE_BINARY_DIR}/generated/dontfloat_version.h
    @ONLY
)
# Каталог со сгенерированным заголовком виден всем целям проекта
include_directories(${CMAKE_BINARY_DIR}/generated)

# MSVC: в qm-dsp (TempoTrackV2::calculateBeatPeriod) std::copy(df.begin()+(i+k), …);
# проверяемые итераторы в Debug дают assert _Verify_offset. Задаём явно для всех CONFIG,
# чтобы Qt Creator / qmake и мульти-конфиг не пропускали уровень (как у MinGW).
if(MSVC)
    add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
endif()

# VS 2025 может выставить VCToolsVersion=14.50 в окружении; v143 (Qt msvc2022_64) требует 14.3x → MSB8052.
if(MSVC AND CMAKE_GENERATOR MATCHES "Visual Studio")
    if(NOT CMAKE_VS_GLOBALS MATCHES "VCToolsVersion=")
        set(_vs_instance_root "")
        if(CMAKE_GENERATOR_INSTANCE)
            set(_vs_instance_root "${CMAKE_GENERATOR_INSTANCE}")
        elseif(DEFINED ENV{VSINSTALLDIR})
            file(TO_CMAKE_PATH "$ENV{VSINSTALLDIR}" _vs_instance_root)
        endif()
        if(_vs_instance_root)
            set(_v143_default "${_vs_instance_root}/VC/Auxiliary/Build/Microsoft.VCToolsVersion.v143.default.txt")
            if(EXISTS "${_v143_default}")
                file(READ "${_v143_default}" _v143_tools_version)
                string(STRIP "${_v143_tools_version}" _v143_tools_version)
                if(_v143_tools_version)
                    list(APPEND CMAKE_VS_GLOBALS "VCToolsVersion=${_v143_tools_version}")
                    message(STATUS "MSVC v143: VCToolsVersion=${_v143_tools_version}")
                endif()
            endif()
        endif()
        unset(_vs_instance_root)
        unset(_v143_default)
        unset(_v143_tools_version)
    endif()
endif()

# Enable automatic handling of Qt files
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Указываем путь для поиска UI файлов
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui)

# Qt6: на Windows — типовые пути установщика Qt; на macOS/Linux — cmake/PlatformQt.cmake
# и/или -DCMAKE_PREFIX_PATH (Homebrew qt@6, ~/Qt/6.x/macos|gcc_64).
set(QT_MIN_VERSION "6.8.0")
if(WIN32)
    if(MSVC)
        if(NOT "C:/Qt/6.9.3/msvc2022_64" IN_LIST CMAKE_PREFIX_PATH AND EXISTS "C:/Qt/6.9.3/msvc2022_64")
            list(PREPEND CMAKE_PREFIX_PATH "C:/Qt/6.9.3/msvc2022_64")
        elseif(NOT "C:/Qt/6.8.3/msvc2022_64" IN_LIST CMAKE_PREFIX_PATH AND EXISTS "C:/Qt/6.8.3/msvc2022_64")
            list(PREPEND CMAKE_PREFIX_PATH "C:/Qt/6.8.3/msvc2022_64")
        endif()
    elseif(MINGW)
        if(NOT "C:/Qt/6.9.3/mingw_64" IN_LIST CMAKE_PREFIX_PATH AND EXISTS "C:/Qt/6.9.3/mingw_64")
            list(PREPEND CMAKE_PREFIX_PATH "C:/Qt/6.9.3/mingw_64")
        elseif(NOT "C:/Qt/6.8.3/mingw_64" IN_LIST CMAKE_PREFIX_PATH AND EXISTS "C:/Qt/6.8.3/mingw_64")
            list(PREPEND CMAKE_PREFIX_PATH "C:/Qt/6.8.3/mingw_64")
        endif()
    endif()
else()
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/PlatformQt.cmake)
    message(STATUS "Сборка ${CMAKE_SYSTEM_NAME}: поиск Qt6 через CMAKE_PREFIX_PATH и стандартные каталоги.")
endif()

# Генерируем compile_commands.json для IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DONTFLOAT_BUILD_PLUGINS "Build DAW plugin targets" ON)
# CLAP и LV2 выключены: развитие идёт на VST3 с ARA, а держать три формата в
# рабочем состоянии дороже, чем они дают. Включаются флагом при сборке.
option(DONTFLOAT_BUILD_CLAP "Build CLAP plugin target when plugins are enabled" OFF)
option(DONTFLOAT_BUILD_LV2 "Build LV2 plugin target when plugins are enabled" OFF)
option(DONTFLOAT_BUILD_VST3 "Build VST3 plugin target when SDK is available" ON)
option(DONTFLOAT_BUILD_PLUGIN_TESTER "Build plugin host probe GUI tool" ON)
option(DONTFLOAT_BUILD_MINI_DAW "Build minimal in-process plugin hosts (mini-DAW) per format/product" ON)
set(DONTFLOAT_CLAP_SDK_ROOT "" CACHE PATH "Optional path to official CLAP SDK")
set(DONTFLOAT_VST3_SDK_ROOT "" CACHE PATH "Optional path to VST3 SDK")
set(DONTFLOAT_ARA_SDK_ROOT "" CACHE PATH "Optional path to ARA 2 SDK")
if(DONTFLOAT_VST3_SDK_ROOT STREQUAL "" AND DEFINED ENV{DONTFLOAT_VST3_SDK_ROOT})
    set(DONTFLOAT_VST3_SDK_ROOT "$ENV{DONTFLOAT_VST3_SDK_ROOT}" CACHE PATH "Optional path to VST3 SDK" FORCE)
elseif(DONTFLOAT_VST3_SDK_ROOT STREQUAL "" AND EXISTS "C:/SDKs/vst3sdk/CMakeLists.txt")
    set(DONTFLOAT_VST3_SDK_ROOT "C:/SDKs/vst3sdk" CACHE PATH "Optional path to VST3 SDK" FORCE)
endif()

# ARA 2 (Audio Random Access): необязательный SDK, см. cmake/AraSdk.cmake
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/AraSdk.cmake)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Multimedia
    MultimediaWidgets
    Concurrent
    Test
)

# Qt Svg рисует иконки кнопок напрямую (см. src/svgiconloader.cpp). Модуль
# есть не в каждой сборке Qt, поэтому он необязательный: без него иконки
# грузятся обычным QIcon, а конфигурация не падает.
find_package(Qt6 ${QT_MIN_VERSION} QUIET COMPONENTS Svg)
if(TARGET Qt6::Svg)
    set(DONTFLOAT_HAS_QT_SVG ON)
    add_compile_definitions(DONTFLOAT_HAS_QT_SVG=1)
    message(STATUS "Qt Svg найден: иконки рисуются QSvgRenderer")
else()
    set(DONTFLOAT_HAS_QT_SVG OFF)
    message(STATUS "Qt Svg не найден: иконки грузятся через QIcon (нужен плагин qsvgicon)")
endif()

# Хелпер: линкует Qt6::Svg, если он есть
function(dontfloat_link_qt_svg target_name)
    if(TARGET Qt6::Svg)
        target_link_libraries(${target_name} PRIVATE Qt6::Svg)
    endif()
endfunction()

# Каталог Qt для CTest на Windows (PATH к DLL); берём из найденного Qt6_DIR (MSVC или MinGW).
if(WIN32)
    get_filename_component(_qt6_cmake_parent "${Qt6_DIR}" DIRECTORY)
    get_filename_component(_qt6_lib "${_qt6_cmake_parent}" DIRECTORY)
    get_filename_component(QT_ROOT_DIR "${_qt6_lib}" DIRECTORY)
    set(QT_ROOT_DIR "${QT_ROOT_DIR}" CACHE PATH "Qt6 root (Windows CTest / DLL PATH)" FORCE)
    message(STATUS "Используется Qt: ${QT_ROOT_DIR}")
    unset(_qt6_cmake_parent)
    unset(_qt6_lib)
endif()

# Add source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/waveformview.cpp
    src/markerengine.cpp
    src/pitchgridwidget.cpp
    src/pianoroll_engine.cpp
    src/pianoroll_toolbar.cpp
    src/midiexporter.cpp
    src/midiimporter.cpp
    src/pitchdetector.cpp
    src/pitchcorrection.cpp
    src/notepreviewplayer.cpp
    src/waveformcolors.cpp
    src/waveformpeaks.cpp
    src/bpmanalyzer.cpp
    src/keyanalyzer.cpp
    src/waveformanalyzer.cpp
    src/audiocommand.cpp
    src/loadfiledialog.cpp
    src/metronomesettingsdialog.cpp
    src/metronomecontroller.cpp
    src/beatfixcommand.cpp
    src/timestretchcommand.cpp
    src/beatvisualizer.cpp
    src/timestretchprocessor.cpp
    src/rubberband_offline.cpp
    src/timeutils.cpp
    src/wavwriter.cpp
    src/audiofileservice.cpp
    src/keyselectionmenu.cpp
    src/keymodulationstrip.cpp
    src/svgiconloader.cpp
    src/spectrogramsettingsdialog.cpp
    src/shortcutsdialog.cpp
)

# Add header files
set(HEADERS
    include/mainwindow.h
    include/waveformview.h
    include/markerengine.h
    include/pitchgridwidget.h
    include/pianoroll_engine.h
    include/pianoroll_toolbar.h
    include/midiexporter.h
    include/midiimporter.h
    include/pitchdetector.h
    include/pitchcorrection.h
    include/pitchnoteeditcommand.h
    include/pitchnotesplitcommand.h
    include/pitchnotemovecommand.h
    include/notepreviewplayer.h
    include/waveformcolors.h
    include/waveformpeaks.h
    include/bpmanalyzer.h
    include/keyanalyzer.h
    include/waveformanalyzer.h
    include/audiocommand.h
    include/loadfiledialog.h
    include/metronomesettingsdialog.h
    include/metronomecontroller.h
    include/beatfixcommand.h
    include/timestretchcommand.h
    include/beatvisualizer.h
    include/timestretchprocessor.h
    include/rubberband_offline.h
    include/timeutils.h
    include/wavwriter.h
    include/audiofileservice.h
    include/uiconstants.h
    include/keyselectionmenu.h
    include/keymodulationstrip.h
    include/svgiconloader.h
    include/spectrogramsettingsdialog.h
    include/fft_engine.h
    include/granularpitchshifter_engine.h
    include/shortcutsdialog.h
)

# Add UI files
set(UI_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/mainwindow.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/loadfiledialog.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/metronomesettingsdialog.ui
)

# Add resource files
set(RESOURCE_FILES
    resources.qrc
)

# Add translation files
set(TRANSLATION_FILES
    translations/ru_RU.ts
    translations/en_US.ts
)

# Windows: embed application icon into EXE
set(EXTRA_SOURCES "")
if(WIN32)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/logo.ico)
        list(APPEND EXTRA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc)
    else()
        message(STATUS "resources/icons/logo.ico not found - EXE will use default Windows icon. Run: python tools/svg_to_ico.py")
    endif()
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/RubberBand.cmake)

# Create executable
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${UI_FILES}
    ${RESOURCE_FILES}
    ${EXTRA_SOURCES}
)

# Include directories
set(_DONTFLOAT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_autogen
)
if(WIN32)
    list(APPEND _DONTFLOAT_INCLUDE_DIRS
        ${CMAKE_CURRENT_BINARY_DIR}/Desktop_Qt_6_8_3_MSVC2022_64bit-Debug
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_autogen/include_Debug
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_autogen/include_Release
    )
endif()
target_include_directories(${PROJECT_NAME} PRIVATE ${_DONTFLOAT_INCLUDE_DIRS})

# Link Qt libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Concurrent
)
dontfloat_link_qt_svg(${PROJECT_NAME})
dontfloat_link_rubberband(${PROJECT_NAME})

# windeployqt-хелпер нужен целям плагинов и мини-DAW, которые создаются ниже
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/WinDeployQt.cmake)

if(DONTFLOAT_BUILD_PLUGINS)
    add_library(dontfloat_plugin_core STATIC
        plugins/core/dontfloat_plugin_core.cpp
        plugins/core/dontfloat_plugin_core.h
        plugins/core/dontfloat_shared_notes.cpp
        plugins/core/dontfloat_shared_notes.h
        plugins/core/dontfloat_diagnostics.h
        plugins/core/plugin_product.cpp
        plugins/core/plugin_product.h
    )
    # Библиотека уходит внутрь разделяемых модулей плагинов (.clap, .so,
    # .vst3), поэтому её объекты обязаны быть позиционно-независимыми.
    # На Windows флаг ничего не меняет, а на Linux без него линковка модуля
    # падает: «relocation R_X86_64_PC32 ... can not be used when making a
    # shared object». Всплыло не сразу — ссылку на __libc_single_threaded
    # притащил std::shared_ptr, которым публикуется результат обработки.
    set_target_properties(dontfloat_plugin_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_compile_features(dontfloat_plugin_core PUBLIC cxx_std_17)
    target_include_directories(dontfloat_plugin_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/core
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    if(DONTFLOAT_ARA_AVAILABLE)
        dontfloat_add_ara_library()
    endif()

    if(DONTFLOAT_BUILD_CLAP OR DONTFLOAT_BUILD_LV2 OR DONTFLOAT_BUILD_VST3)
        include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/PluginProducts.cmake)
        dontfloat_add_all_plugin_products()

        if(DONTFLOAT_BUILD_MINI_DAW)
            include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/PluginMiniDaw.cmake)
            dontfloat_add_all_mini_daws()
        endif()
    endif()
endif()

# Translations: build .qm from .ts (output in build dir; app loads from applicationDirPath())
find_package(Qt6 REQUIRED COMPONENTS LinguistTools)
qt_add_lrelease(DONTFLOAT_lrelease
    TS_FILES ${TRANSLATION_FILES}
    QM_FILES_OUTPUT_VARIABLE DONTFLOAT_QM_FILES
)
add_dependencies(${PROJECT_NAME} DONTFLOAT_lrelease)

# Third-party: qm-dsp (standalone, ранее бралась из thirdparty/mixxx/lib/qm-dsp)
set(QM_DSP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/qm-dsp)
if(EXISTS ${QM_DSP_ROOT})
    # Используем те же файлы, что и Mixxx (из их CMakeLists.txt)
    set(QM_DSP_CPP_SOURCES
        ${QM_DSP_ROOT}/base/Pitch.cpp
        ${QM_DSP_ROOT}/dsp/chromagram/Chromagram.cpp
        ${QM_DSP_ROOT}/dsp/chromagram/ConstantQ.cpp
        ${QM_DSP_ROOT}/dsp/keydetection/GetKeyMode.cpp
        ${QM_DSP_ROOT}/dsp/onsets/DetectionFunction.cpp
        ${QM_DSP_ROOT}/dsp/onsets/PeakPicking.cpp
        ${QM_DSP_ROOT}/dsp/phasevocoder/PhaseVocoder.cpp
        ${QM_DSP_ROOT}/dsp/rateconversion/Decimator.cpp
        ${QM_DSP_ROOT}/dsp/signalconditioning/DFProcess.cpp
        ${QM_DSP_ROOT}/dsp/signalconditioning/FiltFilt.cpp
        ${QM_DSP_ROOT}/dsp/signalconditioning/Filter.cpp
        ${QM_DSP_ROOT}/dsp/signalconditioning/Framer.cpp
        ${QM_DSP_ROOT}/dsp/tempotracking/DownBeat.cpp
        ${QM_DSP_ROOT}/dsp/tempotracking/TempoTrack.cpp
        ${QM_DSP_ROOT}/dsp/tempotracking/TempoTrackV2.cpp
        ${QM_DSP_ROOT}/dsp/tonal/ChangeDetectionFunction.cpp
        ${QM_DSP_ROOT}/dsp/tonal/TCSgram.cpp
        ${QM_DSP_ROOT}/dsp/tonal/TonalEstimator.cpp
        ${QM_DSP_ROOT}/dsp/transforms/FFT.cpp
        ${QM_DSP_ROOT}/maths/Correlation.cpp
        ${QM_DSP_ROOT}/maths/KLDivergence.cpp
        ${QM_DSP_ROOT}/maths/MathUtilities.cpp
    )

    # C файлы для kiss_fft (нужно компилировать как C)
    set(QM_DSP_C_SOURCES
        ${QM_DSP_ROOT}/ext/kissfft/kiss_fft.c
        ${QM_DSP_ROOT}/ext/kissfft/tools/kiss_fftr.c
    )

    # Создаем библиотеку с C++ и C файлами
    add_library(qm_dsp STATIC
        ${QM_DSP_CPP_SOURCES}
        ${QM_DSP_C_SOURCES}
    )

    # Тот же случай: qm_dsp линкуется в модули плагинов (см. dontfloat_plugin_core)
    set_target_properties(qm_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)

    # Устанавливаем язык для C файлов
    set_source_files_properties(${QM_DSP_C_SOURCES} PROPERTIES
        LANGUAGE C
    )

    # Определения компилятора (как в Mixxx)
    target_compile_definitions(qm_dsp PRIVATE kiss_fft_scalar=double)
    if(MSVC)
        # MSVC требует это для M_PI и других математических констант
        target_compile_definitions(qm_dsp PRIVATE _USE_MATH_DEFINES)
        # Подавляем предупреждения компилятора для внешней библиотеки Mixxx
        # C4244: преобразование типов (double->float, int64_t->int, size_t->int) - нормально для библиотеки
        # C4267: преобразование size_t в меньший тип - нормально для библиотеки
        # C4828: проблемы с кодировкой файла - не критично
        # Используем несколько методов для надежного подавления предупреждений
        set_target_properties(qm_dsp PROPERTIES
            VS_WARNINGS_DISABLE "4244;4267;4828"
            VS_GLOBAL_DisableSpecificWarnings "4244;4267;4828"
        )
        # Подавляем предупреждения через опции компилятора
        target_compile_options(qm_dsp PRIVATE
            /wd4244  # Преобразование типов с возможной потерей данных
            /wd4267  # Преобразование size_t в меньший тип
            /wd4828  # Проблемы с кодировкой файла
        )
        # Также подавляем для C файлов (kiss_fft) - используем COMPILE_OPTIONS
        set_source_files_properties(${QM_DSP_C_SOURCES} PROPERTIES
            COMPILE_OPTIONS "/wd4244;/wd4267;/wd4828"
        )
    elseif(UNIX)
        find_package(Threads REQUIRED)
        target_link_libraries(qm_dsp PRIVATE Threads::Threads)
        target_compile_definitions(qm_dsp PRIVATE USE_PTHREADS)
        # Third-party qm-dsp: suppress known external warnings on GCC/Clang
        target_compile_options(qm_dsp PRIVATE
            -Wno-sign-compare
        )
    endif()

    # Директории включений (как в Mixxx)
    target_include_directories(qm_dsp SYSTEM PUBLIC
        ${QM_DSP_ROOT}
        ${QM_DSP_ROOT}/include
    )

    # Enable Mixxx algorithm in our analyzer
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_MIXXX_QM_DSP)
    target_link_libraries(${PROJECT_NAME} PRIVATE qm_dsp)

    message(STATUS "Mixxx qm-dsp enabled: ${QM_DSP_ROOT}")
else()
    message(WARNING "qm-dsp not found at ${QM_DSP_ROOT}, using simplified BPM analyzer")
    message(STATUS "Building with simplified BPM analyzer (qm-dsp not found)")
endif()

# Платформенные свойства исполняемого файла
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
        MACOSX_BUNDLE FALSE
    )
    dontfloat_deploy_qt(${PROJECT_NAME})
elseif(APPLE)
    # Обычный бинарник в build/macos/DONTFLOAT; .app — через tools/macos_build.sh deploy
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE FALSE
    )
endif()

# Install rules
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Установка desktop файла и иконки (Linux)
if(UNIX AND NOT APPLE)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/tools/dontfloat.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
    # Icon=dontfloat в .desktop ищется в hicolor: без этой установки
    # значок в меню и в .deb/.rpm пакетах отсутствовал
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/logo.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME dontfloat.svg
    )
endif()

# Enable testing
enable_testing()

if(DONTFLOAT_BUILD_PLUGINS)
    add_executable(plugin_core_track_tool_test
        plugins/core/tests/plugin_core_track_tool_test.cpp
    )
    target_link_libraries(plugin_core_track_tool_test PRIVATE dontfloat_plugin_core)
    target_include_directories(plugin_core_track_tool_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/core
    )
    add_test(NAME plugin_core_track_tool_test COMMAND plugin_core_track_tool_test)
    set_tests_properties(plugin_core_track_tool_test PROPERTIES
        LABELS "plugins;core;track-tool"
        DESCRIPTION "Smoke tests for DONTFLOAT plugin core Track Tool session"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    if(DONTFLOAT_BUILD_PLUGIN_TESTER AND WIN32)
        set(PLUGIN_TESTER_PROBE_SOURCES
            tools/plugin_tester/plugin_host_probe.cpp
            tools/plugin_tester/plugin_host_probe.h
        )

        add_executable(dontfloat_plugin_tester WIN32
            tools/plugin_tester/main.cpp
            tools/plugin_tester/plugin_tester_window.cpp
            tools/plugin_tester/plugin_tester_window.h
            ${PLUGIN_TESTER_PROBE_SOURCES}
        )
        set_target_properties(dontfloat_plugin_tester PROPERTIES
            AUTOMOC ON
            WIN32_EXECUTABLE TRUE
        )
        target_include_directories(dontfloat_plugin_tester PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/plugin_tester
            ${CMAKE_CURRENT_SOURCE_DIR}/plugins/clap
            ${CMAKE_CURRENT_SOURCE_DIR}/plugins/lv2
        )
        target_link_libraries(dontfloat_plugin_tester PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
        )
        # Qt6Svg нужен модулю плагина (иконки через QSvgRenderer), а он
        # грузится с LOAD_WITH_ALTERED_SEARCH_PATH — рядом с хостом должен лежать
        dontfloat_link_qt_svg(dontfloat_plugin_tester)
        target_compile_definitions(dontfloat_plugin_tester PRIVATE
            "DONTFLOAT_PLUGIN_BUILD_ROOT=\"${CMAKE_BINARY_DIR}\""
        )
        if(MSVC)
            target_link_options(dontfloat_plugin_tester PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
        endif()
        dontfloat_deploy_qt(dontfloat_plugin_tester)

        add_executable(plugin_host_probe_test
            tools/plugin_tester/plugin_host_probe_test.cpp
            ${PLUGIN_TESTER_PROBE_SOURCES}
        )
        target_include_directories(plugin_host_probe_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/plugin_tester
            ${CMAKE_CURRENT_SOURCE_DIR}/plugins/clap
            ${CMAKE_CURRENT_SOURCE_DIR}/plugins/lv2
        )
        target_link_libraries(plugin_host_probe_test PRIVATE Qt6::Core)
        target_compile_definitions(plugin_host_probe_test PRIVATE
            "DONTFLOAT_PLUGIN_BUILD_ROOT=\"${CMAKE_BINARY_DIR}\""
        )
        if(DEFINED QT_ROOT_DIR)
            add_test(NAME plugin_host_probe_test
                COMMAND ${CMAKE_COMMAND}
                    -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
                    -DCONFIG=$<CONFIG>
                    -DTARGET_NAME=plugin_host_probe_test
                    -DQT_ROOT=${QT_ROOT_DIR}
                    -DWORKING_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                    -DCMAKE_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}
                    -DCMAKE_COMMAND=${CMAKE_COMMAND}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQtTest.cmake
            )
        else()
            add_test(NAME plugin_host_probe_test COMMAND plugin_host_probe_test)
        endif()
        set_tests_properties(plugin_host_probe_test PROPERTIES
            LABELS "plugins;host-probe"
            DESCRIPTION "Load/instantiate CLAP, VST3, LV2 product modules"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
    endif()
endif()

# Функция для создания тестового исполняемого файла
function(add_qt_test test_name)
    # Все остальные аргументы - исходные файлы
    set(test_sources ${ARGN})

    add_executable(${test_name} ${test_sources})

    # Автоматическая обработка MOC для Qt Test и всех исходных файлов
    set_target_properties(${test_name} PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
    )

    # Подключаем зависимости
    target_link_libraries(${test_name} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::MultimediaWidgets
        Qt6::Concurrent
        Qt6::Test
    )
    dontfloat_link_qt_svg(${test_name})

    # Директории включений
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    # Подключаем библиотеку qm_dsp если доступна
    if(TARGET qm_dsp)
        target_link_libraries(${test_name} PRIVATE qm_dsp)
        target_compile_definitions(${test_name} PRIVATE USE_MIXXX_QM_DSP)
    endif()

    list(FIND ARGN "src/timestretchprocessor.cpp" _dontfloat_ts_idx)
    if(NOT _dontfloat_ts_idx EQUAL -1)
        target_sources(${test_name} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/rubberband_offline.cpp)
        dontfloat_link_rubberband(${test_name})
    endif()

    # CTest: на Windows тесты не видят Qt DLL (0xc0000135), если bin не в PATH — запуск через cmake -P.
    if(WIN32 AND DEFINED QT_ROOT_DIR)
        add_test(NAME ${test_name}
            COMMAND ${CMAKE_COMMAND}
                -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
                -DCONFIG=$<CONFIG>
                -DTARGET_NAME=${test_name}
                -DQT_ROOT=${QT_ROOT_DIR}
                -DWORKING_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DCMAKE_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}
                -DCMAKE_COMMAND=${CMAKE_COMMAND}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQtTest.cmake
        )
    else()
        add_test(NAME ${test_name} COMMAND ${test_name})
    endif()

    set_tests_properties(${test_name} PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endfunction()

# BPM Analyzer тест (работа с реальными файлами из source4test)
add_qt_test(bpm_analyzer_test
    tests/bpm_analyzer_test.cpp
    src/bpmanalyzer.cpp
    src/audiofileservice.cpp
)

set_tests_properties(bpm_analyzer_test PROPERTIES
    LABELS "bpm;audio;integration;files"
    DESCRIPTION "BPM analyzer tests on real audio files from tests/source4test"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Тесты для новой функциональности вычисления отклонений долей
add_qt_test(beat_deviation_test
    tests/beat_deviation_test.cpp
    src/bpmanalyzer.cpp
)

# Потактовый анализ модуляции (смен тональности), как в Melodyne
add_qt_test(key_analyzer_test
    tests/key_analyzer_test.cpp
    src/keyanalyzer.cpp
)

set_tests_properties(key_analyzer_test PROPERTIES
    LABELS "key;modulation;unit"
    DESCRIPTION "Per-bar key/modulation detection (Melodyne-style)"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Тесты TimeStretchProcessor (тонкомпенсация, processSegment, calculateStretchFactor)
add_qt_test(timestretchprocessor_test
    tests/timestretchprocessor_test.cpp
    src/timestretchprocessor.cpp
    # buildSmartAlignmentMarkers отбирает доли через BPMAnalyzer
    src/bpmanalyzer.cpp
    src/markerengine.cpp
    src/timeutils.cpp
)

set_tests_properties(timestretchprocessor_test PROPERTIES
    LABELS "timestretch;pitch;unit"
    DESCRIPTION "Unit tests for time stretch and pitch compensation"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Пирамида пиков волны: значения совпадают с прямым перебором
add_qt_test(waveform_peaks_test
    tests/waveform_peaks_test.cpp
    src/waveformpeaks.cpp
    include/waveformpeaks.h
)

set_tests_properties(waveform_peaks_test PROPERTIES
    LABELS "unit;ui"
    DESCRIPTION "Waveform peak pyramid matches brute-force min/max"
)

# Правки клипов в DAW глазами плагина: рез, обрезка краёв, сжатие и растяжение.
# Тест гоняет ту же модель клипов, что и окно мини-DAW, поэтому не расходится с ним.
add_qt_test(mini_daw_clip_edits_test
    tests/mini_daw_clip_edits_test.cpp
    tools/mini_daw/mini_daw_clip_model.cpp
    plugins/core/dontfloat_plugin_core.cpp
)
target_include_directories(mini_daw_clip_edits_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/core
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/mini_daw
)

set_tests_properties(mini_daw_clip_edits_test PROPERTIES
    LABELS "unit;plugins;mini-daw"
    DESCRIPTION "Clip split/trim/stretch in the DAW as the plugin sees it"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Выравнивание долей по сетке: метки ведут из доли на сетку, звук остаётся звуком
add_qt_test(beat_align_test
    tests/beat_align_test.cpp
    src/timestretchprocessor.cpp
    # buildSmartAlignmentMarkers отбирает доли через BPMAnalyzer
    src/bpmanalyzer.cpp
    src/rubberband_offline.cpp
    src/markerengine.cpp
    src/timeutils.cpp
)
dontfloat_link_rubberband(beat_align_test)

set_tests_properties(beat_align_test PROPERTIES
    LABELS "unit;pitch;timestretch"
    DESCRIPTION "Beat alignment maps beats onto the grid without destroying audio"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Перестановка нот слышна: коррекция переносит звук вместе с нотой
add_qt_test(note_move_render_test
    tests/note_move_render_test.cpp
    src/pitchcorrection.cpp
    src/timestretchprocessor.cpp
    # buildSmartAlignmentMarkers отбирает доли через BPMAnalyzer
    src/bpmanalyzer.cpp
    src/rubberband_offline.cpp
    src/markerengine.cpp
    src/timeutils.cpp
    include/pitchgridwidget.h
    src/pitchgridwidget.cpp
    src/pianoroll_engine.cpp
)
dontfloat_link_rubberband(note_move_render_test)

set_tests_properties(note_move_render_test PROPERTIES
    LABELS "unit;pitch;pianoroll"
    DESCRIPTION "Moved notes are rendered at their new position"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Тонкомпенсация на реальном файле с одной нотой (pitch-test_C140BPM.mp3)
add_qt_test(pitch_compensation_file_test
    tests/pitch_compensation_file_test.cpp
    src/timestretchprocessor.cpp
    # buildSmartAlignmentMarkers отбирает доли через BPMAnalyzer
    src/bpmanalyzer.cpp
    src/markerengine.cpp
    src/timeutils.cpp
    src/audiofileservice.cpp
)

set_tests_properties(pitch_compensation_file_test PROPERTIES
    LABELS "timestretch;pitch;integration;files"
    DESCRIPTION "Pitch preservation on pitch-test_C140BPM.mp3 with marker stretch/compress"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# ARA2 и обмен нотами в настоящем REAPER. Без REAPER на машине тест сам себя
# пропускает, поэтому в CI он молчит.
add_qt_test(reaper_ara_integration_test
    tests/reaper_ara_integration_test.cpp
)

set_tests_properties(reaper_ara_integration_test PROPERTIES
    LABELS "integration;reaper;ara;plugins;daw"
    DESCRIPTION "ARA2 clip edits and shared reference notes inside a real REAPER"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    TIMEOUT 420
)

# Правка высоты нот и её поведение при растяжении дорожки (реальный MP3)
add_qt_test(pitch_note_stretch_test
    tests/pitch_note_stretch_test.cpp
    src/pitchdetector.cpp
    src/pitchcorrection.cpp
    src/timestretchprocessor.cpp
    # buildSmartAlignmentMarkers отбирает доли через BPMAnalyzer
    src/bpmanalyzer.cpp
    src/markerengine.cpp
    src/timeutils.cpp
    src/audiofileservice.cpp
)

set_tests_properties(pitch_note_stretch_test PROPERTIES
    LABELS "pitch;timestretch;integration;files"
    DESCRIPTION "Note pitch edits survive marker stretch/compress on a real MP3"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Точность PitchDetector на синтезированных MIDI→WAV фикстурах (C0, мелодия, vibrato, detune)
add_qt_test(pitch_detector_accuracy_test
    tests/pitch_detector_accuracy_test.cpp
    src/pitchdetector.cpp
    src/audiofileservice.cpp
)

set_tests_properties(pitch_detector_accuracy_test PROPERTIES
    LABELS "pitch;detector;accuracy;fixtures"
    DESCRIPTION "PitchDetector accuracy on generated MIDI→WAV pitch fixtures"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

set_tests_properties(beat_deviation_test PROPERTIES
    LABELS "bpm;deviation;unit"
    DESCRIPTION "Unit tests for beat deviation calculation and unaligned beat detection"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Разрез нот на пианоролле: snap к сетке / свободный рез, undo-команда
add_qt_test(pianoroll_split_test
    tests/pianoroll_split_test.cpp
    include/pitchgridwidget.h
    src/pitchgridwidget.cpp
    src/pianoroll_engine.cpp
    include/keymodulationstrip.h
    src/keymodulationstrip.cpp
    src/keyanalyzer.cpp
)

set_tests_properties(pianoroll_split_test PROPERTIES
    LABELS "pitch;pianoroll;unit"
    DESCRIPTION "Note split on the piano roll: grid snap, free cut, undo/redo"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Питчер на фикстуре tests/midi/test_1 (MIDI ground truth + WAV)
add_qt_test(midi_pitch_test
    tests/midi_pitch_test.cpp
    src/pitchdetector.cpp
    src/audiofileservice.cpp
)

set_tests_properties(midi_pitch_test PROPERTIES
    LABELS "pitch;detector;midi;fixtures;integration"
    DESCRIPTION "PitchDetector vs tests/midi/test_1.mid ground truth on test_1.wav"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Неровные доли на идеальной сетке test_1.mid (140 BPM) + искусственные сдвиги
add_qt_test(midi_beat_deviation_test
    tests/midi_beat_deviation_test.cpp
    src/bpmanalyzer.cpp
)

set_tests_properties(midi_beat_deviation_test PROPERTIES
    LABELS "bpm;deviation;midi;fixtures"
    DESCRIPTION "findUnalignedBeats on test_1.mid quarter grid (192 kHz sample map)"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# UI: отклик при перетаскивании меток и плавность воспроизведения после выравнивания
add_qt_test(ui_responsiveness_test
    tests/ui_responsiveness_test.cpp
    include/waveformview.h
    src/waveformview.cpp
    src/waveformpeaks.cpp
    src/waveformcolors.cpp
    src/beatvisualizer.cpp
    src/timestretchprocessor.cpp
    src/markerengine.cpp
    src/timeutils.cpp
    src/bpmanalyzer.cpp
    src/audiofileservice.cpp
    src/wavwriter.cpp
)

set_tests_properties(ui_responsiveness_test PROPERTIES
    LABELS "ui;integration;audio;timestretch;files"
    DESCRIPTION "UI responsiveness after beat alignment on example_V80BPM.mp3"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# ============================================================================
# Утилита разметки тестовых файлов (tests/source4test)
# ============================================================================
set(MARKER_TESTGEN_SOURCES
    tools/marker_testgen_main.cpp
    src/markertestgenwindow.cpp
    src/markersfile.cpp
    src/waveformview.cpp
    src/waveformpeaks.cpp
    src/waveformcolors.cpp
    src/markerengine.cpp
    src/timeutils.cpp
    src/bpmanalyzer.cpp
    src/audiofileservice.cpp
    src/beatvisualizer.cpp
    src/wavwriter.cpp
    src/timestretchprocessor.cpp
    src/rubberband_offline.cpp
)

set(MARKER_TESTGEN_HEADERS
    include/markertestgenwindow.h
    include/waveformview.h
)

add_executable(marker_testgen ${MARKER_TESTGEN_SOURCES} ${MARKER_TESTGEN_HEADERS})

target_include_directories(marker_testgen PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(marker_testgen PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::Concurrent
)

if(TARGET qm_dsp)
    target_link_libraries(marker_testgen PRIVATE qm_dsp)
    target_compile_definitions(marker_testgen PRIVATE USE_MIXXX_QM_DSP)
endif()

dontfloat_link_rubberband(marker_testgen)

if(WIN32)
    set_target_properties(marker_testgen PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MSVC)
        target_link_options(marker_testgen PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
    endif()
    dontfloat_deploy_qt(marker_testgen)
endif()

add_qt_test(markersfile_test
    tests/markersfile_test.cpp
    src/markersfile.cpp
    src/markerengine.cpp
    src/timeutils.cpp
)

set_tests_properties(markersfile_test PROPERTIES
    LABELS "unit;markers"
    DESCRIPTION "Markers file read/write for test fixtures"
)

# Метки растяжения на волне — тот же путь, которым их ставит плагин
add_qt_test(waveform_marker_test
    tests/waveform_marker_test.cpp
    include/waveformview.h
    src/waveformview.cpp
    src/waveformpeaks.cpp
    src/waveformcolors.cpp
    src/beatvisualizer.cpp
    src/timestretchprocessor.cpp
    src/markerengine.cpp
    src/timeutils.cpp
    src/bpmanalyzer.cpp
    src/audiofileservice.cpp
    src/wavwriter.cpp
    src/rubberband_offline.cpp
)

set_tests_properties(waveform_marker_test PROPERTIES
    LABELS "unit;markers;plugins"
    DESCRIPTION "Stretch markers added at the playhead (app and plugin path)"
)

# Экспорт нот пианоролла в MIDI (round-trip через разбор SMF из tests/midi_smf.h)
add_qt_test(midi_export_test
    tests/midi_export_test.cpp
    src/midiexporter.cpp
    src/midiimporter.cpp
    src/pitchdetector.cpp
    src/keyanalyzer.cpp
)
target_include_directories(midi_export_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)

set_tests_properties(midi_export_test PROPERTIES
    LABELS "unit;midi;pianoroll"
    DESCRIPTION "Piano roll notes exported to MIDI and reference MIDI imported back"
)

# Захват дорожки по таймлайну и распознавание переноса клипа в DAW
add_qt_test(plugin_content_shift_test
    tests/plugin_content_shift_test.cpp
    plugins/core/dontfloat_plugin_core.cpp
)
target_include_directories(plugin_content_shift_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/core
)

set_tests_properties(plugin_content_shift_test PROPERTIES
    LABELS "unit;plugins"
    DESCRIPTION "Timeline capture and clip-move detection for DAW plugins"
)

# ARA 2: плагин глазами ARA-хоста (фабрика, разбор, чтение нот, две дорожки)
if(DONTFLOAT_ARA_AVAILABLE AND TARGET dontfloat_ara)
    add_qt_test(ara_document_controller_test
        tests/ara_document_controller_test.cpp
        src/midiimporter.cpp
        src/keyanalyzer.cpp
    )
    target_link_libraries(ara_document_controller_test PRIVATE
        dontfloat_ara
        ARA_Host_Library
    )
    set_tests_properties(ara_document_controller_test PROPERTIES
        LABELS "unit;plugins;ara"
        DESCRIPTION "ARA 2 document controller seen from a host: analysis and note content"
    )

    # Мини-DAW с двумя дорожками: один документ ARA, два экземпляра плагина
    add_qt_test(mini_daw_two_tracks_test
        tests/mini_daw_two_tracks_test.cpp
        tools/mini_daw/mini_daw_ara_host.cpp
        tools/mini_daw/mini_daw_ara_host.h
    )
    target_link_libraries(mini_daw_two_tracks_test PRIVATE
        dontfloat_ara
        ARA_Host_Library
    )
    set_tests_properties(mini_daw_two_tracks_test PROPERTIES
        LABELS "unit;plugins;ara"
        DESCRIPTION "Two mini-DAW tracks in one ARA document: own audio, notes and reference"
    )

    # Ноты соседней дорожки остаются референсом: серыми, а не правимыми
    add_qt_test(plugin_reference_notes_test
        tests/plugin_reference_notes_test.cpp
        tools/mini_daw/mini_daw_ara_host.cpp
        tools/mini_daw/mini_daw_ara_host.h
    )
    dontfloat_link_plugin_ui(plugin_reference_notes_test pitcher)
    target_link_libraries(plugin_reference_notes_test PRIVATE ARA_Host_Library)
    set_tests_properties(plugin_reference_notes_test PROPERTIES
        LABELS "unit;plugins;ara;pitch"
        DESCRIPTION "Neighbour track notes stay a grey reference, never the editable set"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# Каретка в окне плагина одна на обе половины, а Play — дублёр кнопки DAW.
# Цель живёт на исходниках интерфейса плагина, поэтому только вместе с ними:
# при DONTFLOAT_BUILD_PLUGINS=OFF хелпера dontfloat_link_plugin_ui нет вовсе
if(DONTFLOAT_BUILD_PLUGINS)
    add_qt_test(plugin_transport_sync_test
        tests/plugin_transport_sync_test.cpp
    )
    dontfloat_link_plugin_ui(plugin_transport_sync_test full)

    set_tests_properties(plugin_transport_sync_test PROPERTIES
        LABELS "unit;plugins;ui"
        DESCRIPTION "Playheads of both editor halves follow each other; transport asks the host"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# Иконки кнопок: SVG рисуется без плагина движка иконок Qt
add_qt_test(svg_icon_test
    tests/svg_icon_test.cpp
    src/svgiconloader.cpp
    include/svgiconloader.h
    resources.qrc
)

set_tests_properties(svg_icon_test PROPERTIES
    LABELS "unit;ui"
    DESCRIPTION "Toolbar and transport icons render from SVG resources"
)

# Общая доска нот: ноты одного экземпляра плагина — референс для соседних
add_qt_test(plugin_shared_notes_test
    tests/plugin_shared_notes_test.cpp
    plugins/core/dontfloat_shared_notes.cpp
)
target_include_directories(plugin_shared_notes_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/core
)

set_tests_properties(plugin_shared_notes_test PROPERTIES
    LABELS "unit;plugins"
    DESCRIPTION "Notes from one plugin instance shown as reference in another"
)

# Установка переводов
install(FILES ${DONTFLOAT_QM_FILES}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/translations
    OPTIONAL
)

# Установка документации
install(FILES README.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    OPTIONAL
)

# ============================================================================
# CPack: установочные пакеты (.deb, .rpm, .tar.gz, NSIS, .dmg)
# ============================================================================
# include(CPack) стоит в самом конце файла намеренно: он записывает
# CPackConfig.cmake прямо в момент включения, поэтому всё, что задано ниже него,
# в конфигурацию уже не попадает. Раньше include шёл до этих set() — и cpack не
# видел ни сопровождающего, ни зависимостей, ни списка генераторов.
set(CPACK_PACKAGE_NAME "DONTFLOAT")
set(CPACK_PACKAGE_VENDOR "DONTFLOAT Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Аудиоредактор для выравнивания BPM")
set(CPACK_PACKAGE_DESCRIPTION "DONTFLOAT - это аудиоредактор для анализа и выравнивания BPM аудиофайлов.")
# Полная версия из project(...): иначе CPack соберёт её из MAJOR.MINOR.PATCH и
# потеряет четвёртую цифру — имена пакетов разъедутся с версией приложения
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# Windows (NSIS)
if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_PACKAGE_NAME "DONTFLOAT")
    set(CPACK_NSIS_DISPLAY_NAME "DONTFLOAT")
    set(CPACK_NSIS_HELP_LINK "https://https://github.com/ili4yov-ika/DONTFLOAT")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/ili4yov-ika/DONTFLOAT")
    set(CPACK_NSIS_CONTACT "ilichyov.kolyan@yandex.ru")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
endif()

# Linux DEB
if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB;RPM;TGZ")

    # DEB настройки
    set(CPACK_DEBIAN_PACKAGE_NAME "dontfloat")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "DONTFLOAT Project <ilichyov.kolyan@yandex.ru>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    # Список сверен с DT_NEEDED собранного бинарника и плагинов, а не с
    # target_link_libraries: Concurrent и MultimediaWidgets в него не попадают,
    # зато нужны Svg (иконки интерфейса рисует плагин qsvg) и GLX/OpenGL.
    # Альтернативы «t64 | обычное имя» — из-за переименования пакетов Qt при
    # переходе на 64-битный time_t (Debian 13, Ubuntu 24.04).
    set(_dontfloat_deb_depends
        "libqt6core6t64 (>= 6.8.0) | libqt6core6 (>= 6.8.0)"
        "libqt6gui6t64 (>= 6.8.0) | libqt6gui6 (>= 6.8.0)"
        "libqt6widgets6t64 (>= 6.8.0) | libqt6widgets6 (>= 6.8.0)"
        "libqt6multimedia6t64 (>= 6.8.0) | libqt6multimedia6 (>= 6.8.0)"
        "libqt6svg6t64 (>= 6.8.0) | libqt6svg6 (>= 6.8.0)"
        "libglx0"
        "libopengl0"
    )
    list(JOIN _dontfloat_deb_depends ", " CPACK_DEBIAN_PACKAGE_DEPENDS)
    unset(_dontfloat_deb_depends)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

    # RPM настройки
    set(CPACK_RPM_PACKAGE_NAME "dontfloat")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
    set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0+")
    # Точные зависимости по sonames rpmbuild добавляет сам; здесь — минимум
    # по именам пакетов, включая qt6-qtsvg: без него не рисуются иконки
    set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase >= 6.8.0, qt6-qtmultimedia >= 6.8.0, qt6-qtsvg >= 6.8.0")
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
endif()

# macOS
if(APPLE)
    set(CPACK_GENERATOR "DragNDrop;TGZ")
    set(CPACK_DMG_VOLUME_NAME "DONTFLOAT")
    set(CPACK_DMG_FORMAT "UDBZ")
    set(CPACK_PACKAGE_FILE_NAME "DONTFLOAT-${PROJECT_VERSION}-macOS")
endif()

# Читает всё, что задано выше, и пишет CPackConfig.cmake — строго в конце файла
include(CPack)
