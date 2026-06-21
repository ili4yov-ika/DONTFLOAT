# DONTFLOAT Plugin Core

`plugins/core` содержит общее DSP-ядро для plugin targets. Сейчас реализован
MVP для granular pitch shift, который используется CLAP-обёрткой.

## Назначение

- Дать плагинам единый API обработки аудиобуферов и параметров.
- Отделить DSP от `MainWindow`, `WaveformView`, `PitchGridWidget`,
  `QMediaPlayer`, `QSettings` и других UI/runtime-зависимостей.
- Сохранить совпадение поведения с текущими возможностями приложения:
  time stretch, pitch shift, BPM/key analysis.

## Что можно переиспользовать

| Возможность | Текущий код |
|---|---|
| Time stretch | `TimeStretchProcessor`, `RubberBandOffline` |
| Pitch shift | `GranularEngine` (`include/granularpitchshifter_engine.h`) |
| BPM / beat grid | `BPMAnalyzer` |
| Тональность | `KeyAnalyzer` |
| Метки stretch | `MarkerData`, расчёт сегментов в `TimeStretchProcessor` |

## Реализованный API

Core оперирует простыми структурами:

```cpp
struct AudioBufferView {
    const float* const* inputs;
    float* const* outputs;
    int inputChannelCount;
    int outputChannelCount;
    int frameCount;
};

struct PitchShiftParams {
    bool enabled;
    float pitchSemitones;
    float grainHz;
    float shape;
    float jitter;
    float wet;
    bool prefilter;
};
```

Qt-типы (`QVector`, `QString`) допустимы в текущем приложении, но для plugin
core лучше перейти на `std::vector`, `std::string` и plain structs.

Текущие файлы:

- `dontfloat_plugin_core.h`
- `dontfloat_plugin_core.cpp`
- `tests/plugin_core_pitch_shift_test.cpp`

Текущий CMake target: `dontfloat_plugin_core`.

## Realtime-ограничения

- Не выделять память в audio callback.
- Не обращаться к файловой системе из audio callback.
- Не использовать `qDebug`, `std::cout`, исключения и блокирующие mutex в
  горячем пути.
- Offline-алгоритмы (Rubber Band R3 stretch) отделять от realtime pitch shift.

## План

1. Расширить smoke-тесты до сравнения с существующим Qt API.
2. Добавить state serialization для параметров.
3. Добавить offline adapters для будущего time stretch.
4. Подключать core из `plugins/vst3` и `plugins/lv2` после CLAP MVP.
