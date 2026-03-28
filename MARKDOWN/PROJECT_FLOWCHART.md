# Блок-схема проекта DONTFLOAT

Документ описывает устройство проекта: точка входа, режимы работы, связи между компонентами и потоки данных. Основан на **MARKDOWN/INIT.MD** и кодовой базе.

**Схемы в HTML** (открыть в браузере):
- [PROJECT_SCHEME.html](PROJECT_SCHEME.html) — общая схема (компоненты, потоки).
- [PROJECT_SCHEME_DETAILED.html](PROJECT_SCHEME_DETAILED.html) — сущности окон: кнопки, виджеты, поля ввода, выпадающие списки, меню.

---

## 1. Точка входа и режимы запуска

```mermaid
flowchart TB
    subgraph ENTRY["main.cpp"]
        A[main] --> B{Аргументы: -c / --console?}
        B -->|Да| C[QCoreApplication]
        B -->|Нет| D[QApplication]
        C --> E[runConsoleMode]
        E --> F[loadAudioFile синтетический]
        F --> G[BPMAnalyzer::analyzeBPM]
        G --> H[Вывод в консоль]
        D --> I[Тема Fusion, тёмная палитра]
        I --> J[MainWindow]
        J --> K[guiApp.exec]
    end
```

- **GUI-режим**: создаётся `QApplication`, настраивается тема и палитра, создаётся и показывается `MainWindow`, запускается event loop.
- **Консольный режим** (`-c -f <файл>`): создаётся `QCoreApplication`, загружаются (пока синтетические) данные, вызывается `BPMAnalyzer::analyzeBPM`, результат выводится в консоль.

---

## 2. Структура директорий (из INIT.MD)

```mermaid
flowchart LR
    subgraph SOURCE["Исходный код"]
        include["include/ (.h)"]
        src["src/ (.cpp)"]
        ui["ui/ (.ui)"]
    end
    subgraph SUPPORT["Поддержка"]
        resources["resources/"]
        docs["docs/"]
        MARKDOWN["MARKDOWN/"]
        tests["tests/"]
        translations["translations/"]
        thirdparty["thirdparty/"]
    end
    SOURCE --> SUPPORT
```

