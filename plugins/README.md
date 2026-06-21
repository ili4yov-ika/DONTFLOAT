# Плагины DONTFLOAT

Каталог содержит первый слой плагинов для DAW. По умолчанию plugin targets
не собираются и не влияют на обычный `DONTFLOAT`; включаются через CMake option
`DONTFLOAT_BUILD_PLUGINS`.

Цель этого каталога — держать отдельные plugin targets, которые используют уже
существующие DSP-ядра проекта без зависимости от GUI.

## Возможные форматы

### VST3
- Основной формат для Windows и macOS, также поддерживается многими DAW на Linux.
- Ожидаемая структура: `.vst3` bundle с тонкой обёрткой над общим DSP-ядром.
- Начат SDK-gated target `dontfloat_pitch_shift_vst3` (`DONTFLOAT_BUILD_VST3=ON`
  + `DONTFLOAT_VST3_SDK_ROOT`).

### CLAP
- Перспективный кроссплатформенный формат для Windows, macOS и Linux.
- Подходит для более современного event/model API и future-proof интеграции.
- Первый MVP реализован как `dontfloat_pitch_shift_clap`.

### LV2
- В первую очередь Linux-экосистема.
- Можно рассматривать как отдельную цель после выделения общего plugin core.
- Первый MVP реализован как `dontfloat_pitch_shift_lv2` с `.ttl` manifest.

## Что уже есть в проекте

Эти компоненты можно вынести в общее ядро для плагинов:

| Компонент | Файлы | Назначение |
|---|---|---|
| Time stretch с тонкомпенсацией | `TimeStretchProcessor`, `RubberBandOffline` | Офлайн сжатие/растяжение по коэффициенту или меткам через Rubber Band R3 |
| Гранулярный pitch shift | `include/granularpitchshifter_engine.h` | Сдвиг высоты в полутонах, grain rate, shape, jitter, wet, prefilter |
| BPM / beat grid | `BPMAnalyzer` | Анализ BPM, долей, отклонений и опорной точки сетки через qm-dsp |
| Анализ тональности | `KeyAnalyzer` | Основная/вторичная тональность, chroma vector |
| Маркеры растяжения | `MarkerData`, `MarkerEngine` | Данные меток и расчёт сегментов для stretch |
| Экспорт WAV | `WavWriter` | Запись обработанного аудио во временный или пользовательский WAV |

Важно: часть этих классов использует Qt-типы (`QVector`, `QString`). Для
плагинов лучше выделить слой `plugin_core` с минимальными зависимостями:
`std::vector<float>`, plain structs, без `QWidget`, `QMediaPlayer` и `QSettings`.

## Предлагаемая структура

```text
plugins/
├── README.md
├── core/
│   ├── README.md
│   ├── dontfloat_plugin_core.h
│   ├── dontfloat_plugin_core.cpp
│   └── tests/
├── vst3/
│   ├── README.md
│   └── dontfloat_vst3_pitch_shift.cpp
├── clap/
│   ├── README.md
│   ├── clap_minimal.h
│   └── dontfloat_clap_pitch_shift.cpp
└── lv2/
    ├── README.md
    ├── lv2_minimal.h
    ├── dontfloat_lv2_pitch_shift.cpp
    └── dontfloat_pitch_shift.lv2/
```

`core/` должен содержать только DSP и сериализуемые параметры. Форматные
обёртки (`vst3/`, `clap/`, `lv2/`) должны заниматься только API хоста:
инициализация, параметры, аудиобуферы, состояние пресета.

Локальная документация:

- [`core/README.md`](core/README.md) — общий DSP-слой без GUI.
- [`vst3/README.md`](vst3/README.md) — будущая VST3-обёртка.
- [`clap/README.md`](clap/README.md) — CLAP MVP для pitch shift.
- [`lv2/README.md`](lv2/README.md) — будущий LV2 bundle.

## Сборка

По умолчанию плагины отключены. Включить:

```powershell
cmake -S . -B build/plugins -DDONTFLOAT_BUILD_PLUGINS=ON -DDONTFLOAT_BUILD_CLAP=ON -DDONTFLOAT_BUILD_LV2=ON
cmake --build build/plugins --target plugin_core_pitch_shift_test dontfloat_pitch_shift_clap dontfloat_pitch_shift_lv2
ctest --test-dir build/plugins -R plugin_core_pitch_shift_test --output-on-failure
```

Цели:

- `dontfloat_plugin_core` — static library общего DSP API.
- `dontfloat_pitch_shift_clap` — CLAP module `dontfloat_pitch_shift.clap`.
- `dontfloat_pitch_shift_lv2` — LV2 module и bundle metadata.
- `dontfloat_pitch_shift_vst3` — VST3 starter target, только при доступном SDK.
- `plugin_core_pitch_shift_test` — smoke-тест core.
- `clap_pitch_shift_smoke_test` — проверка CLAP factory, extensions и process path.
- `lv2_pitch_shift_smoke_test` — проверка LV2 descriptor, instantiate и run path.

