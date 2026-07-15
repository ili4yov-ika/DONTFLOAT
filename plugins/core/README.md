# DONTFLOAT Plugin Core

`plugins/core` содержит общее ядро для будущих DAW plugin targets. Текущий MVP
уже сфокусирован на `DONTFLOAT Track Tool`: session model, analysis/render API
stubs и plain C++ структуры для будущего анализа аудио дорожки, BPM/beat
grid/key analysis, marker map и выравнивания BPM.

## Назначение

- Дать плагинам единый API анализа, render-time обработки и параметров.
- Отделить DSP от `MainWindow`, `WaveformView`, `PitchGridWidget`,
  `QMediaPlayer`, `QSettings` и других UI/runtime-зависимостей.
- Сохранить совпадение поведения с текущими возможностями приложения:
  time stretch, pitch shift, BPM/key analysis.
- Подготовить сериализуемую модель Track Tool session: аудио, анализ, метки,
  параметры выравнивания и состояние render.

## Что можно переиспользовать

- Time stretch: `TimeStretchProcessor`, `RubberBandOffline`.
- Pitch shift: `GranularEngine` (`include/granularpitchshifter_engine.h`).
- BPM / beat grid: `BPMAnalyzer`.
- Тональность: `KeyAnalyzer`.
- Метки stretch: `MarkerData`, расчёт сегментов в `TimeStretchProcessor`.
- Export/render: `WavWriter`.

Для Track Tool нужны будущие plain-struct API:

- `TrackAnalysisOptions` и `TrackAnalysisResult` для BPM/key/beat grid.
- `TrackMarkerMap` для stretch markers и alignment anchors.
- `TrackAlignmentOptions` для target BPM, preserve pitch и fixed-tempo режимов.
- `TrackRenderRequest` и `TrackRenderResult` для offline/render-time обработки.

## Реализованный MVP API

Core оперирует простыми структурами без Qt:

```cpp
struct TrackAudioInfo {
    int sampleRate;
    int channelCount;
    std::int64_t frameCount;
};

struct TrackAnalysisResult {
    TrackToolStatus status;
    float bpm;
    std::vector<TrackBeat> beats;
    std::vector<float> chroma;
};
```

`TrackToolSession` предоставляет `prepare()`, `setAudioInfo()`, `analyze()` и
`render()`. Сейчас `analyze()` и `render()` — безопасные stubs: они валидируют
состояние и возвращают предсказуемые результаты без запуска BPM/Key/Rubber Band.

Qt-типы (`QVector`, `QString`) допустимы в текущем приложении, но для plugin
core лучше перейти на `std::vector`, `std::string` и plain structs.

Текущие файлы:

- `dontfloat_plugin_core.h`
- `dontfloat_plugin_core.cpp`
- `tests/plugin_core_track_tool_test.cpp`

Текущий CMake target: `dontfloat_plugin_core`.

## Realtime-ограничения

- Не выделять память в audio callback.
- Не обращаться к файловой системе из audio callback.
- Не использовать `qDebug`, `std::cout`, исключения и блокирующие mutex в
  горячем пути.
- Offline-алгоритмы (`BPMAnalyzer`, `KeyAnalyzer`, Rubber Band R3 stretch,
  render/export) выполнять вне realtime callback.
- UI Track Tool должен общаться с core через snapshots/commands, а не напрямую
  выполнять тяжёлую обработку из host audio thread.

## План

1. Добавить `TrackToolSession` и сериализацию состояния анализа/меток.
2. Вынести BPM/key analysis adapters без Qt UI-зависимостей.
3. Добавить offline adapters для time stretch и BPM alignment.
4. Добавить render/export API поверх `WavWriter`.
5. Оставить `PitchShiftProcessor` как отдельный DSP-модуль и regression target.
