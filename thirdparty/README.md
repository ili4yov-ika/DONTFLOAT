# Сторонние библиотеки и компоненты

В данной папке содержатся исходные коды сторонних программ и библиотек, используемых или хранимых как референс в проекте DONTFLOAT.

## Обзор

| Библиотека | Лицензия | Назначение |
|---|---|---|
| **qm-dsp** (standalone) | GPLv2 | BPM-анализ, детектирование битов, тональность — **собирается** |
| **Rubber Band** | GPL-2.0-or-later | Тонкомпенсация (time stretch) — **собирается** (`single/RubberBandSingle.cpp`) |
| **Mixxx** | GPLv2 | Референс алгоритмов beat tracking (полное дерево, опционально) |
| **LMMS** | GPLv2 | FFT, гранулярный питч-шифт, DSP — референс и адаптация |
| **Giada** | GPLv3 | Loop machine, sample editor — референс UI/воркфлоу |
| **Aubio** | GPLv3 | Onset detection, pitch, ритмический анализ |
| **Audacity** | GPLv2/v3 | Референс обработки и визуализации аудио |
| **Essentia** | AGPL v3 | Комплексный аудиоанализ, тональность |
| **SoundTouch** | LGPL | Изменение темпа и тона (не используется в сборке) |
| **Sonic Visualiser** | GPLv2 | Визуализация, Vamp-плагины |

> **Сборка**: в DONTFLOAT реально компилируются только `thirdparty/qm-dsp` и `thirdparty/rubberband` (см. `CMakeLists.txt`, `cmake/RubberBand.cmake`, `DONTFLOAT.pro`). Остальные каталоги (`mixxx`, `lmms`, `giada`, `aubio`, `audacity`, `essentia`, `soundtouch`, `sonic-visualiser`) хранятся как референс и для будущей интеграции.

---

## Что интегрировано в DONTFLOAT

| Технология | Источник | Файл в проекте | Режим использования |
|---|---|---|---|
| BPM-анализ | qm-dsp | `src/bpmanalyzer.cpp` | offline, при загрузке/анализе |
| Анализ тональности | qm-dsp | `src/keyanalyzer.cpp` | offline |
| Time stretch с тонкомпенсацией | Rubber Band v4 (R3 offline) | `src/rubberband_offline.cpp`, `src/timestretchprocessor.cpp` | `preservePitch=true`: метки, `Ctrl+T`, фоновое превью воспроизведения |
| Быстрое превью волны при drag меток | собственный код (кубическая интерполяция) | `src/timestretchprocessor.cpp` | `preservePitch=false`: только визуализация, без Rubber Band |
| Гранулярный питч-шифт | адаптация LMMS GranularPitchShifter | `include/granularpitchshifter_engine.h` | опционально после `Ctrl+T` |
| Питч-сетка (viewport, beat grid, легенда) | собственный код | `include/pianoroll_engine.h`, `src/pitchgridwidget.cpp` | UI: видна по умолчанию, `Ctrl+G` |
| FFT / оконные функции | адаптация LMMS `fft_helpers` | `include/fft_engine.h` | спектрограмма |
| Спектрограмма (Cooley-Tukey FFT) | адаптация из LMMS | `include/fft_engine.h`, `src/waveformview.cpp` | визуализация |

CMake подключает Rubber Band через `dontfloat_link_rubberband()`; qm-dsp — макрос `USE_MIXXX_QM_DSP` и переменная `QM_DSP_ROOT=thirdparty/qm-dsp`.

---

## Mixxx / QM-DSP

### Mixxx
- **Описание**: Полнофункциональное DJ-приложение с открытым исходным кодом
- **GitHub**: https://github.com/mixxxdj/mixxx
- **Версия C++**: C++20 (Visual Studio 2022+)
- **Лицензия**: GPLv2
- **Назначение в DONTFLOAT**: Референс BPM-анализа, детектирование битов
- **Примечание**: для сборки DONTFLOAT **не требуется** — используется отдельная копия `thirdparty/qm-dsp`

