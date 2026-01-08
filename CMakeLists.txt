cmake_minimum_required(VERSION 3.16)

project(DONTFLOAT VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable automatic handling of Qt files
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Указываем путь для поиска UI файлов
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui)

# Устанавливаем путь к Qt6 (автоматическое определение)
if(EXISTS "C:/Qt/6.9.3/msvc2022_64")
    set(QT_ROOT_DIR "C:/Qt/6.9.3/msvc2022_64")
elseif(EXISTS "C:/Qt/6.8.3/msvc2022_64")
    set(QT_ROOT_DIR "C:/Qt/6.8.3/msvc2022_64")
else()
    message(FATAL_ERROR "Qt не найден! Установите Qt 6.8.3 или 6.9.3 с поддержкой MSVC 2022 64-bit")
endif()

set(CMAKE_PREFIX_PATH ${QT_ROOT_DIR})
set(CMAKE_PREFIX_PATH ${QT_ROOT_DIR} CACHE PATH "Qt6 installation path" FORCE)
message(STATUS "Используется Qt: ${QT_ROOT_DIR}")

# Генерируем compile_commands.json для IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Ensure Qt6 is available
set(QT_MIN_VERSION "6.8.0")
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Multimedia
    MultimediaWidgets
    Test
)

# Add source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/waveformview.cpp
    src/markerstretchengine.cpp
    src/pitchgridwidget.cpp
    src/waveformcolors.cpp
    src/bpmanalyzer.cpp
    src/keyanalyzer.cpp
    src/waveformanalyzer.cpp
    src/audiocommand.cpp
    src/loadfiledialog.cpp
    src/metronomesettingsdialog.cpp
    src/metronomecontroller.cpp
    src/beatfixcommand.cpp
    src/beatvisualizer.cpp
    src/timestretchprocessor.cpp
    src/timeutils.cpp
)

# Add header files
set(HEADERS
    include/mainwindow.h
    include/waveformview.h
    include/pitchgridwidget.h
    include/waveformcolors.h
    include/bpmanalyzer.h
    include/keyanalyzer.h
    include/waveformanalyzer.h
    include/audiocommand.h
    include/loadfiledialog.h
    include/metronomesettingsdialog.h
    include/metronomecontroller.h
    include/beatfixcommand.h
    include/beatvisualizer.h
    include/timestretchprocessor.h
    include/markerstretchengine.h
    include/timeutils.h
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
    translations/DONTFLOAT_ru_RU.ts
    translations/DONTFLOAT_en_US.ts
)

# Create executable
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${UI_FILES}
    ${RESOURCE_FILES}
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/Desktop_Qt_6_8_3_MSVC2022_64bit-Debug
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_autogen/include_Debug
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_autogen/include_Release
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_autogen
)

# Link Qt libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)

# Translation files are handled manually

# Third-party: Mixxx qm-dsp
set(QM_DSP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mixxx/lib/qm-dsp)
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
        target_compile_definitions(qm_dsp PRIVATE USE_PTHREADS)
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

# Set Windows executable properties
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
        MACOSX_BUNDLE FALSE
    )
endif()

# Install rules
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Enable testing
enable_testing()

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
        Qt6::Test
    )

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

    # Добавляем тест в CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
    
    # Устанавливаем рабочую директорию для тестов (чтобы они могли найти файлы из source4test)
    set_tests_properties(${test_name} PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endfunction()

# BPM Analyzer тест (работа с реальными файлами из source4test)
add_qt_test(bpm_analyzer_test
    tests/bpm_analyzer_test.cpp
    src/bpmanalyzer.cpp
)

set_tests_properties(bpm_analyzer_test PROPERTIES
    LABELS "bpm;audio;integration;files"
    DESCRIPTION "BPM analyzer tests on real audio files from tests/source4test"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
