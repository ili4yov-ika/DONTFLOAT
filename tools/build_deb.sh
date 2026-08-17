#!/bin/bash
# Скрипт сборки Debian/Ubuntu пакета (.deb) для DONTFLOAT
# Требуется: CMake, Qt6, dpkg-buildpackage или debuild
#
# Сборка идёт через dpkg-buildpackage: конфигурация и компиляция описаны
# в debian/rules (dh_auto_configure/dh_auto_build), локальный build/ не нужен.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DEBIAN_DIR="$SCRIPT_DIR/debian"
PACKAGE_NAME="dontfloat"
# Версия — из project(... VERSION ...) в CMakeLists.txt, чтобы не разъезжалась
VERSION="$(sed -n 's/^project(.*VERSION \([0-9.]*\).*/\1/p' "$PROJECT_ROOT/CMakeLists.txt" | head -n 1)"
if [ -z "$VERSION" ]; then
    echo "[ОШИБКА] Не удалось прочитать версию из CMakeLists.txt"
    exit 1
fi

echo "========================================"
echo "DONTFLOAT - Debian Package Builder"
echo "========================================"
echo "Версия: $VERSION"
echo

# Проверка зависимостей
command -v cmake >/dev/null 2>&1 || { echo "[ОШИБКА] CMake не найден!"; exit 1; }
command -v dpkg-buildpackage >/dev/null 2>&1 || { echo "[ОШИБКА] dpkg-buildpackage не найден! Установите: sudo apt-get install build-essential devscripts"; exit 1; }

# Копирование debian файлов в корень проекта (требуется для dpkg-buildpackage)
echo "[1/3] Подготовка debian файлов..."
if [ ! -d "$PROJECT_ROOT/debian" ]; then
    cp -r "$DEBIAN_DIR" "$PROJECT_ROOT/"
fi
# Без бита выполнения dpkg-buildpackage не запустит правила сборки
chmod +x "$PROJECT_ROOT/debian/rules"

# Версия пакета берётся из debian/changelog — сверяем с CMakeLists.txt
echo "[2/3] Проверка версии в debian/changelog..."
CHANGELOG_VERSION="$(sed -n '1s/^[^(]*(\([^)-]*\).*/\1/p' "$PROJECT_ROOT/debian/changelog")"
if [ "$CHANGELOG_VERSION" != "$VERSION" ]; then
    echo "[ПРЕДУПРЕЖДЕНИЕ] debian/changelog: $CHANGELOG_VERSION, CMakeLists.txt: $VERSION"
    echo "                 Пакет получит версию $CHANGELOG_VERSION."
    echo "                 Обновите tools/debian/changelog (dch -v \"$VERSION-1\")."
    VERSION="$CHANGELOG_VERSION"
fi

echo "[3/3] Создание .deb пакета..."
cd "$PROJECT_ROOT"
dpkg-buildpackage -b -us -uc

echo
echo "========================================"
echo "Готово! Пакет создан в: $PROJECT_ROOT/../${PACKAGE_NAME}_${VERSION}-1_amd64.deb"
echo "========================================"
echo
echo "Установка: sudo dpkg -i ../${PACKAGE_NAME}_${VERSION}-1_amd64.deb"
echo "Исправление зависимостей: sudo apt-get install -f"
