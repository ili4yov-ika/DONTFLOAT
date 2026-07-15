# План: рабочий музыкальный питчер (аналог Melodyne)

**Версия плана:** 2026-07-15 (фазы A–F реализованы, см. отметки ниже)  
**Связанные документы:** [ISSUES_AND_PLANS.md](ISSUES_AND_PLANS.md), [TIMESTRETCH_FEATURE.md](TIMESTRETCH_FEATURE.md), [docs/features.md](../docs/features.md)

---

## Цель

Пользователь загружает монофонический (или вокальный) материал, видит **ноты на пианоролле**, правит **высоту** (вертикально) и **длительность** (через метки time stretch на волне), после чего слышит скорректированный результат. Поведение и UX ориентируются на Melodyne, но реализуются на базе существующего стека DONTFLOAT (Qt6, qm-dsp, Rubber Band, собственный пианоролл).

---

## Текущее состояние (MVP-0)

| Компонент | Статус | Где в коде |
|-----------|--------|------------|
| Пианоролл (навигация, сетка, легенда) | ✅ | `PitchGridWidget`, `PianoRollEngine` |
| Синхронизация тактов / каретки с волной | ✅ | `MainWindow::syncPitchGridFromWaveform()`, `syncPitchGridTimelineWidth()` |
| Поля тональности (C Major по умолчанию, модуляция) | ✅ | `ui/mainwindow.ui`, `KeySelectionMenu`, `setKey` / `setKey2` |
| Плашка «Анализировать» после загрузки | ✅ | `pitchGridAnalyzeOverlay`, `onPitchGridAnalyzeClicked()` |
| Анализ **тональности** (фон, qm-dsp) | ✅ | `KeyAnalyzer`, `MainWindow::startPitchAnalysis()` |
| Анализ **нот / f0** и блоки на пианоролле | ✅ | `PitchDetector`, `PitchGridWidget::drawNoteBlocks()` |
| Progress bar на плашке анализа | ✅ | `pitchGridAnalyzeProgress`, `setPitchAnalysisUiRunning()` |
| Редактирование нот (drag по Y, ↑/↓, undo) | ✅ | `PitchGridWidget::notePitchEdited`, `PitchNoteEditCommand` |
| Пересчёт нот при drag меток stretch | ✅ | `MainWindow::refreshPitchGridNotes()`, `warpNotesThroughMarkers()` |
| Рендер с коррекцией высоты по нотам | ✅ (Ctrl+Shift+T) | `PitchCorrection::apply()`, `MainWindow::applyPitchCorrection()` |

---

## Целевой UX (пользовательский сценарий)

### 1. Загрузка и первичный анализ

1. Пользователь открывает аудиофайл (как сейчас: BPM, сетка, волна).
2. Над пианороллом (поля тональности + roll) появляется **полупрозрачная серая плашка** с кнопкой **«Анализировать»**.
3. По нажатию:
   - кнопка скрывается;
   - на том же месте, **чуть шире**, показывается **QProgressBar** (неопределённый или 0–100% по этапам);
   - UI остаётся отзывчивым (анализ в `QtConcurrent`, как у `analyzeKey()`).
4. По завершении:
   - плашка исчезает;
   - на пианоролле рисуются **блоки нот**;
   - поля тональности заполняются результатом (`KeyAnalyzer` + при модуляции — второе поле);
   - легенда подсвечивает in-key / out-of-key (уже есть).

### 2. Редактирование

- **Вертикально:** блок ноты только вверх/вниз (snap к полутонам; опционально snap к gamut выбранной тональности).
- **Горизонтально:** границы ноты **не** редактируются мышью на roll — только через **метки stretch** на волне (сегменты между метками).
- При перемещении меток на волне блоки нот **сжимаются/растягиваются** пропорционально сегменту (как в Melodyne при изменении timing).
- Клик по легенде / roll — seek и каретка (✅ уже работает).
- Каретка на волне и roll — одна вертикаль (✅).

### 3. Прослушивание и экспорт

- Превью: применение pitch-коррекции по карте нот (offline или фоновый пересчёт сегментов).
- Экспорт: WAV с учётом нот и существующего pipeline stretch (`Ctrl+T` / save).

---

## Архитектура данных

### `PitchNote` (новая модель)

```cpp
struct PitchNote {
    qint64 startSample = 0;
    qint64 endSample = 0;
    int midiPitch = 60;           // текущая (отредактированная) высота
    int detectedPitch = 60;       // исходная от анализа
    float confidence = 0.f;
    int segmentIndex = 0;         // индекс между метками stretch (0 = до первой метки)
};
```

