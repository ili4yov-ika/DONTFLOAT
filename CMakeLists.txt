cmake_minimum_required(VERSION 3.16)

project(DONTFLOAT VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable automatic handling of Qt files
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Указываем путь для поиска UI файлов
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui)

# Устанавливаем путь к Qt6 (опционально через CMAKE_PREFIX_PATH снаружи)
# set(QT_ROOT_DIR "/opt/Qt/6.x")
# set(CMAKE_PREFIX_PATH ${QT_ROOT_DIR})

# Ensure Qt6 is available
set(QT_MIN_VERSION "6.8.0")
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Multimedia
    MultimediaWidgets
)

# QM-DSP minimal sources needed by AnalyzerQueenMaryBeats
add_library(qm_dsp STATIC
    thirdparty/mixxx/lib/qm-dsp/dsp/onsets/DetectionFunction.cpp
    thirdparty/mixxx/lib/qm-dsp/dsp/onsets/PeakPicking.cpp
    thirdparty/mixxx/lib/qm-dsp/dsp/tempotracking/TempoTrackV2.cpp
    thirdparty/mixxx/lib/qm-dsp/maths/MathUtilities.cpp
)

target_include_directories(qm_dsp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mixxx/lib/qm-dsp
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mixxx/lib/qm-dsp/include
)

# Add source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/waveformview.cpp
    src/pitchgridwidget.cpp
    src/waveformcolors.cpp
    src/bpmanalyzer.cpp
    src/audiocommand.cpp
    src/beatfixcommand.cpp
    src/mixxxbpmanalyzer.cpp
)

# Add header files
set(HEADERS
    include/mainwindow.h
    include/waveformview.h
    include/pitchgridwidget.h
    include/waveformcolors.h
    include/bpmanalyzer.h
    include/audiocommand.h
    include/beatfixcommand.h
    include/mixxxbpmanalyzer.h
)

# Add UI files
set(UI_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/mainwindow.ui
)

# Add resource files
set(RESOURCE_FILES
    resources.qrc
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
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mixxx/src
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mixxx/lib/qm-dsp
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mixxx/lib/qm-dsp/include
)

# Link Qt libraries and QM-DSP
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    qm_dsp
)

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
