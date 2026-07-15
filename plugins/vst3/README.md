# DONTFLOAT VST3

`plugins/vst3` содержит стартовую VST3-обёртку над `plugins/core`.
Полноценная сборка gated официальным VST3 SDK. Текущий target уже называется
`DONTFLOAT Track Tool` и содержит первый lightweight Qt editor shell.

## Цель

Собрать `.vst3` bundle, который можно загрузить в DAW на Windows, macOS и,
при поддержке хоста, Linux. Целевой плагин должен открывать собственный
DONTFLOAT editor UI, анализировать аудио дорожки, показывать BPM/beat grid/key
analysis, предлагать BPM alignment и отдавать результат через render/export или
host-supported workflow.

## Текущий MVP и целевой target

### DONTFLOAT Track Tool
- Текущий target: `dontfloat_track_tool_vst3`.
- Текущий realtime path: stereo passthrough, без pitch-параметров.
- Текущий editor: нативный VST3 view, который на Windows создаёт lightweight
  Qt `QWidget` без `MainWindow`.
- Основной UX: waveform, markers, beat grid, analysis и BPM alignment.
- Основа: `BPMAnalyzer`, `KeyAnalyzer`, `MarkerData`, `TimeStretchProcessor`,
  `RubberBandOffline`, `WavWriter`.
- Режим обработки: offline/render-time. Realtime `process()` не должен запускать
  тяжёлый анализ или Rubber Band stretch.

## Bundle layout

Ориентировочно:

```text
DONTFLOAT Track Tool.vst3/
└── Contents/
    ├── x86_64-win/
    │   └── DONTFLOAT Track Tool.vst3
    ├── Resources/
    └── moduleinfo.json
```

Текущий MVP собирает `DONTFLOAT Track Tool.vst3`. Для релиза нужно добавить
полноценный bundle layout. Для macOS понадобится стандартный bundle layout,
подпись и нотаризация.

## Интеграция со сборкой

VST3 подключён к `CMakeLists.txt`, но target создаётся только если включён флаг
и указан путь к SDK:

```cmake
option(DONTFLOAT_BUILD_PLUGINS "Build DAW plugins" OFF)
option(DONTFLOAT_BUILD_VST3 "Build VST3 plugin target when SDK is available" OFF)
set(DONTFLOAT_VST3_SDK_ROOT "" CACHE PATH "Optional path to VST3 SDK")
```

Сборка при наличии SDK:

```powershell
cmake -S . -B build/plugins -DDONTFLOAT_BUILD_PLUGINS=ON -DDONTFLOAT_BUILD_VST3=ON -DDONTFLOAT_VST3_SDK_ROOT=C:/path/to/vst3sdk
cmake --build build/plugins --target dontfloat_track_tool_vst3
```

Текущие файлы реализации:

- `dontfloat_vst3_track_tool.cpp` — VST3 processor/controller/native view.
- `plugins/ui/dontfloat_track_tool_editor.*` — lightweight Qt editor shell.

CMake install rule кладёт VST3 artifact в `${CMAKE_INSTALL_LIBDIR}/vst3`.
Windows installer включает опциональную секцию `DAW plugins / VST3 plugin`,
которая копирует staging-содержимое в `%CommonProgramFiles%\VST3`. Если
`DONTFLOAT_VST3_SDK_ROOT` не задан или SDK не найден, target не создаётся и
VST3 artifact не попадает в staging.

## Ограничения

- Не тянуть `MainWindow`, `.ui` файлы и playback/decode workflow в плагин.
- Editor UI отдельный и минимальный: он создаёт `QApplication` только если host
  ещё не создал Qt app, не вызывает `exec()` и не запускает тяжёлую обработку.
- Первый VST3 UI shell поддерживает Windows `HWND`; CLAP/LV2 UI workflow
  проектируется отдельно.
- Все параметры должны иметь стабильные IDs, чтобы проекты DAW открывались
  после обновлений плагина.
- Проверить лицензирование VST3 SDK вместе с GPL-кодом проекта до релиза.
- Не обещать прямое редактирование clip во всех DAW: сначала поддержать
  analysis/render/export и отдельно проверять host-specific integration.

## Минимальный proof-of-concept

1. Завершить factory/export glue по официальным шаблонам VST3 SDK.
2. Подключить analysis state, marker state и render/export commands к editor.
3. Добавить безопасное переиспользование `PitchGridWidget`/analysis panels.
4. Проверить bundle в validator из VST3 SDK и тестовом host.
