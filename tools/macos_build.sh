#!/usr/bin/env bash
# Сборка DONTFLOAT на macOS (CMake Presets + Ninja).
#
# Использование:
#   bash tools/macos_build.sh              # Debug
#   bash tools/macos_build.sh release      # Release
#   bash tools/macos_build.sh debug test   # Debug + ctest
#   bash tools/macos_build.sh release deploy  # Release + macdeployqt
#   bash tools/macos_build.sh release pkg     # Release + macdeployqt + .pkg
#   bash tools/macos_build.sh release pkg dmg # + образ .dmg рядом с .pkg

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ENV_FILE="${HOME}/.dontfloat_macos_env.sh"

if [[ -f "$ENV_FILE" ]]; then
    # shellcheck disable=SC1090
    source "$ENV_FILE"
fi

BUILD_TYPE="debug"
RUN_TEST=0
DEPLOY=0
MAKE_PKG=0
MAKE_DMG=0

for arg in "$@"; do
    case "$arg" in
        release|Release) BUILD_TYPE="release" ;;
        debug|Debug) BUILD_TYPE="debug" ;;
        test) RUN_TEST=1 ;;
        deploy) DEPLOY=1 ;;
        # .pkg и .dmg собираются из готового бандла, поэтому deploy обязателен
        pkg) DEPLOY=1; MAKE_PKG=1 ;;
        dmg) DEPLOY=1; MAKE_DMG=1 ;;
        *)
            echo "Неизвестный аргумент: $arg"
            echo "Использование: $0 [debug|release] [test] [deploy|pkg|dmg]"
            exit 1
            ;;
    esac
done

PRESET="macos-${BUILD_TYPE}"
BUILD_DIR="${PROJECT_ROOT}/build/macos"

cd "$PROJECT_ROOT"

echo "=== DONTFLOAT macOS: preset ${PRESET} ==="
cmake --preset "${PRESET}"
cmake --build --preset "${PRESET}" --parallel

APP_BIN="${BUILD_DIR}/DONTFLOAT"
if [[ ! -x "$APP_BIN" ]]; then
    echo "[ОШИБКА] Исполняемый файл не найден: $APP_BIN"
    exit 1
fi

echo "Собрано: $APP_BIN"

if [[ "$RUN_TEST" -eq 1 ]]; then
    echo "=== CTest (QT_QPA_PLATFORM=offscreen) ==="
    QT_QPA_PLATFORM=offscreen ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

if [[ "$DEPLOY" -eq 1 ]]; then
    MACDEPLOYQT="$(command -v macdeployqt || true)"
    if [[ -z "$MACDEPLOYQT" && -n "${CMAKE_PREFIX_PATH:-}" ]]; then
        MACDEPLOYQT="${CMAKE_PREFIX_PATH}/bin/macdeployqt"
    fi
    if [[ ! -x "$MACDEPLOYQT" ]]; then
        echo "[ПРЕДУПРЕЖДЕНИЕ] macdeployqt не найден — пропуск deploy"
    else
        APP_DIR="${BUILD_DIR}/DONTFLOAT.app"
        VERSION="$(sed -n 's/^project(.*VERSION \([0-9.]*\).*/\1/p' "${PROJECT_ROOT}/CMakeLists.txt" | head -n 1)"
        mkdir -p "${APP_DIR}/Contents/MacOS" "${APP_DIR}/Contents/Resources"
        cp "$APP_BIN" "${APP_DIR}/Contents/MacOS/DONTFLOAT"
        # Цель собирается как обычный исполняемый файл (MACOSX_BUNDLE FALSE),
        # поэтому Info.plist пишем сами — без него macdeployqt не находит
        # исполняемый файл бандла и падает.
        cat >"${APP_DIR}/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key><string>DONTFLOAT</string>
    <key>CFBundleIdentifier</key><string>org.dontfloat.DONTFLOAT</string>
    <key>CFBundleName</key><string>DONTFLOAT</string>
    <key>CFBundleDisplayName</key><string>DONTFLOAT</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>${VERSION:-0.1.0.0}</string>
    <key>CFBundleVersion</key><string>${VERSION:-0.1.0.0}</string>
    <key>LSMinimumSystemVersion</key><string>11.0</string>
    <key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
EOF
        "$MACDEPLOYQT" "$APP_DIR"
        echo "Развёрнуто: $APP_DIR"

        if [[ "$MAKE_PKG" -eq 1 ]]; then
            # Установщик macOS: бандл со всеми Qt-библиотеками внутри кладётся
            # в /Applications. Ставим именно .app, а не голый бинарник в
            # /usr/local — иначе Qt рядом с ним искать негде.
            PKG_ROOT="${BUILD_DIR}/pkgroot"
            PKG_OUT="${BUILD_DIR}/DONTFLOAT-${VERSION:-0.1.0.0}-macOS.pkg"
            rm -rf "$PKG_ROOT"
            mkdir -p "$PKG_ROOT"
            cp -R "$APP_DIR" "$PKG_ROOT/"

            pkgbuild --root "$PKG_ROOT" \
                     --install-location /Applications \
                     --identifier org.dontfloat.DONTFLOAT \
                     --version "${VERSION:-0.1.0.0}" \
                     "$PKG_OUT"
            echo "Пакет: $PKG_OUT"
        fi

        if [[ "$MAKE_DMG" -eq 1 ]]; then
            # Образ с тем же бандлом: привычный для macOS способ раздачи —
            # открыл и перетащил в «Программы». CPack-генератор DragNDrop сюда
            # не годится: он пакует дерево установки CMake, где Qt рядом нет.
            DMG_OUT="${BUILD_DIR}/DONTFLOAT-${VERSION:-0.1.0.0}-macOS.dmg"
            DMG_ROOT="${BUILD_DIR}/dmgroot"
            rm -rf "$DMG_ROOT" "$DMG_OUT"
            mkdir -p "$DMG_ROOT"
            cp -R "$APP_DIR" "$DMG_ROOT/"
            ln -s /Applications "$DMG_ROOT/Applications"

            hdiutil create -volname "DONTFLOAT" \
                           -srcfolder "$DMG_ROOT" \
                           -ov -format UDZO \
                           "$DMG_OUT"
            echo "Образ: $DMG_OUT"
        fi
    fi
fi
