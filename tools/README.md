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

Цель собирается как обычный исполняемый файл (`MACOSX_BUNDLE FALSE`), поэтому
`deploy` сам создаёт `DONTFLOAT.app` и пишет минимальный `Contents/Info.plist`
(версия — из `project(... VERSION ...)`): без него `macdeployqt` не находит
исполняемый файл бандла. `setup_macos.sh` пишет в `~/.dontfloat_macos_env.sh`
только префикс из `brew --prefix qt@6` и предупреждает, если Qt старее
`QT_MIN_VERSION` из `CMakeLists.txt`.

## Структура (установщики)

- `build_windows_installer.bat` — сборка Windows installer (NSIS) вместе с
  CLAP/LV2/VST3 plugin targets
- `build_deb.sh` — сборка Debian/Ubuntu пакета (.deb)
- `build_rpm.sh` — сборка Fedora/RHEL пакета (.rpm)
- `setup_macos.sh` / `macos_build.sh` — окружение и сборка на macOS (см. выше)
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
  `dontfloat_pitcher.clap` + `*_clap.impl.dll` + Qt runtime рядом)
- LV2: `%CommonProgramFiles%\LV2\dontfloat*.lv2\` (DSP + UI stub/impl + Qt внутри бандла)
- VST3: `%CommonProgramFiles%\VST3\DONTFLOAT*.vst3\` (Steinberg bundle
  `Contents\x86_64-win\` + Qt внутри бандла)

`build_windows_installer.bat` собирает только приложение и plugin targets
(`DONTFLOAT_BUILD_MINI_DAW=OFF`, `DONTFLOAT_BUILD_PLUGIN_TESTER=OFF`) и после
`cmake --install` **обязан** положить Qt рядом с каждым `*.impl.dll`:

- `Qt6Core.dll` (и остальные Qt DLL)
- `platforms\qwindows.dll` — без него REAPER пишет
  *«no Qt platform plugin could be initialized»*

Код плагина (`ensureQtApplication`) при установке из свежего installer:
- направляет Qt в каталог модуля (`QT_PLUGIN_PATH` / `platforms`)
- на Windows ставит native timer для `processEvents` (без него VST3 UI
  зависает в REAPER — deadlock `attached()` ↔ message pump)

Инсталлятор обязан комплектовать runtime рядом с `*.impl.dll`. Проверка
встроена в `cmake/DeployPluginQt.cmake` и в `.bat` (сборка падает, если
файлов нет). CLAP/LV2/VST3 бинарники помечены `required` — silent skip больше
невозможен.

Для VST3 `build_windows_installer.bat` сам подхватывает `DONTFLOAT_VST3_SDK_ROOT`
или `C:\SDKs\vst3sdk`. Без SDK VST3 **не** собираются (WARN), CLAP/LV2
продолжают собираться; при наличии SDK отсутствие VST3 — **ошибка**.

Грабля cmd: внутри блока `( ... )` запись `%ERRORLEVEL%` подставляется на этапе
разбора блока — **до** запуска команды. Поиск `makensis` / `cmake` /
`windeployqt` через `where` читал код возврата предыдущей команды: обычно там
ноль, и проверка просто ничего не проверяла, а при ненулевом коде инструмент,
стоящий только в `PATH`, объявлялся ненайденным. Проверки переведены на
`!ERRORLEVEL!` (`setlocal enabledelayedexpansion` в скрипте уже был).

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

Скрипт копирует `tools/debian/` в корень проекта и отдаёт сборку
`dpkg-buildpackage` — конфигурация и компиляция описаны в `debian/rules`
(своей сборки в `build/` скрипт больше не делает, иначе проект собирался дважды).
Версия пакета берётся из `debian/changelog`; скрипт сверяет её с
`project(... VERSION ...)` в `CMakeLists.txt` и предупреждает о расхождении.

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

Скрипт собирает tarball с корневым каталогом `dontfloat-<версия>` (этого ждёт
`%setup -q`), подставляет версию из `CMakeLists.txt` в копию spec-файла и
запускает `rpmbuild -ba`; сборка целиком описана в `%build`
(`tools/rpm/dontfloat.spec`).

### Что попадает в Linux-пакеты

`debian/rules` и `%build` конфигурируют одинаково: приложение + CLAP/LV2,
`DONTFLOAT_BUILD_VST3=OFF` (нужен проприетарный Steinberg SDK),
`DONTFLOAT_BUILD_MINI_DAW=OFF`, `DONTFLOAT_BUILD_PLUGIN_TESTER=OFF`.

| Что | Куда |
| --- | --- |
| Приложение | `/usr/bin/DONTFLOAT` |
| `.desktop` | `/usr/share/applications/dontfloat.desktop` |
| Иконка | `/usr/share/icons/hicolor/scalable/apps/dontfloat.svg` |
| Переводы `.qm` | `/usr/share/DONTFLOAT/translations/` |
| CLAP | `<libdir>/clap/dontfloat*.clap` |
| LV2 | `<libdir>/lv2/dontfloat*.lv2/` |

Иконка ставится из `resources/icons/logo.svg` (`Icon=dontfloat` в `.desktop`
искал её в `hicolor`, а установки не было). SVG-иконки интерфейса требуют
плагин `qsvg` — он в `libqt6svg6` / `qt6-qtsvg`, зависимость прописана в
`debian/control` и в spec.

> В `%files` перечислено **всё**, что кладёт `make install`: непокрытый файл
> валит `rpmbuild` («Installed (but unpackaged) files found»), поэтому при
> добавлении новых `install(...)` правил обновляйте и spec.

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
