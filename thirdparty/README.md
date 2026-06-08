# Сторонние библиотеки и компоненты

В данной папке содержатся исходные коды сторонних программ и библиотек, используемых в проекте DONTFLOAT.

## Обзор

| Библиотека | Лицензия | Назначение |
|---|---|---|
| **qm-dsp** (standalone) | GPLv2 | BPM-анализ, детектирование битов, тональность — **собирается** |
| **Mixxx** | GPLv2 | Референс алгоритмов beat tracking (полное дерево, опционально) |
| **Aubio** | GPLv3 | Onset detection, pitch, ритмический анализ |
| **Audacity** | GPLv2/v3 | Референс обработки и визуализации аудио |
| **Essentia** | AGPL v3 | Комплексный аудиоанализ, тональность |
| **Rubber Band** | GPL-2.0-or-later | Тонкомпенсация (time stretch) — **собирается** (`single/RubberBandSingle.cpp`) |
| **SoundTouch** | LGPL | Изменение темпа и тона (не используется в сборке) |
| **Sonic Visualiser** | GPLv2 | Визуализация, Vamp-плагины |
| **LMMS** | GPLv2 | FFT-алгоритмы, DSP-эффекты, анализ спектра |

> **Сборка**: единственный сторонний код, который реально компилируется в DONTFLOAT, — это `thirdparty/qm-dsp` (отдельная копия qm-dsp) и три C-файла `thirdparty/lmms/plugins/ReverbSC`. Остальные каталоги (`mixxx`, `aubio`, `audacity`, `essentia`, `soundtouch`, `sonic-visualiser`) хранятся как референс и для будущей интеграции, в текущей сборке не участвуют.

---

## Mixxx / QM-DSP

### Mixxx
- **Описание**: Полнофункциональное DJ-приложение с открытым исходным кодом
- **GitHub**: https://github.com/mixxxdj/mixxx
- **Версия C++**: C++20 (Visual Studio 2022+)
- **Лицензия**: GPLv2
- **Назначение в DONTFLOAT**: Основа BPM-анализа, детектирование битов
- **Что используется**:
  - Алгоритм детектирования битов (beat tracking)
  - Определение BPM с уверенностью (confidence)
  - Вычисление отклонений долей от сетки (deviation)
  - Генерация меток `BeatInfo` для визуализации

### QM-DSP (Queen Mary Digital Signal Processing)
- **Описание**: Библиотека для цифровой обработки сигналов и музыкальной информатики
- **Разработчик**: Centre for Digital Music, Queen Mary University of London
- **Лицензия**: GPLv2
- **Расположение**: `thirdparty/qm-dsp` — **отдельная** копия библиотеки. Ранее
  бралась из дерева Mixxx (`thirdparty/mixxx/lib/qm-dsp`); теперь сборка
  (`DONTFLOAT.pro` и `CMakeLists.txt`, переменная `QM_DSP_ROOT`) ссылается
  напрямую на `thirdparty/qm-dsp`, поэтому полное дерево Mixxx для сборки
  **не требуется** и может быть удалено.
- **Назначение в DONTFLOAT**: Анализ тональности, ритмический анализ
- **Что делает**:
  - Анализ хроматограмм (хроматический вектор)
  - Определение тональности (KeyAnalyzer)
  - Onset detection (обнаружение начала звуков)
  - Фазовый вокодер
  - Вейвлет-преобразования
  - Анализ MFCC

---

## Aubio
- **Описание**: Библиотека для извлечения музыкальных характеристик из аудиосигналов
- **GitHub**: https://github.com/aubio/aubio
- **Лицензия**: GPLv3
- **Язык**: C/C++
- **Назначение**: BPM, onset detection, анализ тона
- **Что делает**:
  - Алгоритмы определения темпа (BPM)
  - Обнаружение начала нот (onset detection)
  - Анализ высоты тона (pitch detection)
  - Спектральные характеристики