### QM-DSP (Queen Mary Digital Signal Processing)
- **Описание**: Библиотека для цифровой обработки сигналов и музыкальной информатики
- **Разработчик**: Centre for Digital Music, Queen Mary University of London
- **Лицензия**: GPLv2
- **Расположение**: `thirdparty/qm-dsp` — отдельная копия. Ранее бралась из `thirdparty/mixxx/lib/qm-dsp`; сборка (`DONTFLOAT.pro`, `CMakeLists.txt`) ссылается напрямую на `thirdparty/qm-dsp`
- **Назначение в DONTFLOAT**: BPM, beat grid, анализ тональности
- **Что делает**:
  - TempoTrack / TempoTrackV2, onset detection
  - Хроматограммы, KeyAnalyzer
  - Phase vocoder, MFCC, вейвлеты

---

## Rubber Band Library
- **Описание**: Высококачественный time stretch / pitch shift (Particular Programs Ltd.)
- **GitHub**: https://github.com/breakfastquay/rubberband
- **Лицензия**: GPL-2.0-or-later (совместима с GPLv3 DONTFLOAT)
- **Версия в проекте**: v4.0.0 (тег `v4.0.0`)
- **Сборка**: single-file `thirdparty/rubberband/single/RubberBandSingle.cpp`, статическая библиотека `rubberband_single` (`cmake/RubberBand.cmake`)
- **Движок в DONTFLOAT**: R3 offline, `OptionEngineFiner`, `OptionThreadingNever` (`src/rubberband_offline.cpp`)

**Где вызывается:**
- `TimeStretchProcessor::applyMarkerStretch(..., preservePitch=true)` — финальное растяжение по меткам (`Ctrl+T`), фоновый пересчёт для `QMediaPlayer` после drag
- `preservePitch=false` — **не** Rubber Band, а быстрая кубическая интерполяция для превью волны во время перетаскивания меток

**Получение исходников:**
```bash
git clone --depth 1 --branch v4.0.0 https://github.com/breakfastquay/rubberband.git thirdparty/rubberband
```

---

## LMMS
- **Описание**: Linux MultiMedia Studio — полнофункциональная DAW с открытым исходным кодом
- **GitHub**: https://github.com/LMMS/lmms
- **Лицензия**: GPLv2
- **Назначение в DONTFLOAT**: FFT и DSP как референс; часть алгоритмов адаптирована в `include/`

### FFT / спектральный анализ
| Файл LMMS | Что адаптировано |
|---|---|
| `include/fft_helpers.h`, `src/core/fft_helpers.cpp` | Оконные функции → `include/fft_engine.h` |
| `plugins/SpectrumAnalyzer/` | Архитектура per-channel FFT, waterfall |

### Гранулярный питч-шифт (интегрирован)
| Файл LMMS | Адаптация в DONTFLOAT |
|---|---|
| `plugins/GranularPitchShifter/` | `include/granularpitchshifter_engine.h` (`GranularEngine::applyPitchShiftQt`) |

### Другие DSP-компоненты (только референс)
| Компонент | Файлы | Статус в DONTFLOAT |
|---|---|---|
| Фильтры | `include/BasicFilters.h` | не подключён |
| Компрессор | `plugins/Compressor/` | не подключён |
| ReverbSC | `plugins/ReverbSC/revsc.h` | **удалён** из приложения (ранее `reverbsc_engine.h`); в `thirdparty` только как референс |
| Onset (SlicerT) | `plugins/SlicerT/` | не подключён (есть собственный onset в `MainWindow`) |
| EQ | `plugins/Eq/` | не подключён |

---

## Giada
- **Описание**: Минималистичная loop machine для live performance
- **GitHub**: https://github.com/monocasual/giada
- **Сайт**: https://www.giadamusic.com
- **Лицензия**: GPLv3
- **Назначение в DONTFLOAT**: Референс sample editor, waveform tools, loop-воркфлоу
- **Сборка**: не участвует в сборке DONTFLOAT

