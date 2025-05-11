cmake_minimum_required(VERSION 3.16)
project(DONTFLOAT VERSION 0.1.0 LANGUAGES CXX)

# Установка стандарта C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Опция для включения тестов
option(BUILD_TESTS "Build unit tests" OFF)

# Поиск необходимых компонентов Qt
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)

# Подключение исходников
file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/*.h
)

qt6_add_resources(RESOURCES src/UI/Resources.qrc)

# Добавление основного приложения
add_executable(DONTFLOAT
    ${SOURCES}
    ${RESOURCES}
)

# Подключение Qt6 модулей
target_link_libraries(DONTFLOAT
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
)

# Подключение include-папок
target_include_directories(DONTFLOAT PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/App
    ${PROJECT_SOURCE_DIR}/src/UI
    ${PROJECT_SOURCE_DIR}/src/Audio
    ${PROJECT_SOURCE_DIR}/src/Utils
)

# Установка
install(TARGETS DONTFLOAT DESTINATION bin)

# Тесты
# if(BUILD_TESTS)
#     enable_testing()
#     find_package(Qt6 REQUIRED COMPONENTS Test)

#     file(GLOB_RECURSE TEST_SOURCES tests/*.cpp)

#     add_executable(DONTFLOAT_tests
#         ${TEST_SOURCES}
#     )

#     target_link_libraries(DONTFLOAT_tests
#         Qt6::Core
#         Qt6::Test
#     )

#     target_include_directories(DONTFLOAT_tests PRIVATE
#         ${PROJECT_SOURCE_DIR}/src
#         ${PROJECT_SOURCE_DIR}/src/Audio
#     )

#     add_test(NAME AllTests COMMAND DONTFLOAT_tests)
# endif()