Дополнительные CMake-переменные:

- `DONTFLOAT_BUILD_PLUGINS=ON/OFF` — включает plugin targets.
- `DONTFLOAT_BUILD_CLAP=ON/OFF` — включает CLAP target.
- `DONTFLOAT_BUILD_LV2=ON/OFF` — включает LV2 target.
- `DONTFLOAT_BUILD_VST3=ON/OFF` — включает VST3 target при наличии SDK.
- `DONTFLOAT_CLAP_SDK_ROOT` — зарезервировано под официальный CLAP SDK.
- `DONTFLOAT_VST3_SDK_ROOT` — зарезервировано под VST3 SDK.

Установка:

- CLAP: `${CMAKE_INSTALL_LIBDIR}/clap`.
- LV2: `${CMAKE_INSTALL_LIBDIR}/lv2/dontfloat_pitch_shift.lv2`.
- VST3: `${CMAKE_INSTALL_LIBDIR}/vst3`, если доступен `DONTFLOAT_VST3_SDK_ROOT`.

### Windows NSIS

`tools/build_windows_installer.bat` включает `DONTFLOAT_BUILD_PLUGINS=ON`,
`DONTFLOAT_BUILD_CLAP=ON`, `DONTFLOAT_BUILD_LV2=ON` и
`DONTFLOAT_BUILD_VST3=ON`. Если переменная окружения `DONTFLOAT_VST3_SDK_ROOT`
не задана, VST3 target пропускается, а CLAP/LV2 продолжают собираться.

`tools/nsis_installer.nsi` показывает страницу компонентов. Основное приложение
устанавливается обязательно, а группа `DAW plugins` содержит опциональные
секции:

- `CLAP plugin` → `%CommonProgramFiles%\CLAP\dontfloat_pitch_shift.clap`;
- `LV2 plugin` → `%CommonProgramFiles%\LV2\dontfloat_pitch_shift.lv2`;
- `VST3 plugin` → `%CommonProgramFiles%\VST3`.

## Первые кандидаты для плагинов

### DONTFLOAT Time Stretch
- Параметры: `timeRatio`, `preservePitch`, optional marker map.
- Основа: `TimeStretchProcessor::processChannels()` и
  `TimeStretchProcessor::applyMarkerStretch()`.
- Реалистичный режим: offline / render-time. Для realtime нужны отдельные
  ограничения по latency и буферизации.

### DONTFLOAT Pitch Shift
- Параметры: `enabled`, `pitchSemitones`, `grainHz`, `shape`, `jitter`, `wet`,
  `prefilter`.
- Основа: `GranularEngine::Params` и `GranularEngine::applyPitchShiftQt()`.
- Лучше подходит для realtime, чем Rubber Band offline stretch.
- Реализовано в MVP через `PitchShiftProcessor` и CLAP wrapper.

### DONTFLOAT Analyzer
- Параметры: min/max BPM, fixed tempo, trust file BPM, key analysis.
- Основа: `BPMAnalyzer` и `KeyAnalyzer`.
- Может быть отдельным utility-плагином или offline анализатором, если формат
  хоста поддерживает такой workflow.

## Как это должно работать

### Windows
- Windows installer собирает plugin targets и предлагает отдельную группу
  `DAW plugins` для CLAP/LV2/VST3.
- CLAP устанавливается в `%CommonProgramFiles%\CLAP`, LV2 — в
  `%CommonProgramFiles%\LV2`, VST3 — в `%CommonProgramFiles%\VST3`.
- Текущий `plugins/core` линкуется в плагины статически, поэтому CLAP/LV2 не
  требуют отдельной общей DLL рядом с приложением.
- VST3 остаётся SDK-gated: без `DONTFLOAT_VST3_SDK_ROOT` секция будет в
  установщике, но артефакт не попадёт в staging.

### Linux / macOS
- CLAP/LV2/VST3 bundle содержит shared library (`.so`/`.dylib`) и manifest.
- Общий DSP-код лучше линковать статически в плагин, чтобы избежать проблем с
  путями загрузчика и версиями библиотек.
- Для macOS понадобится bundle layout и подпись/нотаризация на этапе релиза.

## Ограничения и риски

- Rubber Band и qm-dsp имеют GPL-совместимые лицензии; плагин должен
  распространяться в рамках совместимой лицензии проекта.
- GUI-код (`MainWindow`, `WaveformView`, `PitchGridWidget`) нельзя тащить в
  plugin core. Плагин должен иметь собственный минимальный editor UI или
  использовать generic UI хоста.
- Realtime-аудио не должно выделять память, логировать в горячем пути или
  обращаться к файловой системе из callback.
- Текущий time stretch через Rubber Band используется как offline R3; для
  realtime-плагина сначала нужен отдельный streaming design.

## План интеграции

1. Расширить VST3 target до полноценного bundle по официальному SDK.
2. Проверить CLAP/LV2/VST3 в реальных hosts и validators.
3. Добавить state/preset serialization и automation regression tests.
4. Подготовить installer layout для macOS/Linux packaging.