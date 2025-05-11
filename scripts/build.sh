#!/bin/bash

set -e

# Настройки
BUILD_DIR="build"
INSTALL_DIR="dist"
CMAKE_OPTIONS="-DCMAKE_BUILD_TYPE=Release"

# Очистка и создание каталога сборки
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Конфигурация проекта
cmake .. $CMAKE_OPTIONS

# Сборка
cmake --build . --parallel

# Установка (если CMakeLists поддерживает install)
cmake --install . --prefix "../$INSTALL_DIR"

echo "Build completed. Binaries are in $BUILD_DIR/, installed files in $INSTALL_DIR/"
