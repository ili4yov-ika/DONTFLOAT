# Статус подключения библиотек Mixxx

**Дата проверки**: 2025-01-27  
**Дата подключения**: 2025-01-27  
**Дата исправления сборки**: 2025-01-27

## Обзор

Проект использует алгоритмы Mixxx для анализа BPM, тональности и аудио-обработки. Библиотеки qm-dsp из Mixxx **ПОДКЛЮЧЕНЫ** к проекту.

## Текущее состояние

### ✅ Библиотеки присутствуют в проекте

- **Путь**: `thirdparty/mixxx/lib/qm-dsp/`
- **Заголовочные файлы найдены**:
  - `dsp/onsets/DetectionFunction.h` ✅
  - `dsp/tempotracking/TempoTrackV2.h` ✅
  - `dsp/keydetection/GetKeyMode.h` ✅
  - `dsp/chromagram/Chromagram.h` ✅
  - `dsp/transforms/FFT.h` ✅
  - `dsp/signalconditioning/Filter.h` ✅

### ✅ Подключение включено (2025-01-27)

#### CMakeLists.txt (строки 127-205)

```cmake
# Third-party: Mixxx qm-dsp
set(QM_DSP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mixxx/lib/qm-dsp)
if(EXISTS ${QM_DSP_ROOT})
    # Разделяем C++ и C файлы
    set(QM_DSP_CPP_SOURCES
        ${QM_DSP_ROOT}/base/Pitch.cpp
        ${QM_DSP_ROOT}/dsp/chromagram/Chromagram.cpp
        ${QM_DSP_ROOT}/dsp/chromagram/ConstantQ.cpp
        ${QM_DSP_ROOT}/dsp/keydetection/GetKeyMode.cpp
        ${QM_DSP_ROOT}/dsp/onsets/DetectionFunction.cpp
        ${QM_DSP_ROOT}/dsp/onsets/PeakPicking.cpp
        ${QM_DSP_ROOT}/dsp/phasevocoder/PhaseVocoder.cpp
        ${QM_DSP_ROOT}/dsp/rateconversion/Decimator.cpp
        ${QM_DSP_ROOT}/dsp/signalconditioning/DFProcess.cpp
        ${QM_DSP_ROOT}/dsp/signalconditioning/FiltFilt.cpp
        ${QM_DSP_ROOT}/dsp/signalconditioning/Filter.cpp
        ${QM_DSP_ROOT}/dsp/signalconditioning/Framer.cpp
        ${QM_DSP_ROOT}/dsp/tempotracking/DownBeat.cpp
        ${QM_DSP_ROOT}/dsp/tempotracking/TempoTrack.cpp
        ${QM_DSP_ROOT}/dsp/tempotracking/TempoTrackV2.cpp
        ${QM_DSP_ROOT}/dsp/tonal/ChangeDetectionFunction.cpp
        ${QM_DSP_ROOT}/dsp/tonal/TCSgram.cpp
        ${QM_DSP_ROOT}/dsp/tonal/TonalEstimator.cpp
        ${QM_DSP_ROOT}/dsp/transforms/FFT.cpp
        ${QM_DSP_ROOT}/maths/Correlation.cpp
        ${QM_DSP_ROOT}/maths/KLDivergence.cpp
        ${QM_DSP_ROOT}/maths/MathUtilities.cpp
    )
    
    # C файлы для kiss_fft (нужно компилировать как C)
    set(QM_DSP_C_SOURCES
        ${QM_DSP_ROOT}/ext/kissfft/kiss_fft.c
        ${QM_DSP_ROOT}/ext/kissfft/tools/kiss_fftr.c
    )
    
    # Создаем библиотеку с C++ и C файлами
    add_library(qm_dsp STATIC 
        ${QM_DSP_CPP_SOURCES}
        ${QM_DSP_C_SOURCES}
    )
    
    # Устанавливаем язык для C файлов
    set_source_files_properties(${QM_DSP_C_SOURCES} PROPERTIES
        LANGUAGE C
    )
    
    # Определения компилятора (как в Mixxx)
    target_compile_definitions(qm_dsp PRIVATE kiss_fft_scalar=double)
    if(MSVC)
        target_compile_definitions(qm_dsp PRIVATE _USE_MATH_DEFINES)
        target_compile_options(qm_dsp PRIVATE
            /wd4244  # Преобразование типов
            /wd4267  # Преобразование size_t
            /wd4828  # Проблемы с кодировкой
        )
        # Для C файлов тоже подавляем предупреждения
        set_source_files_properties(${QM_DSP_C_SOURCES} PROPERTIES
            COMPILE_FLAGS "/wd4244 /wd4267 /wd4828"
        )
    elseif(UNIX)
        target_compile_definitions(qm_dsp PRIVATE USE_PTHREADS)
    endif()
    
    target_include_directories(qm_dsp SYSTEM PUBLIC 
        ${QM_DSP_ROOT} 
        ${QM_DSP_ROOT}/include
    )
    
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_MIXXX_QM_DSP)
    target_link_libraries(${PROJECT_NAME} PRIVATE qm_dsp)
endif()
```

