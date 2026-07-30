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

bash tools/macos_build.sh                 # Debug (preset macos-debug)
bash tools/macos_build.sh release         # Release
bash tools/macos_build.sh release test    # + ctest (QT_QPA_PLATFORM=offscreen)
bash tools/macos_build.sh release deploy  # + macdeployqt → build/macos/DONTFLOAT.app
```

Требуется: macOS 11+, Xcode CLT, Homebrew. CMake Presets: `macos-debug`, `macos-release` (`CMakePresets.json`).
Поиск Qt: `cmake/PlatformQt.cmake` (Homebrew, `~/Qt/6.x/macos`).

## Структура (установщики)

- `build_windows_installer.bat` — сборка Windows installer (NSIS) вместе с
  CLAP/LV2 plugin targets
- `build_deb.sh` — сборка Debian/Ubuntu пакета (.deb)
- `build_rpm.sh` — сборка Fedora/RHEL пакета (.rpm)
- `nsis_installer.nsi` — скрипт NSIS для Windows installer и опциональных
  секций DAW-плагинов
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

Windows installer включает страницу компонентов. Основная секция приложения
обязательная, группа `DAW plugins` опционально ставит:

- CLAP: `%CommonProgramFiles%\CLAP\` (`dontfloat.clap`, `dontfloat_scratch.clap`,
  `dontfloat_pitcher.clap` + Qt runtime рядом)
- LV2: `%CommonProgramFiles%\LV2\dontfloat*.lv2\` (binary + UI + Qt внутри бандла)
- VST3: `%CommonProgramFiles%\VST3\DONTFLOAT*.vst3\` (Steinberg bundle
  `Contents\x86_64-win\` + Qt внутри бандла)

Для VST3 перед запуском задайте `DONTFLOAT_VST3_SDK_ROOT`; без SDK этот target
пропускается, а CLAP/LV2 продолжают собираться.

**Важно:** плагины линкуются с Qt6. Инсталлятор обязан класть Qt DLL рядом с
модулем (через `windeployqt` / `cmake/DeployPluginQt.cmake`). Без этого DAW
молча пропускает плагин при сканировании. VST3 должен быть **папкой-бандлом**,
а не одним файлом-DLL в корне `VST3\`.

Если плагины «пропали» после установки, пересоберите installer или запустите
от администратора:

```powershell
tools\repair_installed_plugins.ps1
```

затем сделайте rescan в DAW.

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

## Локализация (i18n)

Исходный язык msgid — **английский** (`tr()` / `.ui`). Каталоги: `translations/en_US.ts` (identity), `translations/ru_RU.ts` (EN→RU).

```powershell
# Проверка: в UI-строках нет кириллицы (кроме автонима «Русский»)
python tools/check_tr_english_source.py

# После правок строк
lupdate -no-obsolete src include ui plugins/ui -ts translations/en_US.ts translations/ru_RU.ts
python tools/finalize_translations.py
lrelease translations/en_US.ts translations/ru_RU.ts
```

См. также: `migrate_tr_to_english.py`, `install_en_source_translations.py`, `MARKDOWN/PLAN_I18N_ENGLISH_SOURCE.md`.

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
