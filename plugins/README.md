# Внешние плагины DONTFLOAT для DAW

Каталог содержит слой DAW-плагинов. По умолчанию plugin targets не собираются
и не влияют на обычный `DONTFLOAT`; включаются через CMake option
`DONTFLOAT_BUILD_PLUGINS`.

## Три вида плагинов

Продуктовая линейка для VST3 / CLAP / LV2 делится на **три отдельных плагина**.
Каждый — самостоятельный модуль в DAW со своим UI и набором функций, но все
три опираются на общий `plugins/core` и переиспользуют виджеты/DSP из основного
приложения.

| Плагин | Назначение | UI / функции | Аналог |
|--------|------------|--------------|--------|
| **DONTFLOAT** | Полная программа внутри DAW | Waveform, beat grid, метки stretch, BPM alignment, тональность, пианоролл, экспорт — тот же рабочий процесс, что в standalone `DONTFLOAT.exe` | Standalone-приложение как plugin editor |
| **DONTFLOAT Scratch** | Выравнивание долей **без** питчера | Waveform, BPM/beat grid, отклонения, stretch markers, time stretch, BPM alignment, render/export. **Без** анализа нот, пианоролла и pitch-коррекции | «Ритмическая» часть DONTFLOAT |
| **DONTFLOAT Pitcher** | Только питчер | Пианоролл, анализ f0/нот, тональность, редактирование высоты нот, pitch-коррекция, превью при drag. **Без** BPM alignment и stretch markers на волне | Melodyne |

```mermaid
flowchart TB
    subgraph core ["plugins/core"]
        Session["TrackToolSession"]
        Analysis["BPM / Key / Beat"]
        Markers["Markers / Stretch"]
        Pitch["PitchDetector / PitchCorrection"]
        Render["Offline Render / Export"]
    end

    subgraph products ["DAW plugins (VST3 / CLAP / LV2)"]
        Full["DONTFLOAT\n(full app)"]
        Scratch["DONTFLOAT Scratch\n(beats only)"]
        Pitcher["DONTFLOAT Pitcher\n(Melodyne-like)"]
    end

    core --> Full
    core --> Scratch
    core --> Pitcher

    Full --> FullUi["Waveform + Grid + Markers + Pitch"]
    Scratch --> ScratchUi["Waveform + Grid + Markers"]
    Pitcher --> PitcherUi["Piano roll + Notes + Keys"]
```

### Разделение ответственности

**DONTFLOAT** — основной plugin target: пользователь получает в DAW тот же
инструмент, что и в desktop-приложении. Подходит, когда нужен полный цикл:
загрузка/захват аудио → анализ BPM и тональности → метки и выравнивание долей
→ при необходимости правка нот и pitch-коррекция → экспорт/render.

**DONTFLOAT Scratch** — облегчённый вариант для задач **только по ритму**:
fixed-tempo / beat grid, визуализация отклонений, drag меток stretch,
time stretch с тонкомпенсацией, выравнивание BPM. Питчер, `PitchGridWidget`,
`PitchDetector` и `PitchCorrection` в этот плагин **не входят**.

**DONTFLOAT Pitcher** — узкоспециализированный питчер, UX близкий к Melodyne:
анализ монофонического материала, блоки нот на пианоролле, вертикальный drag,
undo, live preview при редактировании, offline pitch-коррекция. Waveform с
метками stretch и BPM alignment **не входят** — только pitch-редактор.

### Форматы и имена артефактов (целевые)

Каждый продукт собирается во всех поддерживаемых форматах:

| Продукт | CLAP | LV2 bundle | VST3 |
|---------|------|------------|------|
| DONTFLOAT | `dontfloat.clap` | `dontfloat.lv2/` | `DONTFLOAT.vst3` |
| DONTFLOAT Scratch | `dontfloat_scratch.clap` | `dontfloat_scratch.lv2/` | `DONTFLOAT Scratch.vst3` |
| DONTFLOAT Pitcher | `dontfloat_pitcher.clap` | `dontfloat_pitcher.lv2/` | `DONTFLOAT Pitcher.vst3` |

Установка (Windows NSIS, целевая группа **DAW plugins**):

