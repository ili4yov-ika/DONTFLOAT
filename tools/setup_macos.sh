#!/usr/bin/env bash
# Настройка macOS для сборки DONTFLOAT (Homebrew: Qt 6, CMake, Ninja).
# Запуск:
#   bash tools/setup_macos.sh
#   source ~/.dontfloat_macos_env.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ENV_FILE="${HOME}/.dontfloat_macos_env.sh"

echo "========================================"
echo "DONTFLOAT — настройка macOS"
echo "========================================"
echo "Проект: $PROJECT_ROOT"
echo

if ! command -v brew >/dev/null 2>&1; then
    echo "[ОШИБКА] Homebrew не найден. Установите: https://brew.sh"
    exit 1
fi

echo "[1/3] Пакеты Homebrew..."
brew update
brew install cmake ninja pkg-config qt@6

QT_PREFIX="$(brew --prefix qt@6)"
echo
echo "[2/3] Запись окружения в $ENV_FILE ..."
# Только префикс из brew: вторая строка с /opt/homebrew + /usr/local ставила
# в начало PATH чужой Qt (на Intel/ARM с двумя установками брался не тот)
cat >"$ENV_FILE" <<EOF
# DONTFLOAT macOS (сгенерировано tools/setup_macos.sh)
export PATH="${QT_PREFIX}/bin:\$PATH"
export CMAKE_PREFIX_PATH="${QT_PREFIX}"
export QT_PLUGIN_PATH="${QT_PREFIX}/plugins"
EOF

# shellcheck disable=SC1090
source "$ENV_FILE"

echo "[3/3] Проверка Qt..."
if ! cmake --version >/dev/null 2>&1; then
    echo "[ОШИБКА] cmake недоступен после установки"
    exit 1
fi

if [[ ! -f "${QT_PREFIX}/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
    echo "[ОШИБКА] Qt6 не найден в ${QT_PREFIX}"
    exit 1
fi

# Проект требует Qt >= QT_MIN_VERSION из CMakeLists.txt
QT_MIN_VERSION="$(sed -n 's/^set(QT_MIN_VERSION "\([0-9.]*\)").*/\1/p' "${PROJECT_ROOT}/CMakeLists.txt" | head -n 1)"
QT_VERSION="$(qmake6 -query QT_VERSION 2>/dev/null || qmake -query QT_VERSION 2>/dev/null || true)"
if [[ -n "$QT_MIN_VERSION" && -n "$QT_VERSION" ]]; then
    OLDEST="$(printf '%s\n%s\n' "$QT_MIN_VERSION" "$QT_VERSION" | sort -V | head -n 1)"
    if [[ "$OLDEST" != "$QT_MIN_VERSION" ]]; then
        echo "[ПРЕДУПРЕЖДЕНИЕ] Qt ${QT_VERSION} старее требуемой ${QT_MIN_VERSION} — CMake откажется конфигурировать"
    else
        echo "Qt ${QT_VERSION} (требуется >= ${QT_MIN_VERSION})"
    fi
fi

echo
echo "Готово. Добавьте в ~/.zshrc:"
echo "  source \"$ENV_FILE\""
echo
echo "Сборка:"
echo "  bash tools/macos_build.sh"
echo "  # или: cmake --preset macos-debug && cmake --build --preset macos-debug"
