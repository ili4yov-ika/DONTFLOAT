#!/bin/bash

set -e

APP_NAME="DONTFLOAT"
VERSION="1.0.0"
OUTPUT_DIR="package"
BUILD_DIR="build"
DIST_DIR="dist"

# Очистка
rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"

# Копирование собранных файлов
cp -r "$DIST_DIR" "$OUTPUT_DIR/$APP_NAME-$VERSION"

# Копирование ресурсов
cp -r README.md LICENSE "$OUTPUT_DIR/$APP_NAME-$VERSION/"

# Упаковка
cd "$OUTPUT_DIR"
tar -czf "$APP_NAME-$VERSION.tar.gz" "$APP_NAME-$VERSION"

echo "Package created: $OUTPUT_DIR/$APP_NAME-$VERSION.tar.gz"