- CLAP → `%CommonProgramFiles%\CLAP\`
- LV2 → `%CommonProgramFiles%\LV2\`
- VST3 → `%CommonProgramFiles%\VST3\`

### Текущее состояние реализации

Сборка даёт **три отдельных продукта** × три формата (CLAP / LV2 / VST3):

| Продукт | CLAP ID | Editor |
|---------|---------|--------|
| **DONTFLOAT** | `com.dontfloat.full` | Scratch + Pitch (`DontfloatFullEditor`) |
| **DONTFLOAT Scratch** | `com.dontfloat.scratch` | Waveform + BPM (`DontfloatScratchEditor`) |
| **DONTFLOAT Pitcher** | `com.dontfloat.pitcher` | Пианоролл (`DontfloatPitchEditor`) |

Общая инфраструктура: `plugins/core/plugin_product.*`, `DontfloatPluginEditorShell`,
параметризованные `dontfloat_*_plugin_impl.cpp` с `DONTFLOAT_PLUGIN_PRODUCT_INDEX`,
CMake-модуль `cmake/PluginProducts.cmake`, NSIS — три артефакта в каждой секции DAW plugins.

Legacy `dontfloat_track_tool_*` targets удалены из CMake; старые исходники остаются в
репозитории для справки, но больше не собираются.

## Целевой DAW Workflow (DONTFLOAT — полная версия)

1. Пользователь ставит **DONTFLOAT** (или Scratch / Pitcher — по задаче) на
   аудио-дорожку или открывает plugin editor.
2. Плагин получает аудио от host или через offline/render workflow.
3. `plugins/core` анализирует материал (набор анализов зависит от продукта:
   Scratch — BPM/доли/markers; Pitcher — key + f0/ноты; DONTFLOAT — всё).
4. UI показывает интерфейс, соответствующий выбранному плагину (см. таблицу выше).
5. Пользователь подтверждает правки и render/export (BPM stretch и/или pitch).
6. Результат возвращается через render/export или host-specific workflow.

**DONTFLOAT Scratch:** шаги 3–5 только для ритма (без pitch).
**DONTFLOAT Pitcher:** шаги 3–5 только для нот и высоты (без меток BPM stretch).

```mermaid
flowchart LR
    DawHost["DAW Host"] --> PluginWrapper["VST3 CLAP LV2 Wrapper"]
    PluginWrapper --> DontfloatUi["DONTFLOAT Plugin UI"]
    PluginWrapper --> PluginCore["Plugin Core"]
    PluginCore --> Analysis["BPM Key Beat Analysis"]
    PluginCore --> Alignment["Markers BPM Alignment"]
    PluginCore --> Render["Offline Render Export"]
    Analysis --> DontfloatUi
    Alignment --> DontfloatUi
    Render --> DawHost
```

## Форматы

### VST3
- Основной формат для Windows и macOS, также поддерживается многими DAW на Linux.
- Лучше всего подходит для редакторского UI и host integration.
- Целевые артефакты: `DONTFLOAT.vst3`, `DONTFLOAT Scratch.vst3`, `DONTFLOAT Pitcher.vst3`.
- Сейчас: SDK-gated target `dontfloat_track_tool_vst3` (MVP, зачаток Pitcher UI).

### CLAP
- Кроссплатформенный формат с современной event/model архитектурой.
- Подходит для editor/extension workflow, automation и state.
- Целевые артефакты: `dontfloat.clap`, `dontfloat_scratch.clap`, `dontfloat_pitcher.clap`.
- Сейчас: `dontfloat_track_tool_clap` — MVP с `clap.gui` и `DontfloatPitchEditor`.

### LV2
- В первую очередь Linux-экосистема и hosts с `.ttl` manifests.
- Целевые bundle: `dontfloat.lv2/`, `dontfloat_scratch.lv2/`, `dontfloat_pitcher.lv2/`.
- Сейчас: `dontfloat_track_tool_lv2` + `dontfloat_track_tool_lv2_ui` (MVP layout).

## Что уже есть в проекте

Компоненты по продуктам (переиспользуются через `plugins/core` и `plugins/ui`):

| Модуль | DONTFLOAT | Scratch | Pitcher |
|--------|:---------:|:-------:|:-------:|
| `WaveformView`, beat grid | ✅ | ✅ | — |
| `MarkerData`, stretch markers | ✅ | ✅ | — |
| `TimeStretchProcessor`, BPM alignment | ✅ | ✅ | — |
| `KeyAnalyzer` | ✅ | — | ✅ |
| `PitchGridWidget`, `PitchDetector` | ✅ | — | ✅ |
| `PitchCorrection`, note preview | ✅ | — | ✅ |
| `WavWriter`, render/export | ✅ | ✅ | ✅ |

Общие DSP/анализ-классы:

- `TimeStretchProcessor`, `RubberBandOffline` — offline/render-time сжатие и
  растяжение с тонкомпенсацией.
- `BPMAnalyzer` — BPM, beat grid, отклонения и fixed-tempo сценарии.
- `KeyAnalyzer` — основная/вторичная тональность, chroma vector и confidence.
- `MarkerData`, `MarkerEngine` — stretch markers и расчёт сегментов.
- `WavWriter` — экспорт/render результата в WAV.
- `include/granularpitchshifter_engine.h` — granular pitch shift как DSP-модуль,
  а не центральная цель продукта.

Важно: часть текущих классов использует Qt-типы (`QVector`, `QString`) и
виджеты. Для плагинов нужен слой `plugin_core` с `std::vector`, `std::string`,
plain structs и без зависимости от `MainWindow`, `QMediaPlayer` и `QSettings`.

## Предлагаемая структура

```text
plugins/
├── README.md
├── core/                          # общий API для всех трёх продуктов
├── ui/
│   ├── dontfloat_full_editor.*     # DONTFLOAT (полный UI)
│   ├── dontfloat_scratch_editor.*  # DONTFLOAT Scratch
│   ├── dontfloat_pitch_editor.*    # DONTFLOAT Pitcher (уже есть MVP)
│   └── dontfloat_qt_hosting.*
├── vst3/
│   ├── dontfloat_vst3.cpp
│   ├── dontfloat_vst3_scratch.cpp
│   └── dontfloat_vst3_pitcher.cpp
├── clap/
│   ├── dontfloat_clap.cpp
│   ├── dontfloat_clap_scratch.cpp
│   └── dontfloat_clap_pitcher.cpp
└── lv2/
    ├── dontfloat.lv2/
    ├── dontfloat_scratch.lv2/
    └── dontfloat_pitcher.lv2/