- Хранение: `PitchNoteMap` / `QVector<PitchNote>` в `MainWindow` или отдельном `PitchSession` (предпочтительно для undo и сериализации).
- Связь с метками: при изменении `WaveformView::markers` пересчитывать `startSample`/`endSample` нот внутри затронутых сегментов (сохраняя **относительное** положение внутри сегмента).

### `PitchAnalysisJob` (новый pipeline)

Этапы для progress bar:

| Этап | % (ориентир) | Движок |
|------|--------------|--------|
| Подготовка моно-сигнала | 5 | `AudioFileService` / mix down |
| Определение тональности | 15 | `KeyAnalyzer` (уже есть) |
| F0 / ноты (frame-wise) | 50 | **новый** `PitchDetector` (aubio YIN / qm-dsp / autocorr — см. тесты) |
| Сегментация в ноты | 20 | onset + pitch stability + min duration |
| Привязка к меткам stretch | 10 | `WaveformView::markers`, BPM grid |

Референсы в репозитории: `tests/pitch_compensation_file_test.cpp` (autocorr f0), `thirdparty/aubio`, `thirdparty/essentia` (не в билде — только как референс алгоритмов).

### Отрисовка

- `PitchGridWidget`: слой `drawNoteBlocks()` поверх сетки, под легендой и кареткой (или каретка поверх всего — как сейчас).
- Hit-test для drag по Y; курсор `SizeVerCursor` / `ClosedHandCursor`.
- Цвет блока: in-key / out-of-key / selected (согласовать с `legendKeyColor`).

---

## Фазы реализации

### Фаза A — UX анализа (плашка + progress)

- [x] **A1.** Состояния overlay: `Idle` → `Analyzing` → `Done` (кнопка ↔ progress bar).
- [x] **A2.** Заменить кнопку на progress bar (`QProgressBar` в том же `pitchGridAnalyzeOverlay`).
- [x] **A3.** Единый слот `startPitchAnalysis()` вместо только `analyzeKey()`: тональность + ноты одной фоновой задачей.
- [ ] **A4.** Отмена анализа (опционально): кнопка «Отмена» на плашке.
- [x] **A5.** Переводы: «Анализ…», «Анализ нот…», ошибки; `lupdate` + `tools/fix_translations.py`.

**Файлы:** `mainwindow.cpp/h`, `ui` не трогать (overlay кодом).

### Фаза B — Детектор нот (offline)

- [x] **B1.** `include/pitchdetector.h`, `src/pitchdetector.cpp` — API: `detectNotes(mono, sampleRate, options, onProgress) → QVector<PitchNote>`.
- [x] **B2.** Моно downmix (`AudioFileService::toMono` от `getSourceAudioData()`).
- [x] **B3.** Frame-based f0: децимация до ~11 кГц + нормированная автокорреляция с защитой от октавной ошибки.
- [x] **B4.** Post-processing: медианный фильтр (окно 5), сегментация по стабильному полутону, min length 70 ms.
- [ ] **B5.** Юнит-тест на `pitch-test_C140BPM.mp3`: одна нота, диапазон sample ±5%.
- [x] **B6.** Прогресс из worker: колбэк → `std::atomic<int>` → `QTimer` обновляет progress bar.

### Фаза C — Отображение блоков нот

- [x] **C1.** `PitchGridWidget::setNotes()`, `drawNoteBlocks()`.
- [x] **C2.** Координаты X через тот же viewport, что сетка (`PianoRollEngine::Viewport`, `timelineReferenceWidth`).
- [x] **C3.** Блоки рисуются под легендой; **каретка** — поверх всего.
- [x] **C4.** Выделение ноты по клику (белая рамка); отредактированные ноты — оранжевые.

### Фаза D — Редактирование высоты

- [x] **D1.** Drag ноты по Y → `midiPitch`, snap semitone (по рядам пианоролла).
- [x] **D2.** Undo/redo: `PitchNoteEditCommand` в `QUndoStack` (последовательные правки одной ноты объединяются).
- [ ] **D3.** Опция «Quantize to key» (использовать `PianoRollEngine::KeySignature`).
- [x] **D4.** Горячие клавиши: ↑/↓ полутон, Shift+↑/↓ октава (при выделенной ноте), Esc — снять выделение; Ctrl+Shift+T в «Настройках горячих клавиш».

### Фаза E — Связь с метками stretch

