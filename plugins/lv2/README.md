# DONTFLOAT LV2

`plugins/lv2` содержит начальную LV2-реализацию pitch shift плагина DONTFLOAT.

## Назначение

LV2 стоит рассматривать как Linux-first формат после появления стабильного
`plugins/core`. Основной сценарий — загрузка в Linux DAW и plugin hosts,
которые используют `.ttl` manifests.

## Возможные плагины

### DONTFLOAT Pitch Shift
- Реализован как `dontfloat_pitch_shift_lv2`.
- Основа: `plugins/core` + `GranularEngine`.
- Realtime-параметры: enabled, pitch, grain rate, shape, jitter, wet, prefilter.

### DONTFLOAT Analyzer
- Возможен как utility-плагин, если host workflow позволяет отображать
  результаты анализа BPM/тональности.
- Основа: `BPMAnalyzer`, `KeyAnalyzer`.
- Не должен выполнять тяжёлый анализ в audio callback.

## Текущая структура

```text
plugins/lv2/
├── README.md
├── lv2_minimal.h
├── dontfloat_lv2_pitch_shift.cpp
├── dontfloat_pitch_shift.lv2/
│   ├── manifest.ttl
│   └── dontfloat_pitch_shift.ttl
```

Файлы `.ttl` должны описывать URI плагина, порты аудио, параметры, значения по
умолчанию и путь к shared library.

## Правила реализации

- DSP находится в `plugins/core`; LV2-слой только адаптирует API хоста.
- Port indices должны быть стабильными.
- Realtime callback не выделяет память и не вызывает тяжёлый анализ.
- State/preset формат должен быть совместим между версиями.
- GUI лучше отложить: начать с generic UI хоста.

## Сборка

LV2 target подключён к CMake за флагами:

```cmake
option(DONTFLOAT_BUILD_PLUGINS "Build DAW plugins" OFF)
option(DONTFLOAT_BUILD_LV2 "Build LV2 plugin target when plugins are enabled" ON)
```

Сборка:

```powershell
cmake -S . -B build/plugins -DDONTFLOAT_BUILD_PLUGINS=ON -DDONTFLOAT_BUILD_LV2=ON
cmake --build build/plugins --target dontfloat_pitch_shift_lv2 lv2_pitch_shift_smoke_test
ctest --test-dir build/plugins -R lv2_pitch_shift_smoke_test --output-on-failure
```

В build-каталоге создаётся bundle:

```text
plugins/lv2/dontfloat_pitch_shift.lv2/
├── dontfloat_pitch_shift.dll|so|dylib
├── manifest.ttl
└── dontfloat_pitch_shift.ttl
```

Для Linux packaging понадобится установка bundle в стандартный путь:

```text
~/.lv2/
/usr/lib/lv2/
/usr/local/lib/lv2/
```

CMake install rule кладёт bundle в
`${CMAKE_INSTALL_LIBDIR}/lv2/dontfloat_pitch_shift.lv2`.

## Проверка

1. Проверить `.ttl` через `lv2_validate`, если доступен.
2. Загрузить bundle в тестовый LV2 host.
3. Сравнить результат обработки с `plugins/core`.
4. Проверить сохранение state/preset в DAW.