```

Текущий MVP (`dontfloat_track_tool_*`) — временная монолитная обёртка до
разделения на три target. Файлы `dontfloat_*_track_tool.*` будут переименованы
или разветвлены по мере реализации продуктовой линейки.

`core/` должен содержать DSP, анализ и сериализуемую модель проекта плагина.
Форматные обёртки (`vst3/`, `clap/`, `lv2/`) должны заниматься только API
хоста: lifecycle, editor binding, audio/offline buffers, automation, state и
форматными ограничениями.

Локальная документация:

- [`core/README.md`](core/README.md) — общий analysis/DSP/render слой без UI.
- [`vst3/README.md`](vst3/README.md) — VST3 wrappers (три продукта).
- [`clap/README.md`](clap/README.md) — CLAP wrappers (три продукта).
- [`lv2/README.md`](lv2/README.md) — LV2 bundles (три продукта).

## Сборка

По умолчанию плагины отключены. Включить:

```powershell
cmake -S . -B build/plugins -DDONTFLOAT_BUILD_PLUGINS=ON -DDONTFLOAT_BUILD_CLAP=ON -DDONTFLOAT_BUILD_LV2=ON
cmake --build build/plugins --target plugin_core_track_tool_test dontfloat_track_tool_clap dontfloat_track_tool_lv2
ctest --test-dir build/plugins -R plugin_core_track_tool_test --output-on-failure
```

Цели **сейчас** (MVP, один модуль `track_tool`):

- `dontfloat_plugin_core` — static library общего DSP API.
- `dontfloat_track_tool_clap` — CLAP module `dontfloat_track_tool.clap`.
- `dontfloat_track_tool_lv2` — LV2 module и bundle metadata.
- `dontfloat_track_tool_vst3` — VST3 target при доступном SDK.
- smoke-тесты: `plugin_core_track_tool_test`, `clap_track_tool_smoke_test`,
  `lv2_track_tool_smoke_test`, `lv2_track_tool_ui_smoke_test`.

Цели **после разделения** (×3 продукта ×3 формата = до 9 installable artifacts):

- `dontfloat_*`, `dontfloat_scratch_*`, `dontfloat_pitcher_*` для CLAP/LV2/VST3.

Дополнительные CMake-переменные:

- `DONTFLOAT_BUILD_PLUGINS=ON/OFF` — включает plugin targets.
- `DONTFLOAT_BUILD_CLAP=ON/OFF` — включает CLAP target.
- `DONTFLOAT_BUILD_LV2=ON/OFF` — включает LV2 target.
- `DONTFLOAT_BUILD_VST3=ON/OFF` — включает VST3 target при наличии SDK.
- `DONTFLOAT_CLAP_SDK_ROOT` — зарезервировано под официальный CLAP SDK.
- `DONTFLOAT_VST3_SDK_ROOT` — зарезервировано под VST3 SDK.

Установка:

- CLAP: `${CMAKE_INSTALL_LIBDIR}/clap`.
- LV2: `${CMAKE_INSTALL_LIBDIR}/lv2/dontfloat_track_tool.lv2`.
- VST3: `${CMAKE_INSTALL_LIBDIR}/vst3`, если доступен `DONTFLOAT_VST3_SDK_ROOT`.

### Windows NSIS

`tools/build_windows_installer.bat` включает `DONTFLOAT_BUILD_PLUGINS=ON`,
`DONTFLOAT_BUILD_CLAP=ON`, `DONTFLOAT_BUILD_LV2=ON` и
`DONTFLOAT_BUILD_VST3=ON`. Если переменная окружения `DONTFLOAT_VST3_SDK_ROOT`
не задана, VST3 target пропускается, а CLAP/LV2 продолжают собираться.

`tools/nsis_installer.nsi` показывает страницу компонентов. Основное приложение
устанавливается обязательно. Группа **DAW plugins** (все секции включены по
умолчанию):

**Сейчас** — один MVP на формат:

- CLAP → `%CommonProgramFiles%\CLAP\dontfloat_track_tool.clap`
- LV2 → `%CommonProgramFiles%\LV2\dontfloat_track_tool.lv2`
- VST3 → `%CommonProgramFiles%\VST3`

**Целевое** — три продукта × три формата (9 опциональных секций или
подгруппы DONTFLOAT / Scratch / Pitcher с CLAP/LV2/VST3 внутри).

## Соответствие продуктов и кода

Подробное описание трёх плагинов — в начале документа (раздел **Три вида
плагинов**). Кратко:

| Продукт | Роль | Ключевые классы UI |
|---------|------|-------------------|
| **DONTFLOAT** | Standalone в DAW | `WaveformView`, markers, `PitchGridWidget`, полный editor |
| **DONTFLOAT Scratch** | Только доли/BPM | `WaveformView`, markers, без pitch-модулей |
| **DONTFLOAT Pitcher** | Melodyne-like | `DontfloatPitchEditor`, `PitchGridWidget`, `PitchDetector` |

Режим для всех трёх: analysis + offline/render-time transform. Realtime audio
callback не выполняет тяжёлый анализ или Rubber Band R3 stretch.

## Как это должно работать

### Windows
- Windows installer собирает plugin targets и предлагает отдельную группу
  `DAW plugins` для CLAP/LV2/VST3.
- CLAP устанавливается в `%CommonProgramFiles%\CLAP`, LV2 — в
  `%CommonProgramFiles%\LV2`, VST3 — в `%CommonProgramFiles%\VST3`.
- Текущий `plugins/core` линкуется в плагины статически, поэтому CLAP/LV2 не
  требуют отдельной общей DLL рядом с приложением.
- VST3 остаётся SDK-gated: без `DONTFLOAT_VST3_SDK_ROOT` артефакт не попадёт в
  staging. После установки SDK target собирается, но пока остаётся стартовым
  wrapper, а не готовым Track Tool.

### Linux / macOS
- CLAP/LV2/VST3 bundle содержит shared library (`.so`/`.dylib`) и manifest.
- Общий DSP-код лучше линковать статически в плагин, чтобы избежать проблем с
  путями загрузчика и версиями библиотек.
- Для macOS понадобится bundle layout и подпись/нотаризация на этапе релиза.

## Ограничения и риски

- Rubber Band и qm-dsp имеют GPL-совместимые лицензии; плагин должен
  распространяться в рамках совместимой лицензии проекта.
- GUI-код (`MainWindow`, `WaveformView`, `PitchGridWidget`) нельзя тащить в
  audio callback. Для каждого из трёх продуктов — отдельный plugin editor,
  переиспользующий виджеты осознанно или через выделенную UI-модель.
- Realtime-аудио не должно выделять память, логировать в горячем пути или
  обращаться к файловой системе из callback.
- Текущий time stretch через Rubber Band используется как offline R3; для
  realtime-плагина сначала нужен отдельный streaming design.
- Нельзя обещать прямое редактирование clip в любой DAW: сначала нужно
  проектировать render/export и host-specific capabilities.

## План интеграции

1. ✅ Модель `TrackToolSession`: audio, analysis, markers, pitch notes, render state.
2. ✅ MVP host wrappers (VST3/CLAP/LV2) и `DontfloatPitchEditor` (зачаток Pitcher).
3. Разделить MVP на три продукта: CMake targets, plugin ID, factory, install rules.
4. **DONTFLOAT Pitcher** — parity с standalone-питчером (`PLAN_CREATE_A_WORKING_PITCHER.md`).
5. **DONTFLOAT Scratch** — waveform + markers editor без pitch UI.
6. **DONTFLOAT** — объединённый full editor (Scratch + Pitcher в одном plugin).
7. NSIS: группа DAW plugins с тремя продуктами (CLAP/LV2/VST3 каждый).
8. Проверка в реальных DAW hosts и format validators.