- [x] **E1.** Подписка на `WaveformView::markersChanged` и `markerDragFinished` → `refreshPitchGridNotes()`.
- [x] **E2.** Алгоритм warp нот: кусочно-линейное отображение `originalPosition → position` (`warpNotesThroughMarkers`); при Ctrl+T warp запекается в базовые координаты нот.
- [x] **E3.** Добавление/удаление метки покрывается тем же пересчётом warp (переразбиение не требуется: ноты хранятся в исходных координатах).
- [ ] **E4.** Тест: 3 метки, drag средней → длины блоков в сегментах 1 и 2 меняются, pitch не меняется.

### Фаза F — Аудио: pitch correction

- [x] **F1.** `PitchCorrection::apply()`: per-note pitch shift = `midiPitch - detectedPitch` (semitones), длина сегмента не меняется.
- [x] **F2.** Rubber Band R3 stretch (тонкомпенсация) + ресемплинг к исходной длине; кроссфейд на границах сегментов.
- [x] **F3.** Фоновый preview после правки ноты (общий debounce/пайплайн с превью меток) + зацикленное прослушивание удерживаемой ноты (`NotePreviewPlayer`, varispeed).
- [x] **F4.** Команда «Применить коррекцию высоты нот» (Ctrl+Shift+T, меню «Правка», undo через `TimeStretchCommand`).
- [ ] **F5.** Регрессия: `pitch_compensation_file_test` + новый тест с 2 нотами разной высоты.

### Фаза G — Полировка и релиз

- [ ] **G1.** Сохранение/загрузка нот в sidecar JSON рядом с WAV (или в project file — позже).
- [x] **G2.** Документация: `docs/features.md`, `docs/shortcuts.md`, `DOCUMENTATION_CHANGELOG.md`.
- [x] **G3.** Ограничения v1: только моно-мелодии (downmix); полифония — backlog.
- [ ] **G4.** Windows / Linux smoke; MSVC + MinGW presets.

---

## Требования к пианороллу (чеклист)

| # | Требование | Статус |
|---|------------|--------|
| 1 | Вертикальные линии тактов **параллельны** линиям на волне | ✅ |
| 2 | Рабочая ширина таймлайна = ширина волны (каретки совпадают) | ✅ |
| 3 | Блоки нот: только Y + stretch от меток | ✅ |
| 4 | Область под легендой: seek, каретка поверх | ✅ |
| 5 | Вертикальный скролл overlay слева, fade у каретки | ✅ |

---

## Требования к реализации (общие)

1. **Переводы:** все новые строки в `tr()`, `lupdate -no-obsolete`, проверка `lrelease`.
2. **Кроссплатформенность:** без WinAPI-only; анализ и рендер — offline, не в audio callback.
3. **Открытость:** GPL-совместимые зависимости; новые файлы с SPDX/заголовком как в проекте.
4. **Melodyne-like UX:** не копировать UI один-в-один; сохранить стиль DONTFLOAT (тёмная тема, плашки, status bar).
5. **Горячие клавиши:** каждое новое действие — в `KeyboardShortcutsDialog` + `MARKDOWN/SHORTCUTS.md`.
6. **Документация:** обновлять по завершении каждой фазы (см. `DOCUMENTATION_CHANGELOG.md`).

---

## Риски и ограничения v1

| Риск | Митигация |
|------|-----------|
| Полифония / аккорды | v1 — моно; сообщение пользователю при стерео вокале |
| Длинный анализ на больших файлах | Downsample для f0; прогресс; отмена |
| Качество pitch shift | A/B Granular vs Rubber Band; настройки из «Настройки питч-шифтера» |
| Рассинхрон нот и stretch preview | Единый источник truth: `displaySampleCount()` + те же формулы, что `TimeStretchProcessor` |
| Производительность отрисовки | Кеш QRectF блоков; перерисовка только при изменении notes/viewport |

---

## Backlog (после v1)

- Полифонический режим (несколько нот на один момент времени).
- Формант-сохранение (отдельно от pitch shift).
- MIDI export/import нот.
- Интеграция в DAW-плагин `DONTFLOAT Track Tool` (`plugins/core`).
- Vibrato / micro-pitch кривые внутри ноты.

---

## Ближайшие шаги (оставшееся)

1. **Тесты** — B5 (детектор на `pitch-test_C140BPM.mp3`), E4 (warp нот), F5 (коррекция 2 нот).
2. **A4 / D3** — отмена анализа, quantize-to-key.
3. **G1 / G4** — sidecar JSON для нот, кроссплатформенный smoke.

Основной сценарий (анализ → блоки нот → правка высоты → Ctrl+Shift+T → скорректированный звук) реализован.
