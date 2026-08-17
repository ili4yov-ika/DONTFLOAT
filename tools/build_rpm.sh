#!/bin/bash
# Скрипт сборки RPM пакета для DONTFLOAT (Fedora/RHEL)
# Требуется: CMake, Qt6, rpmbuild
#
# Собирает пакет ТОЛЬКО через rpmbuild: конфигурация и компиляция описаны
# в tools/rpm/dontfloat.spec (%build), локальный build/ не используется.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RPM_DIR="$SCRIPT_DIR/rpm"
PACKAGE_NAME="dontfloat"
# Версия — из project(... VERSION ...) в CMakeLists.txt, чтобы не разъезжалась
VERSION="$(sed -n 's/^project(.*VERSION \([0-9.]*\).*/\1/p' "$PROJECT_ROOT/CMakeLists.txt" | head -n 1)"
if [ -z "$VERSION" ]; then
    echo "[ОШИБКА] Не удалось прочитать версию из CMakeLists.txt"
    exit 1
fi
RELEASE="1"

echo "========================================"
echo "DONTFLOAT - RPM Package Builder"
echo "========================================"
echo "Версия: $VERSION"
echo

# Проверка зависимостей
command -v cmake >/dev/null 2>&1 || { echo "[ОШИБКА] CMake не найден!"; exit 1; }
command -v rpmbuild >/dev/null 2>&1 || { echo "[ОШИБКА] rpmbuild не найден! Установите: sudo dnf install rpm-build rpmdevtools"; exit 1; }

# Настройка RPM build окружения
echo "[1/4] Настройка RPM build окружения..."
RPMBUILD_DIR="$HOME/rpmbuild"
mkdir -p "$RPMBUILD_DIR"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

# Создание tarball исходников.
# Корневой каталог в архиве обязан называться <name>-<version>: этого ждёт
# %setup -q в spec-файле (иначе rpmbuild не найдёт распакованные исходники).
echo "[2/4] Создание исходного архива..."
cd "$PROJECT_ROOT/.."
SOURCE_DIR_NAME="$(basename "$PROJECT_ROOT")"
tar --exclude='.git' \
    --exclude='build' \
    --exclude='*.o' \
    --exclude='*.a' \
    --transform "s,^${SOURCE_DIR_NAME},${PACKAGE_NAME}-${VERSION}," \
    -czf "$RPMBUILD_DIR/SOURCES/${PACKAGE_NAME}-${VERSION}.tar.gz" \
    "$SOURCE_DIR_NAME"

# Копирование spec файла с подстановкой версии
echo "[3/4] Подготовка spec файла..."
sed -e "s/^%define version .*/%define version ${VERSION}/" \
    -e "s/^%define release .*/%define release ${RELEASE}/" \
    "$RPM_DIR/${PACKAGE_NAME}.spec" > "$RPMBUILD_DIR/SPECS/${PACKAGE_NAME}.spec"

# Сборка RPM пакета (конфигурация + компиляция внутри %build spec-файла)
echo "[4/4] Создание RPM пакета..."
cd "$RPMBUILD_DIR"
rpmbuild -ba "SPECS/${PACKAGE_NAME}.spec"

echo
echo "========================================"
echo "Готово! RPM пакет создан в:"
echo "  $RPMBUILD_DIR/RPMS/x86_64/${PACKAGE_NAME}-${VERSION}-${RELEASE}*.x86_64.rpm"
echo "========================================"
echo
echo "Установка: sudo dnf install $RPMBUILD_DIR/RPMS/x86_64/${PACKAGE_NAME}-${VERSION}-${RELEASE}*.x86_64.rpm"