- **Сборка**:
  ```bash
  cd thirdparty/aubio
  ./waf configure
  ./waf build
  ```

---

## Essentia
- **Описание**: Библиотека для анализа аудиосигналов от MTG (Universitat Pompeu Fabra)
- **GitHub**: https://github.com/MTG/essentia
- **Лицензия**: Affero GPL v3
- **Язык**: C++/Python
- **Назначение**: Комплексный анализ аудио, тональность, BPM
- **Что делает**:
  - Определение темпа и ритма
  - Анализ тональности и гармонии
  - Низкоуровневые аудиохарактеристики (MFCC, спектр, огибающая)
  - Классификация жанров
- **Сборка**:
  ```bash
  cd thirdparty/essentia
  ./waf configure
  ./waf build
  ```

---

## Rubber Band Library
- **Описание**: Высококачественный time stretch / pitch shift (Particular Programs Ltd.)
- **GitHub**: https://github.com/breakfastquay/rubberband
- **Лицензия**: GPL-2.0-or-later (совместима с GPLv3 DONTFLOAT)
- **Назначение в DONTFLOAT**: тонкомпенсация при растяжении по меткам (`Ctrl+T`, предпросмотр)
- **Сборка в проекте**: single-file `thirdparty/rubberband/single/RubberBandSingle.cpp` (встроенный FFT/resampler)
- **Получение исходников**:
  ```bash
  git clone --depth 1 --branch v4.0.0 https://github.com/breakfastquay/rubberband.git thirdparty/rubberband
  ```

---

## SoundTouch
- **Описание**: Библиотека для обработки аудиосигналов в реальном времени
- **GitLab**: https://gitlab.com/soundtouch/soundtouch
- **Лицензия**: LGPL (совместима с GPL)
- **Язык**: C++
- **Назначение**: Изменение темпа и высоты звука без деградации качества
- **Что делает**:
  - Изменение темпа без изменения высоты (time stretch)
  - Изменение высоты без изменения темпа (pitch shift)
  - Реальное время, низкая латентность
- **Сборка**:
  ```bash
  cd thirdparty/soundtouch
  cmake .
  cmake --build .
  ```
- **Примечание**: В DONTFLOAT для тонкомпенсации используется **Rubber Band** (GPL); SoundTouch не подключён

---

## Sonic Visualiser
- **Описание**: Приложение для визуализации и анализа аудиосигналов
- **GitHub**: https://github.com/sonic-visualiser/sonic-visualiser
- **Лицензия**: GPLv2
- **Язык**: C++, Qt
- **Назначение**: Референсная реализация для визуализации волн и спектрограмм
- **Что используется**:
  - Идеи визуализации спектрограмм
  - Алгоритмы масштабирования и прокрутки временной оси
  - Поддержка плагинов Vamp для анализа BPM

---

## LMMS
- **Описание**: Linux MultiMedia Studio — полнофункциональная DAW с открытым исходным кодом
- **GitHub**: https://github.com/LMMS/lmms
- **Лицензия**: GPLv2
- **Язык**: C++17, Qt
- **Назначение в DONTFLOAT**: FFT-алгоритмы и DSP-компоненты для спектрограммы
- **Что используется**:

### FFT / Спектральный анализ
| Файл | Что адаптировано |
|------|-----------------|
| `include/fft_helpers.h`, `src/core/fft_helpers.cpp` | Оконные функции (Blackman-Harris, Hamming, Hanning, Rectangular) — адаптированы в `include/fft_engine.h` |
| `plugins/SpectrumAnalyzer/SaProcessor.h/.cpp` | Архитектура per-channel FFT-анализатора — использована как референс |
| `plugins/SpectrumAnalyzer/SaWaterfallView.h/.cpp` | Идея waterfall-спектрограммы с историей |

### Алгоритм Cooley-Tukey FFT
Из исходников LMMS адаптированы оконные функции:
```
fft_helpers.cpp → include/fft_engine.h:
  - precomputeWindow() — Blackman-Harris, Hamming, Hanning, Rectangular
  - Нормировка амплитуды (gain correction)
  - Логарифмическая компрессия бинов (compressbands concept)
```