- **include/** — заголовки; **src/** — реализация; **ui/** — формы Qt Designer.
- **MARKDOWN/** — техническая документация; **docs/** — пользовательская.
- **tests/** — тесты и тестовые данные; **thirdparty/** — qm-dsp (Mixxx), прочее.

---

## 3. Главное окно и подчинённые компоненты

```mermaid
flowchart TB
    subgraph MAIN["MainWindow"]
        UI[Ui::MainWindow]
        MW[MainWindow логика]
        UI --> MW
    end

    subgraph WIDGETS["Виджеты"]
        WV[WaveformView]
        PG[PitchGridWidget]
        HSB[horizontalScrollBar]
        PVSB[pitchGridVerticalScrollBar]
        MS[QSplitter]
    end

    subgraph PLAYBACK["Воспроизведение"]
        MP[QMediaPlayer]
        AO[QAudioOutput]
        PT[playbackTimer]
    end

    subgraph SETTINGS["Настройки и состояние"]
        QS[QSettings]
        US[QUndoStack]
        MC[MetronomeController]
    end

    MAIN --> WIDGETS
    MAIN --> PLAYBACK
    MAIN --> SETTINGS
    MW --> WV
    MW --> PG
    MW --> MP
    MW --> US
    MW --> MC
```

- **MainWindow** — центр приложения: создаёт и держит все виджеты, плеер, таймер, стек отмены, метроном, читает/пишет QSettings.
- **WaveformView** — основная область: волна, метки A/B, метки растяжения, синхронизация с позицией воспроизведения.
- **PitchGridWidget** — питч-сетка (вкл. по Ctrl+G), синхронизируется с WaveformView по скроллу/зуму.

---

## 4. Загрузка и обработка аудиофайла

```mermaid
flowchart LR
    subgraph OPEN["Открытие файла"]
        A1[openAudioFile / Drag&Drop]
        A1 --> A2[processAudioFile]
        A2 --> A3[LoadFileDialog модальный]
        A3 --> A4[MainWindow::loadAudioFile]
        A4 --> A5[QAudioDecoder → float, нормализация]
        A5 --> A6[waveformView->setAudioData]
        A6 --> A7[Опционально: BPMAnalyzer в диалоге]
        A7 --> A8[mediaPlayer->setSource]
    end
```

- Файл выбирается через меню или перетаскивание → `processAudioFile(path)`.
- Внутри: показ **LoadFileDialog** (анализ/выравнивание долей), загрузка через **loadAudioFile** (QAudioDecoder, float, каналы), передача данных в **WaveformView**, при необходимости анализ BPM в диалоге, установка источника для **QMediaPlayer** (для воспроизведения может использоваться временный WAV после обработки).

---

## 5. WaveformView: данные и зависимости

```mermaid
flowchart TB
    subgraph WV["WaveformView"]
        AD[audioData: QVector QVector float]
        BI[beats: BeatInfo]
        MR[markers: Marker]
        BP[playbackPosition]
        ZO[zoomLevel / horizontalOffset]
    end

    subgraph DEPS["Зависимости WaveformView"]
        BPMA[BPMAnalyzer]
        ME[MarkerEngine]
        TSP[TimeStretchProcessor]
        BV[BeatVisualizer]
        WC[WaveformColors]
        TU[TimeUtils]
    end

    WV --> DEPS
    ME --> MR
    TSP --> MR
    BPMA --> BI
```

- **WaveformView** хранит: многоканальные сэмплы, массив битов (BeatInfo), массив меток (Marker из MarkerEngine), позицию воспроизведения, зум и смещение.
- Использует: **BPMAnalyzer** (биты, BPM), **MarkerEngine** (MarkerData/Marker), **TimeStretchProcessor** (applyTimeStretch по меткам), **BeatVisualizer** (опционально), **WaveformColors**, **TimeUtils**.

---

## 6. Система команд (Command Pattern)

```mermaid
flowchart TB
    subgraph COMMANDS["Команды QUndoCommand"]
        AC[AudioCommand]
        BFC[BeatFixCommand]
        TSC[TimeStretchCommand]
    end

    subgraph STACK["QUndoStack (MainWindow)"]
        US[undo/redo]
    end

    subgraph TARGET["Объект изменений"]
        WV[WaveformView]
    end

    AC --> WV
    BFC --> WV
    TSC --> WV
    AC --> US
    BFC --> US
    TSC --> US
```

- **AudioCommand** — смена аудиоданных в WaveformView (старые/новые данные), отмена/повтор через стек.
- **BeatFixCommand** — подмена данных на выровненные по долям (использует BPMAnalyzer::fixBeats и анализ); создаётся в процессе диалога «Анализ и выравнивание долей», пушится в **undoStack**.
- **TimeStretchCommand** — применение сжатия/растяжения по меткам (Ctrl+T): сохраняет состояние меток и аудио, вызывает **TimeStretchProcessor** через WaveformView, пушится в **undoStack**.

---

## 7. Поток данных: от файла до отображения и отмены

```mermaid
flowchart LR
    F[Файл WAV/MP3/FLAC] --> L[loadAudioFile]
    L --> D[float, каналы]
    D --> WV[WaveformView::setAudioData]
    WV --> R[Отрисовка волны]
    D --> BA[BPMAnalyzer::analyzeBPM]
    BA --> WV
    WV --> BE[beats, gridStart]
    BE --> R
    WV --> MR[markers]
    MR --> TSP[TimeStretchProcessor]
    TSP --> WV
    WV --> US[QUndoStack]
    US --> BFC[BeatFixCommand / TimeStretchCommand]
    BFC --> WV
```

- Файл → загрузка в float → WaveformView (и при необходимости временный WAV для плеера).
- Отдельно: BPM-анализ → биты и gridStart в WaveformView → отрисовка сетки/долей.
- Метки в WaveformView используются TimeStretchProcessor; команды отмены/повтора меняют данные в WaveformView через стек.

---

## 8. Анализ и исправление долей (диалог)

```mermaid
flowchart TB
    subgraph DIALOG["LoadFileDialog (Анализ и выравнивание долей)"]
        D1[Показать диалог]
        D2[Загрузка аудио в фоне]
        D3[BPMAnalyzer::analyzeBPM]
        D4[Показать BPM, биты]
        D5{Пользователь: Применить?}
        D5 -->|Да| D6[BPMAnalyzer::fixBeats]
        D6 --> D7[BeatFixCommand]
        D7 --> D8[undoStack->push]
        D5 -->|Нет| D9[Только setAudioData]
    end
    D1 --> D2 --> D3 --> D4 --> D5
```

- В **processAudioFile** показывается **LoadFileDialog** с опциями анализа.
- После загрузки вызывается **BPMAnalyzer::analyzeBPM**; при согласии пользователя — **fixBeats** и **BeatFixCommand** в **undoStack**, иначе только установка данных в WaveformView.

---

## 9. Временное растяжение по меткам (Ctrl+T)

```mermaid
flowchart TB
    subgraph STRETCH["applyTimeStretch (MainWindow)"]
        S1[Получить метки из WaveformView]
        S2[TimeStretchCommand создаётся]
        S3[undoStack->push command]
    end
    subgraph CMD["TimeStretchCommand"]
        S4[redo: waveformView->applyTimeStretch]
        S5[TimeStretchProcessor::applyMarkerStretch]
        S6[WaveformView::updateOriginalData]
    end
    S1 --> S2 --> S3
    S3 --> S4 --> S5 --> S6
```

- Пользователь выставляет метки в WaveformView, нажимает Ctrl+T (или пункт меню).
- MainWindow создаёт **TimeStretchCommand** с текущим WaveformView и метками, пушит в **undoStack**.
- При redo команда вызывает **WaveformView::applyTimeStretch** → **TimeStretchProcessor** → обновление данных и меток в WaveformView.

---

## 10. Сборка (CMake)

```mermaid
flowchart LR
    subgraph CORE["Ядро приложения"]
        main[main.cpp]
        mainwindow[mainwindow]
        waveformview[waveformview]
        markerengine[markerengine]
        pitchgrid[pitchgridwidget]
    end
    subgraph AUDIO["Аудио и анализ"]
        bpmanalyzer[bpmanalyzer]
        keyanalyzer[keyanalyzer]
        waveformanalyzer[waveformanalyzer]
        timestretchprocessor[timestretchprocessor]
        timestretchcommand[timestretchcommand]
        audiocommand[audiocommand]
        beatfixcommand[beatfixcommand]
    end
    subgraph UI["UI и диалоги"]
        loadfiledialog[loadfiledialog]
        metronomesettings[metronomesettingsdialog]
    end
    subgraph THIRDPARTY["thirdparty"]
        qmdsp[qm-dsp Mixxx]
    end
    CORE --> AUDIO
    CORE --> UI
    bpmanalyzer --> qmdsp
    keyanalyzer --> qmdsp
```

- Исполняемый файл собирает все перечисленные модули; при наличии **thirdparty/mixxx/lib/qm-dsp** подключается библиотека **qm_dsp** и определяется `USE_MIXXX_QM_DSP` для BPM и тональности.

---

## Краткая сводка

| Элемент | Назначение |
|--------|------------|
| **main.cpp** | Точка входа: парсинг аргументов, GUI или консоль, создание MainWindow / runConsoleMode |
| **MainWindow** | Окно, меню, виджеты, плеер, таймер, QSettings, QUndoStack, MetronomeController |
| **WaveformView** | Волна, метки A/B, метки растяжения, биты, зум, вызов TimeStretchProcessor и визуализации |
| **PitchGridWidget** | Питч-сетка (Ctrl+G), синхронизация с WaveformView |
| **MarkerEngine** | MarkerData / Marker — данные и UI меток для растяжения |
| **TimeStretchProcessor** | Алгоритмы сжатия/растяжения по меткам (в т.ч. WSOLA) |
| **BPMAnalyzer** | BPM, биты, отклонения, fixBeats; опционально qm-dsp |
| **QUndoStack** | AudioCommand, BeatFixCommand, TimeStretchCommand — отмена/повтор |
| **LoadFileDialog** | Диалог при открытии файла: анализ и выравнивание долей, создание BeatFixCommand |
| **MetronomeController** | Метроном, синхрон с BPM, настройки громкости |

Документ можно обновлять при изменении архитектуры (синхронизация с **MARKDOWN/ARCHITECTURE.md** и **docs/** по правилам из **INIT.MD**).