**Исправления сборки (2025-01-27)**:
- ✅ Добавлен язык C в `project()`: `LANGUAGES C CXX`
- ✅ Разделены C++ и C файлы для правильной компиляции
- ✅ Установлен `LANGUAGE C` для C файлов kiss_fft
- ✅ Добавлено `_USE_MATH_DEFINES` для MSVC (для M_PI и других математических констант)
- ✅ Используются только необходимые файлы (как в Mixxx)
- ✅ Правильные пути включений
- ✅ Исправлена обработка MOC для тестов (AUTOMOC ON, добавлены заголовочные файлы)

#### DONTFLOAT.pro

- **Статус**: ✅ Поддержка добавлена
- **Реализация**: Автоматическое подключение при наличии библиотек
- **Определения**: `USE_MIXXX_QM_DSP`, `kiss_fft_scalar=double`, `_USE_MATH_DEFINES` (MSVC)

### ✅ Использование в коде

#### Макрос USE_MIXXX_QM_DSP

- **Статус**: **ОПРЕДЕЛЁН** (при наличии библиотек)
- **Результат**: Используются библиотеки Mixxx вместо упрощённых алгоритмов

#### Файлы с условной компиляцией

1. **`src/bpmanalyzer.cpp`**:
   - `#ifdef USE_MIXXX_QM_DSP` (строки 9-12, 670-710, 724-801)
   - Использует упрощённые алгоритмы обнаружения onset'ов и битов

2. **`src/keyanalyzer.cpp`**:
   - `#ifdef USE_MIXXX_QM_DSP` (строки 7-10, 43, 98)
   - Использует упрощённый алгоритм определения тональности

3. **`src/waveformanalyzer.cpp`**:
   - `#ifdef USE_MIXXX_QM_DSP` (строки 8-11)
   - Использует упрощённые алгоритмы FFT и фильтрации

## Текущая работа

### ✅ Библиотеки Mixxx подключены

- **BPM анализ**: Использует `DetectionFunction` и `TempoTrackV2` из qm-dsp
- **Алгоритм Mixxx**: Реализован как `analyzeBPMUsingMixxx()`, использует библиотеки Mixxx
- **Fallback**: Если библиотеки не найдены, используются упрощённые алгоритмы

### ✅ Преимущества

- **Точность**: Используются проверенные алгоритмы Mixxx
- **Производительность**: Оптимизированные библиотеки
- **Функциональность**: Полный набор функций из qm-dsp

## Исправленные проблемы подключения

### ✅ Проблемы MSVC решены

1. **Совместимость компилятора**: Добавлено `_USE_MATH_DEFINES` для MSVC
2. **Зависимости**: Все зависимости включены (kiss_fft, математические функции)
3. **Настройки сборки**: Используются те же настройки, что и в Mixxx

### ✅ Выполненные проверки

1. ✅ Использованы те же файлы, что и в Mixxx
2. ✅ Добавлены правильные определения компилятора
3. ✅ Настроены правильные пути включений
4. ✅ Добавлена поддержка как CMake, так и qmake
5. ✅ Исправлена компиляция C файлов kiss_fft (разделение C/C++ файлов)
6. ✅ Исправлена обработка MOC для тестов
7. ✅ Проект успешно собирается через CMake (MSVC 2022)

## Статус

### ✅ Библиотеки подключены

1. ✅ Код раскомментирован в `CMakeLists.txt`
2. ✅ Исправлены проблемы совместимости MSVC
3. ✅ Добавлена поддержка в `DONTFLOAT.pro` для qmake
4. ✅ Исправлена компиляция C файлов kiss_fft
5. ✅ Исправлена обработка MOC для тестов
6. ✅ **Проверено**: Проект успешно собирается через CMake (MSVC 2022)

### Текущая реализация

- **Автоматическое подключение**: Библиотеки подключаются автоматически при наличии
- **Fallback**: Если библиотеки не найдены, используются упрощённые алгоритмы
- **Поддержка**: Работает как с CMake, так и с qmake

## Пути включений в коде

### Правильные пути (текущие)

```cpp
#ifdef USE_MIXXX_QM_DSP
#include <dsp/onsets/DetectionFunction.h>
#include <dsp/tempotracking/TempoTrackV2.h>
#include <dsp/keydetection/GetKeyMode.h>
#include <dsp/chromagram/Chromagram.h>
#include <dsp/transforms/FFT.h>
#include <dsp/signalconditioning/Filter.h>
#endif
```

Эти пути будут работать, если `thirdparty/mixxx/lib/qm-dsp` добавлен в `include_directories`.

## Следующие шаги

1. ✅ **Проверено**: Библиотеки присутствуют в проекте
2. ✅ **Проверено**: Пути включений правильные
3. ⚠️ **Требуется**: Решение о включении библиотек или оставлении упрощённых алгоритмов
4. ⚠️ **Требуется**: Если включать - проверить и исправить проблемы MSVC совместимости
5. ⚠️ **Требуется**: Добавить поддержку в `DONTFLOAT.pro` для qmake сборки

