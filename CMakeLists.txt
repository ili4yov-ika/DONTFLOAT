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

# Устанавливаем путь к Qt6
set(QT_ROOT_DIR "F:/Qt/6.8.3/msvc2022_64")
set(CMAKE_PREFIX_PATH ${QT_ROOT_DIR})

# Ensure Qt6 is available
set(QT_MIN_VERSION "6.8.0")
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Multimedia
    MultimediaWidgets
)

# Add source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/waveformview.cpp
    src/pitchgridwidget.cpp
    src/waveformcolors.cpp
    src/bpmanalyzer.cpp
    src/keyanalyzer.cpp
    src/waveformanalyzer.cpp
    src/audiocommand.cpp
    src/loadfiledialog.cpp
    src/metronomesettingsdialog.cpp
    src/beatfixcommand.cpp
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
    include/beatfixcommand.h
)

# Add UI files
set(UI_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/mainwindow.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/loadfiledialog.ui
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

# Third-party: Mixxx qm-dsp (for Mixxx-style BPM analysis)
set(QM_DSP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mixxx/lib/qm-dsp)
if(EXISTS ${QM_DSP_ROOT})
    file(GLOB_RECURSE QM_DSP_SOURCES
        ${QM_DSP_ROOT}/*.cpp
        ${QM_DSP_ROOT}/*.c
    )
    add_library(qm_dsp STATIC ${QM_DSP_SOURCES})
    target_include_directories(qm_dsp PUBLIC ${QM_DSP_ROOT})

    # Enable Mixxx algorithm in our analyzer
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_MIXXX_QM_DSP)
    target_link_libraries(${PROJECT_NAME} PRIVATE qm_dsp)

    message(STATUS "Mixxx qm-dsp enabled: ${QM_DSP_ROOT}")
else()
    message(WARNING "Mixxx qm-dsp not found at ${QM_DSP_ROOT}. Building with simplified BPM analyzer.")
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
