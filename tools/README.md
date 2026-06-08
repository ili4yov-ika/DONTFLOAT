# Инструменты проекта DONTFLOAT

Скрипты сборки пакетов, WSL-окружение и утилиты разработки.

## Утилита разметки тестов (`marker_testgen`)

CMake-цель для подготовки эталонных меток в `tests/source4test/`:

```bash
cmake --build build --config Release --target marker_testgen
./build/Release/Release/marker_testgen.exe   # VS multi-config
# или build/Release/marker_testgen.exe — в зависимости от генератора
```

Подробный workflow: [tests/source4test/README.md](../tests/source4test/README.md)

## macOS

```bash
chmod +x tools/setup_macos.sh tools/macos_build.sh
bash tools/setup_macos.sh          # Homebrew: cmake, ninja, qt@6 → ~/.dontfloat_macos_env.sh
source ~/.dontfloat_macos_env.sh

bash tools/macos_build.sh              # Debug (preset macos-debug)
bash tools/macos_build.sh release      # Release
bash tools/macos_build.sh release test # + ctest (QT_QPA_PLATFORM=offscreen)
bash tools/macos_build.sh release deploy  # + macdeployqt → build/macos/DONTFLOAT.app
```

Требуется: macOS 11+, Xcode CLT, Homebrew. CMake Presets: `macos-debug`, `macos-release` (`CMakePresets.json`).
Поиск Qt: `cmake/PlatformQt.cmake` (Homebrew, `~/Qt/6.x/macos`).

## Структура (установщики)

- `build_windows_installer.bat` — сборка Windows installer (NSIS)
- `build_deb.sh` — сборка Debian/Ubuntu пакета (.deb)
- `build_rpm.sh` — сборка Fedora/RHEL пакета (.rpm)
- `nsis_installer.nsi` — скрипт NSIS для Windows installer
- `debian/` — файлы для сборки .deb пакета
- `rpm/` — файлы для сборки .rpm пакета

## Использование

### Windows

```batch
tools\build_windows_installer.bat
```

Требуется:
- CMake
- Qt6 (MSVC 2022 64-bit)
- NSIS (Nullsoft Scriptable Install System)

### Linux (Debian/Ubuntu)

```bash
chmod +x tools/build_deb.sh
tools/build_deb.sh
```

Требуется:
- CMake
- Qt6
- dpkg-buildpackage или debuild
- devscripts

### Linux (Fedora/RHEL)

```bash
chmod +x tools/build_rpm.sh
tools/build_rpm.sh
```

Требуется:
- CMake
- Qt6
- rpmbuild
- rpmdevtools

## Альтернатива: CPack

Проект также поддерживает CPack (встроенный в CMake). После сборки проекта:

```bash
# Windows
cpack -G NSIS

# Linux
cpack -G DEB  # или -G RPM

# macOS (после Release-сборки в build/macos)
cpack -G DragNDrop
```
