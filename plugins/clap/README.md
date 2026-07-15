# DONTFLOAT CLAP

`plugins/clap` содержит первый CLAP MVP над `plugins/core`. Текущий код
проверяет audio processing entry point, audio ports и factory для
`DONTFLOAT Track Tool`: будущего editor UI для анализа дорожки, BPM alignment и
render/export workflow.

## Почему CLAP

- Кроссплатформенный формат для Windows, macOS и Linux.
- Хорошо подходит для современных параметров, automation и event model.
- Формат проще держать независимым от GUI-приложения DONTFLOAT.

## Реализованный плагин

Текущий технический MVP — `DONTFLOAT Track Tool`:

- stereo in / stereo out passthrough в realtime path;
- без pitch-параметров и без тяжёлого анализа в `process()`;
- lightweight Qt editor shell через `clap.gui` extension;
- descriptor id: `com.dontfloat.track-tool`;
- CMake target: `dontfloat_track_tool_clap`;
- output: `dontfloat_track_tool.clap`.

Следующий слой: анализ аудио host-дорожки, состояние
анализа/меток и offline/render-time операции.

## Структура

Текущая структура:

```text
plugins/clap/
├── README.md
├── clap_minimal.h
├── dontfloat_clap_track_tool.cpp
├── ../ui/dontfloat_track_tool_editor.*
├── ../ui/dontfloat_qt_hosting.*
└── tests/
```

CLAP-слой должен выполнять только задачи формата:

- объявление plugin descriptor;
- создание и уничтожение инстанса;
- обработка аудиобуферов и events;
- синхронизация параметров с host automation;
- сохранение и восстановление state.
- открытие plugin editor через `clap.gui` и обмен командами с Track Tool
  session.

DSP должен оставаться в `plugins/core`.

## Realtime-правила

- `process()` не выделяет память.
- Параметры передаются lock-free или через заранее подготовленные snapshots.
- Никакого доступа к Qt UI, файлам, настройкам приложения или логированию в
  audio callback.
- Все буферы и временные структуры подготавливаются в `activate()` /
  `start_processing()`.
- BPM/key analysis, Rubber Band stretch и export должны идти вне `process()`.

## Целевой Track Tool

CLAP-версия должна использовать преимущества формата для editor/state workflow:

- plugin editor показывает waveform, beat grid, markers и анализ тональности;
- `process()` остаётся лёгким и realtime-safe;
- тяжёлый анализ запускается по команде UI или host-supported offline workflow;
- результат BPM alignment возвращается через render/export или возможности host,
  которые будут проверены отдельно.

## Интеграция со сборкой

CLAP подключён к корневому `CMakeLists.txt` за опциями:

```cmake
option(DONTFLOAT_BUILD_PLUGINS "Build DAW plugins" OFF)
option(DONTFLOAT_BUILD_CLAP "Build CLAP plugin target when plugins are enabled" ON)
set(DONTFLOAT_CLAP_SDK_ROOT "" CACHE PATH "Optional path to official CLAP SDK")
```

Сейчас используется локальный минимальный ABI subset `clap_minimal.h`, чтобы
MVP собирался без скачивания SDK. `DONTFLOAT_CLAP_SDK_ROOT` зарезервирован для
перехода на официальный SDK.

Сборка:

```powershell
cmake -S . -B build/plugins -DDONTFLOAT_BUILD_PLUGINS=ON -DDONTFLOAT_BUILD_CLAP=ON
cmake --build build/plugins --target dontfloat_track_tool_clap clap_track_tool_smoke_test
ctest --test-dir build/plugins -R clap_track_tool_smoke_test --output-on-failure
```

Install rule кладёт CLAP module в `${CMAKE_INSTALL_LIBDIR}/clap`.
Windows NSIS installer копирует его из staging в
`%CommonProgramFiles%\CLAP\dontfloat_track_tool.clap` через опциональную
секцию `DAW plugins / CLAP plugin`.

## Проверка

1. Собрать CLAP target.
2. Проверить descriptor в CLAP validator / тестовом host.
3. Проверить audio ports и passthrough process path.
4. Проверить `clap.gui` в CLAP validator / тестовом host.
5. После добавления state API проверить automation/state workflow.
