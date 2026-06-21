# DONTFLOAT VST3

`plugins/vst3` содержит стартовую VST3-обёртку над `plugins/core`.
Полноценная сборка gated официальным VST3 SDK.

## Цель

Собрать `.vst3` bundle, который можно загрузить в DAW на Windows, macOS и,
при поддержке хоста, Linux. Плагин должен использовать общий DSP-код, а не
зависеть от основного GUI-приложения DONTFLOAT.

## Первые кандидаты

### DONTFLOAT Pitch Shift
- Realtime-friendly кандидат.
- Основа: `GranularEngine`.
- Параметры: pitch semitones, grain rate, shape, jitter, wet, prefilter.

### DONTFLOAT Time Stretch
- Скорее offline/render-time режим.
- Основа: `TimeStretchProcessor` + `RubberBandOffline`.
- Требует отдельного дизайна latency, буферизации и поведения при изменении
  `timeRatio`.

## Bundle layout

Ориентировочно:

```text
DONTFLOAT Pitch Shift.vst3/
└── Contents/
    ├── x86_64-win/
    │   └── DONTFLOAT Pitch Shift.vst3
    ├── Resources/
    └── moduleinfo.json
```

Для Windows итоговый бинарник может быть `.vst3` DLL внутри bundle. Для macOS
понадобится стандартный bundle layout, подпись и нотаризация на релизе.

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
cmake --build build/plugins --target dontfloat_pitch_shift_vst3
```

Текущий файл реализации: `dontfloat_vst3_pitch_shift.cpp`.

CMake install rule кладёт VST3 artifact в `${CMAKE_INSTALL_LIBDIR}/vst3`.
Windows installer включает опциональную секцию `DAW plugins / VST3 plugin`,
которая копирует staging-содержимое в `%CommonProgramFiles%\VST3`. Если
`DONTFLOAT_VST3_SDK_ROOT` не задан или SDK не найден, target не создаётся и
VST3 artifact не попадает в staging.

## Ограничения

- Не тянуть `MainWindow`, `.ui` файлы и `QApplication` в плагин.
- Editor UI должен быть отдельным и минимальным; на первом этапе можно
  использовать generic UI хоста.
- Все параметры должны иметь стабильные IDs, чтобы проекты DAW открывались
  после обновлений плагина.
- Проверить лицензирование VST3 SDK вместе с GPL-кодом проекта до релиза.

## Минимальный proof-of-concept

1. Завершить фабрику/контроллер по официальным шаблонам VST3 SDK.
2. Добавить параметры в controller и сохранить стабильные IDs.
3. Проверить bundle в validator из VST3 SDK.
4. Добавить smoke-тест загрузки bundle в тестовом хосте.
