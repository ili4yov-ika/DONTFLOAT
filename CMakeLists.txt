cmake_minimum_required(VERSION 3.16)

project(DONTFLOAT VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable automatic handling of Qt files
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Указываем путь для поиска UI файлов
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui)

# Используем переменную окружения Qt6_DIR если она установлена
if(DEFINED ENV{Qt6_DIR})
    set(Qt6_DIR $ENV{Qt6_DIR})
endif()

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
)

# Add header files
set(HEADERS
    include/mainwindow.h
    include/waveformview.h
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
    ${Qt6Core_INCLUDE_DIRS}
    ${Qt6Gui_INCLUDE_DIRS}
    ${Qt6Widgets_INCLUDE_DIRS}
    ${Qt6Multimedia_INCLUDE_DIRS}
    ${Qt6MultimediaWidgets_INCLUDE_DIRS}
)

# Link Qt libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)

# Set Windows executable properties
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
        MACOSX_BUNDLE FALSE
    )

    # Добавляем пути к заголовочным файлам Qt явно
    target_include_directories(${PROJECT_NAME} PRIVATE
        $ENV{Qt6_DIR}/../../include
        $ENV{Qt6_DIR}/../../include/QtCore
        $ENV{Qt6_DIR}/../../include/QtGui
        $ENV{Qt6_DIR}/../../include/QtWidgets
        $ENV{Qt6_DIR}/../../include/QtMultimedia
        $ENV{Qt6_DIR}/../../include/QtMultimediaWidgets
    )
endif()

# Install rules
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Копирование DLL для Windows
if(WIN32)
    set(QT_INSTALL_PATH "${Qt6_DIR}/../..")
    if (EXISTS "${QT_INSTALL_PATH}")
        set(QT_BIN_PATH "${QT_INSTALL_PATH}/bin")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_BIN_PATH}/Qt6Core.dll"
                "${QT_BIN_PATH}/Qt6Gui.dll"
                "${QT_BIN_PATH}/Qt6Widgets.dll"
                "${QT_BIN_PATH}/Qt6Multimedia.dll"
                "${QT_BIN_PATH}/Qt6MultimediaWidgets.dll"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
        )
    endif()
endif()