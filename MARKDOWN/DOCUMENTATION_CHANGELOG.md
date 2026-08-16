# История изменений документации

## 2026-08-16 (пианоролл: панель кнопок и разрез нот)

- Под пианороллом — панель `PianoRollToolbar` (`include/pianoroll_toolbar.h`, `src/pianoroll_toolbar.cpp`): кнопка «Разделить» + пара «Вдоль сетки» / «Свободный рез»; режим реза сохраняется в `QSettings` (`pianoRollCutSnapToGrid`).
- Оформление панели — макет `MARKDOWN/example_panel_buttons.svg`, увеличенный в 1,5 раза: полоса 34 px, капсула 30 px, ножницы 26 px (подложка — мягкий квадрат 4 px), переключатели 27 px; слева капсула повторяет скругление кнопки, справа — полукруг; нажатое состояние всегда темнее отпущенного. Иконки — `resources/icons/{trimmer,along_the_grid,free_cut}.svg`.
- Грабли Qt при вёрстке панели: `border-radius` больше половины размера виджета игнорируется (кнопка рисуется прямоугольной), а правила с псевдосостояниями у `QToolButton` не наследуют box-свойства — радиус продублирован в каждом состоянии.
- Разрез ноты: клавиша `S` по каретке воспроизведения (перехват у «Стоп» через `QEvent::ShortcutOverride`, пока фокус на пианоролле) и клик по ноте в режиме «Разделить»; режим держится до повторного нажатия кнопки или `Esc`.
- `PitchGridWidget`: `CutMode`, `setSplitModeActive`, подсветка будущего реза под курсором, сигналы `noteSplitRequested` / `noteSplitRejected` / `splitModeChanged`; сам разрез применяет `MainWindow`.
- Undo/redo разреза — `PitchNoteSplitCommand` (`include/pitchnotesplitcommand.h`); координаты реза переводятся из таймлайна в исходное аудио обратным отображением по меткам.
- Новый тест `tests/pianoroll_split_test.cpp` (snap к сетке против свободного реза, `canSplitNoteAt`, undo/redo, клик и `S` в виджете).
- Диалог горячих клавиш: пункт `SplitNote` (по умолчанию `S`).
- Переводы пересобраны (`lupdate -no-obsolete`, `lrelease`): 428 строк, добавлены контексты `PianoRollToolbar` и `PitchDetectorSettingsDialog`.
- Обновлены: `README.md`, `docs/features.md`, `docs/shortcuts.md`, `docs/architecture.md`, `MARKDOWN/ARCHITECTURE.md`, `MARKDOWN/SHORTCUTS.md`, `MARKDOWN/PROJECT_FLOWCHART.md`, `MARKDOWN/DEVELOPMENT_GUIDE.md`, `MARKDOWN/PLAN_CREATE_A_WORKING_PITCHER.md`, `MARKDOWN/ISSUES_AND_PLANS.md`, `MARKDOWN/INIT.MD`, `tests/README.md`.

## 2026-07-31 (Windows plugins / installer)