### Питч-сетка (интегрировано)
| Источник Giada | Адаптация в DONTFLOAT |
|---|---|
| `geWaveform` (viewport) | референс при проектировании; реализация — `PianoRollEngine` |
| Вертикальная сетка / snap | `PianoRollEngine::visibleGridLines`, `snapToGrid` |
| UI piano roll | `src/pitchgridwidget.cpp` (`PitchGridWidget`) |

Полное дерево `thirdparty/giada` — только референс; в билд входит собственный `PianoRollEngine` в корне проекта.

---

## Aubio
- **Описание**: Библиотека для извлечения музыкальных характеристик из аудиосигналов
- **GitHub**: https://github.com/aubio/aubio
- **Лицензия**: GPLv3
- **Назначение**: BPM, onset detection, pitch — референс
- **Сборка** (отдельно, не в DONTFLOAT):
  ```bash
  cd thirdparty/aubio
  ./waf configure
  ./waf build
  ```

---

## Essentia
- **Описание**: Библиотека анализа аудио от MTG (Universitat Pompeu Fabra)
- **GitHub**: https://github.com/MTG/essentia
- **Лицензия**: Affero GPL v3
- **Назначение**: Комплексный анализ, тональность, BPM — референс

---

## SoundTouch
- **Описание**: Time stretch / pitch shift в реальном времени
- **GitLab**: https://gitlab.com/soundtouch/soundtouch
- **Лицензия**: LGPL
- **Примечание**: В DONTFLOAT для тонкомпенсации используется **Rubber Band**; SoundTouch не подключён

---

## Sonic Visualiser
- **Описание**: Визуализация и анализ аудиосигналов
- **GitHub**: https://github.com/sonic-visualiser/sonic-visualiser
- **Лицензия**: GPLv2
- **Назначение**: Референс спектрограмм, масштабирования таймлайна, Vamp-плагинов

---

## Audacity
- **Описание**: Аудиоредактор с открытым исходным кодом
- **Лицензия**: GPLv2/v3
- **Назначение**: Референс обработки и UI-паттернов

---

## Встроенные библиотеки Mixxx (в дереве `thirdparty/mixxx`)

Присутствуют только если каталог Mixxx не удалён; для сборки DONTFLOAT не нужны.

| Компонент | Лицензия | Назначение |
|---|---|---|
| PortAudio | MIT | Низкоуровневый доступ к аудиоустройствам |
| HIDAPI | BSD/GPL | USB HID, DJ-контроллеры |
| SPSCQueue (rigtorp) | MIT | Lock-free очередь между потоками |
| Kaitai Struct | MIT | Парсинг бинарных метаданных |
| ReplayGain | GPLv2 | Нормализация громкости |

---

## Коммерческие альтернативы (не в `thirdparty`)

| Продукт | Поставщик | Назначение | Статус |
|---|---|---|---|
| **élastique Pro** (SDK 3.x) | [zplane.development](https://licensing.zplane.de/technology) | Time stretch / pitch shift, realtime, formant shift для полифонии | **не интегрирован**; лицензия по запросу (royalty) |

В DONTFLOAT вместо élastique используется **Rubber Band** (GPL, open source). élastique применяется в Ableton Live (Complex Pro), DJ.Studio, Pro Tools (élastiqueAAX) и др.

---

## Системные требования

- **C++**: C++17 (DONTFLOAT), C++20 (Mixxx)
- **Компилятор**: MSVC 2022+ / MinGW (Windows), GCC 11+ / Clang 12+ (Linux/macOS)
- **CMake**: 3.16+
- **Qt**: 6.8+ (в проекте: 6.9.3), компоненты Core, Gui, Widgets, Multimedia, Concurrent

---

## Лицензионные замечания

- Собираемые библиотеки (qm-dsp, Rubber Band) совместимы с GPL DONTFLOAT
- При коммерческом использовании отдельно проверьте AGPL (Essentia) и LGPL (SoundTouch)
- Адаптации из LMMS (`fft_engine.h`, `granularpitchshifter_engine.h`) — самостоятельные реализации по образцу LMMS, не прямое копирование плагинов
- **élastique** и другие коммерческие SDK требуют отдельного лицензионного соглашения с zplane
- При добавлении новых библиотек проверяйте совместимость лицензий
