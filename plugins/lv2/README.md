# DONTFLOAT LV2

`plugins/lv2` содержит начальную LV2-реализацию plugin wrapper DONTFLOAT.
Текущий target уже называется `DONTFLOAT Track Tool` и реализует минимальный
passthrough wrapper для будущего анализа дорожки, BPM/beat grid/key analysis,
marker map и render/export workflow.

## Назначение

LV2 стоит рассматривать как Linux-first формат для hosts, которые используют
`.ttl` manifests. Для Track Tool особенно важны worker/offline capabilities и
ограничения конкретного host: не каждый LV2 host сможет дать прямой доступ к
clip editing, поэтому безопасная базовая модель — analysis + render/export.

## Текущий MVP и целевой плагин

### DONTFLOAT Track Tool
- Реализован как `dontfloat_track_tool_lv2`.
- Текущий realtime path: stereo passthrough, без тяжёлого анализа.
- Lightweight Qt editor shell вынесен в отдельный LV2 UI binary
  `dontfloat_track_tool_ui`.
- UI показывает waveform, BPM/beat grid, markers и key/chroma analysis.
- Core использует `BPMAnalyzer`, `KeyAnalyzer`, `TimeStretchProcessor`,
  `RubberBandOffline` и `WavWriter`.
- Тяжёлый анализ и stretch не выполняются в realtime callback.

## Текущая структура

```text
plugins/lv2/
├── README.md
├── lv2_minimal.h
├── dontfloat_lv2_track_tool.cpp
├── dontfloat_lv2_track_tool_ui.cpp
├── dontfloat_track_tool.lv2/
│   ├── manifest.ttl
│   └── dontfloat_track_tool.ttl
```

Файлы `.ttl` должны описывать URI плагина, порты аудио, параметры, значения по
умолчанию и путь к shared library.

## Правила реализации

- DSP находится в `plugins/core`; LV2-слой только адаптирует API хоста.
- Port indices должны быть стабильными.
- Realtime callback не выделяет память и не вызывает тяжёлый анализ.
- State/preset формат должен быть совместим между версиями.
- Track Tool editor подключается через LV2 UI extension. Host должен
  поддерживать объявленный UI type (`ui:WindowsUI` на Windows,
  `ui:X11UI`/`ui:CocoaUI` на других платформах).

## Сборка

LV2 target подключён к CMake за флагами:

```cmake
option(DONTFLOAT_BUILD_PLUGINS "Build DAW plugins" OFF)
option(DONTFLOAT_BUILD_LV2 "Build LV2 plugin target when plugins are enabled" ON)
```

Сборка:

```powershell
cmake -S . -B build/plugins -DDONTFLOAT_BUILD_PLUGINS=ON -DDONTFLOAT_BUILD_LV2=ON
cmake --build build/plugins --target dontfloat_track_tool_lv2 dontfloat_track_tool_lv2_ui lv2_track_tool_smoke_test lv2_track_tool_ui_smoke_test
ctest --test-dir build/plugins -R "lv2_track_tool.*smoke_test" --output-on-failure
```

В build-каталоге создаётся bundle:

```text
plugins/lv2/dontfloat_track_tool.lv2/
├── dontfloat_track_tool.dll|so|dylib
├── dontfloat_track_tool_ui.dll|so|dylib
├── manifest.ttl
└── dontfloat_track_tool.ttl
```

Для Linux packaging понадобится установка bundle в стандартный путь:

```text
~/.lv2/
/usr/lib/lv2/
/usr/local/lib/lv2/
```

CMake install rule кладёт bundle в
`${CMAKE_INSTALL_LIBDIR}/lv2/dontfloat_track_tool.lv2`.

Windows NSIS installer тоже умеет установить LV2 bundle: опциональная секция
`DAW plugins / LV2 plugin` копирует его в
`%CommonProgramFiles%\LV2\dontfloat_track_tool.lv2`. Это полезно для Windows
hosts с LV2 support, хотя основной целевой сценарий формата остаётся Linux.

## Проверка

1. Проверить `.ttl` через `lv2_validate`, если доступен.
2. Загрузить bundle в тестовый LV2 host.
3. Сравнить результат обработки с `plugins/core`.
4. Проверить `lv2ui_descriptor` и загрузку UI binary в тестовом host.
5. Проверить сохранение state/preset в DAW.