- LV2 UI: `ui:parent` на Windows — HWND как значение `feature->data` (не `HWND*`); иначе пустое окно в REAPER.
- Qt в DAW: `ensureQtApplication` задаёт `QT_PLUGIN_PATH` / `platforms` рядом с `*.impl.dll` и Win32-таймер для `processEvents` (без `exec()` VST3 зависал в `attached()`).
- VST3: stub+impl бандлы + Qt в `Contents\x86_64-win\`; инсталлятор авто-детектит `C:\SDKs\vst3sdk`.
- `tools/build_windows_installer.bat`: без tests/mini-DAW; Qt deploy обязателен (`Qt6Core` + `qwindows.dll`); CLAP/LV2/VST3 — `required`; при SDK отсутствие VST3 — ошибка.
- Обновлены: `cmake/DeployPluginQt.cmake`, `tools/nsis_installer.nsi`, `tools/repair_installed_plugins.ps1`, `tools/README.md`.

## 2026-07-30 (i18n: English source в коде и .ts)

- UI msgid переведён на английский (`tr()` / `.ui` / плагины UI); автоним «Русский» сохранён.
- Live `translations/en_US.ts` = EN→EN, `ru_RU.ts` = EN→RU; `.qm` пересобраны (`lupdate -no-obsolete`, `lrelease`).
- Скрипты: `migrate_tr_to_english.py`, `install_en_source_translations.py`; helpers `fix_translations.py` / `finalize_translations.py` / `apply_remaining_en.py` — под EN→RU.
- CI-guard: `tools/check_tr_english_source.py` + job `i18n-guard` в GitHub Actions.
- `setEnglishLanguage` / старт: EN работает без `.qm` (строки = source); RU по-прежнему требует `ru_RU.qm`.
- Политика языка обновлена в `MARKDOWN/INIT.MD`, `.cursor/commands/init.md`, `MARKDOWN/DEVELOPMENT_GUIDE.md`, `docs/features.md`, `docs/architecture.md`, `README.md`, `tools/README.md`.
- План: `MARKDOWN/PLAN_I18N_ENGLISH_SOURCE.md` (этапы выполнены).

## 2026-07-30 (i18n: подготовка EN как исходного языка)

- Добавлены каталоги `translations/from_en/` (EN→EN / EN→RU) и скрипт `tools/prepare_en_source_translations.py`.
- План миграции UI с русского msgid на английский: `MARKDOWN/PLAN_I18N_ENGLISH_SOURCE.md`.

## 2026-07-30 (UI питч-сетки, модуляции, MIDI-тесты)

- Питч-сетка **видна по умолчанию** (`pitchGridVisible` default = true); `Ctrl+G` по-прежнему переключает.
- Одинаковые горизонтальные отступы волны и питч-сетки: `UiConstants::kTimelineHorizontalMarginPx` = **4 px** (внутренний layout волны без лишних Qt-margins).
- Над пианороллом — потактовая полоса тональности `KeyModulationStrip` (регионы из `KeyAnalyzer::analyzeKeyPerBar`), вместо пары статичных dual-полей.
- Фикстура `tests/midi/test_1` (+ README) и автотесты `midi_pitch_test`, `midi_beat_deviation_test`.
- Обновлён скрин `docs/main_ui.png`.
- Обновлены: `README.md`, `docs/features.md`, `docs/architecture.md`, `docs/shortcuts.md`, `tests/README.md`, `MARKDOWN/ARCHITECTURE.md`, `MARKDOWN/PROJECT_FLOWCHART.md`, `MARKDOWN/PLAN_CREATE_A_WORKING_PITCHER.md`, `MARKDOWN/DEVELOPMENT_GUIDE.md`, `thirdparty/README.md`.

## 2026-07-15 (питчер: живой звук при правке нот)

- Правка высоты ноты теперь слышна без `Ctrl+Shift+T`: после drag/`↑`/`↓`/undo фоновый рендер (общий с превью меток stretch) применяет коррекцию высоты и переключает воспроизведение (`updatePlaybackAfterMarkerDrag` объединяет time stretch и `PitchCorrection::apply`).
- Добавлено зацикленное прослушивание удерживаемой ноты: пока блок зажат мышью, сегмент играет по кругу через `NotePreviewPlayer` (QAudioSink, varispeed), высота меняется мгновенно при перетаскивании; основное воспроизведение ставится на паузу.
- `Ctrl+Shift+T` теперь «закрепляет» коррекцию (рендер от исходных данных, чтобы не задваивать сдвиг поверх фонового превью).
- Обновлены: `docs/features.md`, `docs/shortcuts.md`.

## 2026-07-15 (питчер: ноты на пианоролле, коррекция высоты)

- Реализованы фазы A–F плана `PLAN_CREATE_A_WORKING_PITCHER.md`:
  - плашка «Анализировать» показывает progress bar; тональность и ноты анализируются одной фоновой задачей (`MainWindow::startPitchAnalysis()`);
  - новый детектор нот `PitchDetector` (`include/pitchdetector.h`, `src/pitchdetector.cpp`): децимация до ~11 кГц, автокорреляция, медианное сглаживание, сегментация по полутонам;
  - блоки нот на пианоролле (`PitchGridWidget::drawNoteBlocks()`): синие — как определены, оранжевые — отредактированные, белая рамка — выделенная;
  - редактирование высоты: drag по вертикали, `↑`/`↓` (Shift — октава), undo/redo через `PitchNoteEditCommand`;
  - warp нот при перетаскивании меток time stretch (`warpNotesThroughMarkers`), запекание координат при `Ctrl+T`;
  - коррекция звука по нотам `Ctrl+Shift+T` (`PitchCorrection::apply()`: Rubber Band R3 + ресемплинг, кроссфейды, undo).
- Обновлены: `docs/features.md`, `docs/shortcuts.md`, `MARKDOWN/PLAN_CREATE_A_WORKING_PITCHER.md`, диалог горячих клавиш.
- Переводы синхронизированы (`lupdate -no-obsolete`, `tools/fix_translations.py`): 324 строки, добавлены строки анализа нот и коррекции высоты.

## 2026-06-27 (пианоролл, тональность, переводы)

- Питч-сетка переведена на собственный `PianoRollEngine` (`include/pianoroll_engine.h`); `GiadaPitchGridEngine` удалён из билда.
- Задокументированы: легенда нот справа (полупрозрачная), подсветка out-of-key, синхронизация сетки с учётом ширины легенды, каретка поверх легенды без snap к тактам.
- Тональность по умолчанию **C Major**; анализ по кнопке **«Анализировать»** на плашке после загрузки трека (`KeyAnalyzer`, фоновый поток).
- Убраны упоминания пиков волны на пианоролле.
- Переводы синхронизированы (`lupdate -no-obsolete`, `tools/fix_translations.py`): 349 строк, включая `MarkerTestGenWindow`, `AudioFileService`, кнопку «Анализировать».
- Обновлены: `README.md`, `docs/features.md`, `docs/architecture.md`, `docs/shortcuts.md`, `thirdparty/README.md`, `MARKDOWN/ARCHITECTURE.md`, `MARKDOWN/README.md`, `MARKDOWN/PROJECT_FLOWCHART.md`, `MARKDOWN/INIT.MD`.

## 2026-06-21 (плагины: CLAP/LV2 UI shell)

- Lightweight Qt editor shell распространён на CLAP и LV2: `clap.gui` extension
  для CLAP и отдельный `dontfloat_track_tool_ui` binary для LV2 bundle.
- Добавлен общий Qt-hosting helper для plugin editor без blocking
  `QApplication::exec()` и без зависимости от `MainWindow`.
- Обновлены `plugins/README.md`, `plugins/clap/README.md`,
  `plugins/lv2/README.md` и LV2 `.ttl` metadata.

## 2026-06-21 (плагины: VST3 UI shell)

- Добавлен первый VST3-first проброс интерфейса: lightweight Qt editor shell
  `DONTFLOAT Track Tool` без прямого встраивания `MainWindow`.
- Зафиксированы ограничения MVP: Windows `HWND` attachment, без blocking
  `QApplication::exec()`, без анализа/render из audio callback.
- Обновлена документация `plugins/README.md` и `plugins/vst3/README.md`.

## 2026-06-21 (плагины: реализация Track Tool MVP)

- `dontfloat_pitch_shift_*` targets заменены на `dontfloat_track_tool_*`.
- Документация обновлена под новые wrapper files, LV2 bundle, smoke tests и
  Windows installer paths: `dontfloat_track_tool.clap`,
  `dontfloat_track_tool.lv2`, `DONTFLOAT Track Tool.vst3`.
- `plugins/core` описывает реализованный `TrackToolSession` и безопасные
  analysis/render stubs вместо granular pitch-shift API.

## 2026-06-21 (плагины: DONTFLOAT Track Tool)

- Зафиксирована новая продуктовая цель VST3/CLAP/LV2: не Melodyne-like pitcher,
  а DAW-плагин `DONTFLOAT Track Tool` с UI DONTFLOAT, анализом аудио дорожки,
  BPM/beat grid/key analysis, markers и выравниванием BPM.
- На момент записи `dontfloat_pitch_shift_*` targets были описаны как
  технический MVP; позднее они заменены на `dontfloat_track_tool_*`.
- Обновлён roadmap `plugins/core`: будущие модули `TrackToolSession`, analysis,
  marker map, BPM alignment и render/export.
- Обновлены: `README.md`, `tools/README.md`, `MARKDOWN/DEVELOPMENT_GUIDE.md`,
  `plugins/README.md`, `plugins/core/README.md`, `plugins/clap/README.md`,
  `plugins/lv2/README.md`, `plugins/vst3/README.md`.

## 2026-06-21 (Windows NSIS: секции DAW-плагинов)

- Задокументирована новая страница компонентов в `tools/nsis_installer.nsi`:
  обязательная установка приложения и опциональная группа `DAW plugins`.
- Описаны пути установки CLAP/LV2/VST3 на Windows:
  `%CommonProgramFiles%\CLAP`, `%CommonProgramFiles%\LV2`,
  `%CommonProgramFiles%\VST3`.
- Зафиксировано поведение `tools/build_windows_installer.bat`: installer build
  включает `DONTFLOAT_BUILD_PLUGINS=ON`, CLAP/LV2 собираются по умолчанию, VST3
  зависит от `DONTFLOAT_VST3_SDK_ROOT`.
- Обновлены: `README.md`, `tools/README.md`, `MARKDOWN/DEVELOPMENT_GUIDE.md`,
  `plugins/README.md`, `plugins/clap/README.md`, `plugins/lv2/README.md`,
  `plugins/vst3/README.md`.

## 2026-06-14 (питч-сетка: актуализация документации)

- Питч-сетка **не заморожена**: работает через `PitchGridWidget` + `GiadaPitchGridEngine`, по умолчанию **скрыта** (`Ctrl+G`, `QSettings` → `pitchGridVisible`).
- Задокументированы: синхронизация с `WaveformView` (zoom, offset, каретка, `displaySampleCount()`), таймлайн на полную ширину под подписями нот, overlay-скроллбар слева, ввод мышью и колёсиком.
- Удалены устаревшие формулировки «заморожена / пункт меню заблокирован».
- Обновлены: `docs/features.md`, `docs/architecture.md`, `docs/shortcuts.md`, `README.md`, `MARKDOWN/ARCHITECTURE.md`, `MARKDOWN/INIT.MD`, `MARKDOWN/README.md`, `MARKDOWN/PROJECT_FLOWCHART.md`, `MARKDOWN/SHORTCUTS.md`, `thirdparty/README.md`.

## 2026-06-13 (удаление реверберации после растяжения)

- Удалён эффект «Реверберация после растяжения»: с переходом тонкомпенсации на
  Rubber Band R3 маскировка артефактов не нужна, эффект только окрашивал звук.
- Удалены: пункт меню «Настройки → Реверберация», настройка `reverbEnabled`,
  `include/reverbsc_engine.h` (из репозитория, `CMakeLists.txt` и `DONTFLOAT.pro`).
- Переводы синхронизированы (`lupdate -no-obsolete`), оба языка полные.

## 2026-05-31 (Windows presets, UI-тест меток)

### Сборка и запуск в VS Code/Cursor
- CMake Presets: `windows-msvc-debug/release`, `windows-mingw-debug/release` (`CMakePresets.json`).
- Post-build `windeployqt` для `DONTFLOAT` и `marker_testgen` (`cmake/WinDeployQt.cmake`).
- По умолчанию в `.vscode/settings.json`: preset `windows-msvc-release`, каталог `build/Desktop_Qt_6_9_3_MSVC2022_64bit-Release`.
- Обновлены: `BUILD_IN_VSCODE.md` (presets, F5 vs Play, структура каталогов сборки).

### `ui_responsiveness_test`
- Документирован сценарий `testMarkerDragWorkflowThreeRandom` (MP3 → доли → 2 случайные метки → 3 drag).
- Переменные окружения: `DONTFLOAT_RUN_UI_TEST`, лимиты `DONTFLOAT_UI_DRAG_*`.
- Обновлены: `tests/README.md`, `tests/source4test/README.md`, `MARKDOWN/TESTING_GUIDE.md`.

## 2026-05-31 (сборка macOS)

### CMake Presets и CI
- Добавлены пресеты `macos-debug` / `macos-release` (`CMakePresets.json`, Ninja, `build/macos/`).
- `cmake/PlatformQt.cmake` — поиск Qt на macOS (Homebrew, `~/Qt/6.x/macos`), deployment target 11.0.
- Скрипты `tools/setup_macos.sh`, `tools/macos_build.sh` (сборка, ctest, macdeployqt).
- CI: job `macos` в `.github/workflows/ci.yml` (macos-14, Qt 6.8.3, offscreen-тесты).
- VS Code: задачи и launch `🍎 (macOS) CMake Debug` в `.vscode/tasks.json`, `launch.json`.
- Обновлены: `BUILD_IN_VSCODE.md`, `README.md`, `tools/README.md`.

## 2026-05-31 (тест тонкомпенсации на pitch-test)

### Интеграционный тест `pitch_compensation_file_test`
- Новый Qt Test на `tests/source4test/pitch-test_C140BPM.mp3` (одна устойчивая нота ~165 Hz).
- Сценарии: сжатие/растяжение всего файла (×0.5, ×1.5, ×2) и по половинам через метки;
  контроль без тонкомпенсации (ожидаемый сдвиг f0 ×2).
- Метод проверки: автокорреляция f0, допуск ±6%.
- В CI пропускается (`QSKIP` при `CI`/`GITHUB_ACTIONS`).
- Обновлены: `tests/README.md`, `tests/source4test/README.md`, `MARKDOWN/TESTING_GUIDE.md`,
  `MARKDOWN/TIMESTRETCH_FEATURE.md`, `README.md`.

## 2026-05-31 (Rubber Band, сборка MSVC, сетка, marker_testgen)

### Тонкомпенсация
- WSOLA заменён на **Rubber Band Library v4** (R3 offline, `RubberBandOffline`).
- Обновлены: `TIMESTRETCH_FEATURE.md`, `ARCHITECTURE.md`, `docs/architecture.md`,
  `docs/features.md`, `docs/shortcuts.md`, `DEVELOPMENT_GUIDE.md`, `PROJECT_FLOWCHART.md`,
  `ISSUES_AND_PLANS.md`, `TROUBLESHOOTING.md`.

### Сборка Windows (MSB8052)
- Документирована ошибка несовместимости `VCToolsVersion=14.50` с toolset v143 (VS 2022 + VS 2025).
- Обновлены `BUILD_IN_VSCODE.md`, `TROUBLESHOOTING.md` (фиксация v143 в CMake и `.vscode/settings.json`).

### Утилиты и тесты
- `tools/README.md` — раздел `marker_testgen`; `tests/source4test/README.md` — workflow сетки (Shift+ЛКМ, Shift+◀/▶).
- `docs/shortcuts.md` — подстройка тактовой сетки.

## 2026-05-31 (отладочный вывод GUI)

- Убраны `std::cout`/`qDebug` при каждом запуске GUI; диагностика старта — категория
  `dontfloat.startup` (включить: `--verbose` / `-v` или `QT_LOGGING_RULES`).
- Обновлены `docs/architecture.md`, `MARKDOWN/PROJECT_FLOWCHART.md`, `MARKDOWN/TESTING_GUIDE.md`,
  `MARKDOWN/TROUBLESHOOTING.md`, `MARKDOWN/ISSUES_AND_PLANS.md`.

## 2026-05-30 (консольный режим)

### Консольный режим
- Консольный режим (`-c -f <файл>`) теперь **реально декодирует** файл через `QAudioDecoder`
  (нативный формат, конвертация UInt8/Int16/Int32/Float, даунмикс в моно) — синтетический
  генератор удалён. По умолчанию используется алгоритм Mixxx (флаг `--simple` — упрощённый).
- На Windows вывод подключается к консоли родителя (`AttachConsole`/`freopen`) или к
  перенаправлению `> файл`. Обновлены `README.md`, `MARKDOWN/CONSOLE_MODE.md`,
  `MARKDOWN/ISSUES_AND_PLANS.md`.

## 2026-05-30

### Сборка и зависимости
- **qm-dsp вынесена в standalone** `thirdparty/qm-dsp` (ранее бралась из
  `thirdparty/mixxx/lib/qm-dsp`). Переменная `QM_DSP_ROOT` в `DONTFLOAT.pro` и
  `CMakeLists.txt` теперь указывает на новый путь; полное дерево Mixxx для
  сборки не требуется. Обновлены `thirdparty/README.md`, `ISSUES_AND_PLANS.md`,
  `PROJECT_FLOWCHART.md`, `.vscode/*`.
- Удалён мёртвый код: `bpmfixdialog.*` (были повреждены — содержали посторонние
  логи) и `markerstretchengine.*` (дубликат уже существующего
  `TimeStretchCommand`). В сборке не участвовали.
- В `DONTFLOAT.pro` добавлены в `HEADERS` используемые header-only движки
  `fft_engine.h`, `reverbsc_engine.h`, `granularpitchshifter_engine.h`
  (согласовано с `CMakeLists.txt`).

### Загрузка аудио
- Файл декодируется в **нативном формате** (без принудительного ресемплинга в
  44.1 kHz/стерео); поддержаны форматы сэмплов UInt8/Int16/Int32/Float.
  Соответственно обновлены разделы «Аудио форматы» в `README.md`,
  `docs/features.md`, `docs/architecture.md`, `ARCHITECTURE.md`,
  `DEVELOPMENT_GUIDE.md`.

### Ссылки
- Исправлены битые ссылки на отсутствующий `MIXXX_LIBRARIES_STATUS.md`
  (в `README.md` и `MARKDOWN/README.md`) — теперь ведут на `thirdparty/README.md`.