### Другие DSP-компоненты (доступны для будущей интеграции)
| Компонент | Файлы | Описание |
|---|---|---|
| **Фильтры** | `include/BasicFilters.h` | Header-only: LowPass, HighPass, BandPass, Notch, Moog, SV-фильтры |
| **Гранулярный питч-шифтер** | `plugins/GranularPitchShifter/` | Grain-based с Hermite-интерполяцией, ring buffer |
| **Компрессор** | `plugins/Compressor/` | Lookahead, RMS/peak, auto-makeup, stereo link |
| **Реверберация (ReverbSC)** | `plugins/ReverbSC/revsc.h` | Алгоритм SoundPipe sp_revsc (C, 2 параметра) |
| **Onset detection (SlicerT)** | `plugins/SlicerT/SlicerT.cpp` | FFT spectral flux → автоматическое создание меток |
| **Параметрический EQ** | `plugins/Eq/` | BiQuad с плавной интерполяцией + визуализация кривой |
| **Параметрический EQ** | `include/BasicFilters.h` | Header-only, 15+ типов фильтров |

- **Сборка**: Исходники LMMS используются только как reference/адаптация — отдельная сборка не требуется

---

## Встроенные библиотеки Mixxx

### PortAudio
- **Лицензия**: MIT License
- **Назначение**: Низкоуровневый доступ к аудиоустройствам (встроена в Mixxx)

### HIDAPI
- **GitHub**: https://github.com/libusb/hidapi
- **Лицензия**: BSD/GPL
- **Назначение**: Поддержка USB HID устройств, DJ-контроллеры (встроена в Mixxx)

### SPSCQueue (rigtorp)
- **GitHub**: https://github.com/rigtorp/SPSCQueue
- **Лицензия**: MIT License
- **Назначение**: Lock-free очередь для передачи аудиоданных между потоками (header-only)

### Kaitai Struct
- **GitHub**: https://github.com/kaitai-io/kaitai_struct
- **Лицензия**: MIT License
- **Назначение**: Парсинг бинарных форматов метаданных аудиофайлов

### ReplayGain
- **Лицензия**: GPLv2
- **Назначение**: Нормализация громкости аудио (встроена в Mixxx)

---

## Системные требования

- **C++**: C++17 (проект DONTFLOAT), C++20 (Mixxx)
- **Компилятор**: Visual Studio 2022+ (Windows), GCC 11+ / Clang 12+ (Linux/macOS)
- **CMake**: 3.16+
- **Qt**: 6.8+

---

## Что интегрировано в DONTFLOAT

| Технология | Источник | Файл в проекте |
|---|---|---|
| BPM-анализ | qm-dsp (`thirdparty/qm-dsp`) | `src/bpmanalyzer.cpp` |
| Анализ тональности | qm-dsp (`thirdparty/qm-dsp`) | `src/keyanalyzer.cpp` |
| Time stretch (тонкомпенсация) | Rubber Band Library v4 (GPL, R3 offline) | `src/rubberband_offline.cpp`, `thirdparty/rubberband/single/RubberBandSingle.cpp` |
| FFT оконные функции | LMMS fft_helpers | `include/fft_engine.h` |
| Спектрограмма (Cooley-Tukey FFT) | Адаптация из LMMS | `include/fft_engine.h`, `src/waveformview.cpp` |

---

## Лицензионные замечания

- Все включённые библиотеки совместимы с GPL и между собой
- При коммерческом использовании необходимо проверить лицензионные условия AGPL (Essentia) и LGPL (SoundTouch) отдельно
- Компоненты LMMS используются как **reference/inspiration** с адаптацией (не копированием кода под GPL): `fft_engine.h` — самостоятельная реализация алгоритма с оконными функциями по образцу LMMS `fft_helpers`
- При интеграции новых библиотек убедитесь в совместимости лицензий
