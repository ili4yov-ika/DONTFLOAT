# DONTFLOAT CLAP

`plugins/clap` содержит первый CLAP MVP над `plugins/core`.

## Почему CLAP

- Кроссплатформенный формат для Windows, macOS и Linux.
- Хорошо подходит для современных параметров, automation и event model.
- Формат проще держать независимым от GUI-приложения DONTFLOAT.

## Реализованный плагин

`DONTFLOAT Pitch Shift`:

- realtime-обработка через `GranularEngine`;
- параметры: `enabled`, `pitchSemitones`, `grainHz`, `shape`, `jitter`, `wet`,
  `prefilter`;
- stereo in / stereo out;
- без собственного editor UI на первом этапе.
- descriptor id: `com.dontfloat.pitch-shift`;
- CMake target: `dontfloat_pitch_shift_clap`;
- output: `dontfloat_pitch_shift.clap`.

`DONTFLOAT Time Stretch` лучше оставить вторым этапом: текущий Rubber Band R3
используется как offline stretch и требует отдельной streaming-архитектуры.

## Структура

Текущая структура:

```text
plugins/clap/
├── README.md
├── clap_minimal.h
└── dontfloat_clap_pitch_shift.cpp
```

CLAP-слой должен выполнять только задачи формата:

- объявление plugin descriptor;
- создание и уничтожение инстанса;
- обработка аудиобуферов и events;
- синхронизация параметров с host automation;
- сохранение и восстановление state.

DSP должен оставаться в `plugins/core`.

## Realtime-правила

- `process()` не выделяет память.
- Параметры передаются lock-free или через заранее подготовленные snapshots.
- Никакого доступа к Qt UI, файлам, настройкам приложения или логированию в
  audio callback.
- Все буферы и временные структуры подготавливаются в `activate()` /
  `start_processing()`.

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
cmake --build build/plugins --target dontfloat_pitch_shift_clap clap_pitch_shift_smoke_test
ctest --test-dir build/plugins -R clap_pitch_shift_smoke_test --output-on-failure
```

Install rule кладёт CLAP module в `${CMAKE_INSTALL_LIBDIR}/clap` или
`${CMAKE_INSTALL_BINDIR}/clap` на Windows.

## Проверка

1. Собрать CLAP target.
2. Проверить descriptor в CLAP validator / тестовом host.
3. Сравнить render результата с прямым вызовом `plugins/core`.
4. Проверить automation каждого параметра.
