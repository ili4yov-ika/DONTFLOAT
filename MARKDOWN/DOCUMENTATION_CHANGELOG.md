# История изменений документации

## 2026-08-18 (тональности референса — потактово)

- **Подпись «Референс:» с панели убрана**: полоса и так стоит прямо под полосой тональностей проекта, лишний текст только занимал место.
- **Тональности референса раскладываются по тактам, как у проекта.** Раньше на панели было одно поле с общей тональностью всего файла — модуляции в нём не было видно вовсе. Теперь это та же `KeyModulationStrip`, что и над пианороллом: одно поле на регион тактов, поле стоит на своём месте таймлайна и едет вместе с зумом и прокруткой.
- Полоса референса отличается видом и поведением (`KeyModulationStrip::setReferenceAppearance`): поля серые, как сами референсные ноты, и не открывают меню выбора тональности — референс правится только новым импортом.
- Новый `MidiImporter::analyzeKeyPerBar`: по каждому такту строится хрома из длительностей звучащих в нём нот, такт без нот держит тональность предыдущего (пауза не модулирует), такты до первой ноты и после последней не разбираются. Итог собирает общий с аудио `KeyAnalyzer::summarizeBarKeys` — тот же `PerBarKeyResult`, что у потактового анализа звука.
- Тесты: `midi_export_test` — разрыв региона на модуляции (до мажор → ре мажор с третьего такта) и удержание тональности через пустой такт; `pianoroll_split_test` — полоса референса показывает регионы по порядку и молчит на клик, тогда как полоса проекта открывает меню. Весь набор: 43 теста, все зелёные.

## 2026-08-18 (импорт референсного MIDI)

- **Референсные ноты из MIDI-файла**: пункт меню **«Файл → Импорт референсного MIDI…»** и кнопка **«Импорт MIDI»** рядом с «Экспорт MIDI» на панели пианоролла (обе ведут в один обработчик, и в главном окне, и в Pitcher).
- После выбора файла модальное окно спрашивает, как положить ноты на таймлайн: **«Оставить как есть»** (в темпе самого файла), **«Подогнать под BPM»** проекта или **«Выровнять и подогнать под BPM»** — вдобавок первая нота встаёт на начало тактовой сетки.
- Ноты рисуются **серым позади своих** (`PitchGridWidget::setReferenceNotes`, отрисовка перед `drawNoteBlocks`): их нельзя выделять, тянуть и резать — это фон для сверки.
- Под панелью полей тональностей появляется **панель тональности референса**; она определяется по самим нотам — взвешенная длительностями хрома идёт в `KeyAnalyzer::detectKeyFromChroma` (метод стал публичным ради этого случая). До импорта панель скрыта.
- Новый модуль `include/midiimporter.h` / `src/midiimporter.cpp`: разбор SMF (running status, темп мета-событием `0x51`, парность note on/off), тики → сэмплы по темпу файла или проекта, понятный текст ошибки вместо падения на не-MIDI файле.
- Тесты: `tests/midi_export_test.cpp` дополнен пятью проверками импорта — три режима тайминга, определение тональности референса и отказ на не-MIDI файле; в `tests/pianoroll_split_test.cpp` добавлены две проверки виджета — референс рисуется и пропадает по `clearReferenceNotes`, клик по нему в режиме реза ничего не режет. Весь набор: 43 теста, все зелёные.
- Обновлены: `docs/features.md`, `docs/shortcuts.md`, `MARKDOWN/SHORTCUTS.md`, `MARKDOWN/ARCHITECTURE.md`, `plugins/README.md`, `tests/README.md`, переводы `en_US` / `ru_RU`.

## 2026-08-17 (метки в плагине, слышимый результат, выходные WAV в build/temp)

- **Метки растяжения теперь ставятся и в плагине**: клавиша `M` по каретке, как в главном окне (`WaveformView::addMarker`, минимальный отрезок 50 мс). Раньше в плагине не было ни одной точки входа для создания меток — отсюда и «не могу создать». Клавиша ловится только при фокусе в редакторе плагина (`Qt::WidgetWithChildrenShortcut`), чтобы не отбирать `M` у DAW.
- **Правки стали слышны в DAW.** `process()` отдавал вход как есть, поэтому коррекция высот и растяжение жили только внутри плагина. Теперь обработанный звук кладётся в сессию (`TrackToolSession::setRenderedOutput`) и отдаётся в выход обёртками CLAP и VST3; захват входа делается **до** подмены — у хостов вход и выход часто один буфер. Плагин сообщает хосту, что звук пересчитан (`notify_output_changed` в расширении `dontfloat.transport/1`), и мини-DAW прогоняет дорожку заново — изменённые ноты слышно.
- **Выходные WAV headless-хостов пишутся в `build/temp`** (каталог создаётся сам), а не в корень репозитория; `--output` по-прежнему задаёт свой путь.
- Тесты: `plugin_content_shift_test` дополнен проверками готового результата (подмена выхода и границы диапазона), добавлен `waveform_marker_test` — метка по каретке, отказ при слишком близкой метке, отсутствие меток без аудио.
- Обновлены: `plugins/README.md`, `tools/mini_daw/README.md`, `tests/README.md`.

## 2026-08-17 (сетка DAW, каретка в обе стороны, правка клипов в мини-DAW)

- Шапка плагина приведена к обновлённому макету `MARKDOWN/example_plugin_dontfloat.svg`: группа `OD` / `<` / `BG` / `>` стоит сразу за названием редакции, «Экспорт MIDI» — прямоугольная кнопка с рамкой у правого края полосы пианоролла (в плагине рядом с ней живёт «Применить коррекцию» — `PianoRollToolbar::addTrailingWidget`).
- **Тактовая сетка DAW передаётся в плагин**: из транспорта читаются темп, размер такта и начало текущего такта (CLAP — `tempo` / `tsig_num` / `bar_start`, VST3 — `tempo` / `timeSigNumerator` / `barPositionMusic`), `setHostBeatGrid` ставит их волне и пианороллу. Мини-DAW заполняет эти поля своей сеткой.
- **Каретка из плагина двигает каретку DAW**: клик по волне или пианороллу просит хост встать туда же. Стандартного канала для этого в CLAP и VST3 нет, поэтому добавлено своё расширение CLAP `dontfloat.transport/1` (`request_seek`) — мини-DAW его отдаёт, прочие хосты возвращают `nullptr`. Обратный поток защищён флагом от зацикливания.
- **Мини-DAW умеет клипы**: дорожка собирается из клипов (позиция, кусок исходника, коэффициент растяжения) и заново прогоняется через плагин после каждой правки. `S` — рез по каретке, `Ctrl+←/→` — сдвиг (или перетаскивание правой кнопкой), `Alt+←/→` и `Shift+←/→` — обрезка краёв, `Ctrl+↑/↓` — растяжение/сжатие. Границы клипов рисуются на дорожке пунктиром. Когда фокус в пианоролле плагина, `S` по-прежнему режет ноту — виджет перехватывает клавишу.
- `DONTFLOAT.pro`: добавлен `midiexporter`, убраны ссылки на удалённый `pitchshiftsettingsdialog` (qmake-сборка на них падала).
- Обновлены: `plugins/README.md`, `tools/mini_daw/README.md`.

## 2026-08-17 (экспорт нот в MIDI)

- Ноты пианоролла выгружаются в стандартный MIDI-файл: пункт меню **«Файл → Экспорт MIDI…»** и кнопка **«Экспорт MIDI»** справа на панели пианоролла (обе ведут в один обработчик, обе неактивны, пока нот нет).
- Новый модуль `include/midiexporter.h` / `src/midiexporter.cpp`: SMF формата 0, 480 тиков на четверть, темп мета-событием; сэмплы переводятся в тики по BPM и частоте дискретизации, нулём файла берётся начало тактовой сетки — доли в DAW совпадают с сеткой DONTFLOAT.
- Кнопка есть и в плагинах (панель пианоролла общая): Pitcher экспортирует свои ноты с BPM и началом сетки из анализа сессии.
- Новый тест `tests/midi_export_test.cpp` — round-trip: файл читается разбором SMF из `tests/midi_smf.h`, сверяются темп, разрешение, высоты, позиции и длительности.
- Планы по плагинам (что доделать по захвату дорожки и переносу клипа) записаны в `MARKDOWN/ISSUES_AND_PLANS.md`, п. 0.-1.

## 2026-08-17 (плагины: захват по таймлайну, авто-анализ, перенос клипа)

- **Кнопки «Экспорт WAV» и «Анализировать» / «BPM analysis» убраны.** Анализ идёт сам при **каждом** изменении содержимого дорожки: когда поток блоков от хоста утихает (400 мс), редактор считает отпечаток содержимого и сравнивает с тем, по которому считался прошлый анализ.
- Захват дорожки стал **адресоваться таймлайном**: `TrackToolSession::writeHostFrames` кладёт блок по позиции DAW (CLAP — `clap_event_transport_t`, VST3 — `ProcessContext::projectTimeSamples`; у LV2 транспорта нет, там по-прежнему запись в конец). Буфер повторяет дорожку: перед клипом лежит тишина.
- Блок, пришедший не следом за предыдущим (перемотка, повтор, перенос клипа), начинает новый проход — старый захват сбрасывается. Раньше повторное воспроизведение просто удлиняло дорожку.
- **Перенос клипа в DAW двигает разметку плагина**: `computeContentFingerprint` + `detectContentShift` отличают «тот же материал на новой позиции» от нового материала; в первом случае метки растяжения, тактовая сетка, точки цикла (Scratch) и ноты (Pitcher) сдвигаются на ту же дельту без перезапуска анализа — правки пользователя сохраняются.
- Мини-DAW: клип на дорожке перетаскивается **правой кнопкой мыши** (проверка переноса), `PluginHost::process` получил позицию блока на таймлайне.
- Новый тест `tests/plugin_content_shift_test.cpp`: запись по позиции таймлайна, распознавание переноса клипа и отличие нового материала.
- Обновлены: `plugins/README.md`, `tools/mini_daw/README.md`, `MARKDOWN/ARCHITECTURE.md`, `tests/README.md`.

## 2026-08-17 (интерфейс плагинов — как в главном окне)

- Плагины перерисованы по макету `MARKDOWN/example_plugin_dontfloat.svg`: шапка с именем редакции и теми же кнопками, что на панели главного окна (`OD` / `<` / `BG` / `>`, транспорт ▶ ■ метроном `A` `B` ↺), волна со скроллбаром, поля тональностей, пианоролл с панелью разреза, статусбар внизу. Синяя градиентная шапка и подписи секций убраны.
- Новый модуль темы `plugins/ui/dontfloat_plugin_theme.*`: Fusion + тёмная палитра из `src/main.cpp`. Свой `QApplication` (плагин поднял Qt сам) красится глобально; если цикл Qt крутит хост — красится только поддерево редактора, интерфейс хоста не трогаем.
- Содержимое редакций теперь общается с оболочкой через `DontfloatEditorContent` (`plugins/ui/dontfloat_editor_content.h`): статус уходит сигналом `statusMessage` в один статусбар, кнопки шапки — в секцию с волной (у Pitcher волны нет, поэтому группы сетки и цикла скрыты).
- В плагине появилась панель разреза `PianoRollToolbar` и работающая клавиша `S`; `resources.qrc` теперь компилируется и в модуль плагина — без него иконки шапки и панели были бы пустыми. `cmake/DeployPluginQt.cmake` дополнительно проверяет `imageformats/qsvg.dll` (SVG-иконки).
- Кнопка `OD` использует общий с главным окном алгоритм: `MarkerUtils::detectOnsetSamples` вынесен из `MainWindow::createOnsetMarkersAuto` в `markerengine`; на него добавлен тест в `markersfile_test`.
- ▶ / ■ в плагине — **локальное прослушивание** захваченной дорожки (включённый цикл `A`—`B` ограничивает его куском), метроном тикает во время прослушивания. Мультимедиа создаётся лениво: его инициализация при построении редактора проворачивала вложенный цикл событий хоста.
- Мини-DAW: окно в оформлении DONTFLOAT и защита `reloadPlugin` от повторного входа — иначе хост удалял `PluginHost` прямо во время `embedEditor()` (падение `0xC0000005` по `feeefeee`).
- Обновлены: `plugins/README.md`, `README.md`, `tools/mini_daw/README.md`.

## 2026-08-17 (клавиша S — только разрез ноты)

- **`S` больше не «Стоп»**: клавиша полностью отдана разрезу ноты по каретке воспроизведения. В `MainWindow` появился глобальный шорткат (`ApplicationShortcut` → слот `splitNoteAtPlaybackCursor`), поэтому рез работает при любом фокусе, а не только когда фокус на пианоролле.
- Виджет по-прежнему перехватывает клавишу через `QEvent::ShortcutOverride`: это делает `S` рабочей внутри плагинов (там `MainWindow` нет) и не даёт разрезать дважды, когда фокус на пианоролле.
- У действия «Стоп» горячей клавиши по умолчанию больше нет (кнопка на панели / пункт меню; свою можно назначить в диалоге, id `Stop`). Сохранённое старое значение `Shortcuts/Stop = S` разово вычищается при старте (флаг `Shortcuts/stopKeyFreedForSplit`), иначе у существующих пользователей клавиша осталась бы «Стопом».
- Если пианоролл скрыт или нот ещё нет, `S` пишет подсказку в статусбар вместо тихого игнорирования.
- Переводы: 424/424 (`lupdate -no-obsolete` + `lrelease`); заодно доперевели строки плагинов, оставшиеся без перевода после работы над мини-DAW.
- Обновлены: `README.md`, `docs/shortcuts.md`, `docs/features.md`, `docs/architecture.md`, `MARKDOWN/SHORTCUTS.md`.

## 2026-08-17 (проверка скриптов сборки пакетов)

Проверены `tools/build_windows_installer.bat`, `build_deb.sh`, `build_rpm.sh`,
`macos_build.sh`, `setup_macos.sh`; найденное исправлено.

- `build_windows_installer.bat`: `%ERRORLEVEL%` внутри блоков `( )` подставлялся до запуска `where` — проверка читала код предыдущей команды (обычно ноль, то есть не проверяла ничего; при ненулевом коде инструмент из `PATH` считался ненайденным). Заменено на `!ERRORLEVEL!`; семантика проверена отдельным тестовым `.bat`, а ветка поиска через `PATH` — прогоном урезанной копии скрипта с подставными `makensis`/`cmake`. Второй `if exist` затирал найденный Qt 6.9.3 путём к 6.8.3 — теперь это fallback. Пути установки (`lib\clap`, `lib\lv2\*.lv2`, `lib\vst3\*.vst3\Contents\x86_64-win`) и имена `*.impl.dll` сверены с `install(...)` в `CMakeLists.txt` / `cmake/PluginProducts.cmake` и с `tools/nsis_installer.nsi` — совпадают.
- `build_rpm.sh`: архив собирался с корневым каталогом репозитория, а `%setup -q` ждёт `dontfloat-<версия>` (добавлен `--transform`); версия читается из `project(... VERSION ...)` и подставляется в копию spec (раньше `0.0.0.1` был зашит в двух местах, а `--define _version` ничего не менял); убрана лишняя локальная сборка — проект собирался дважды (в `build/` и внутри `rpmbuild`).
- `tools/rpm/dontfloat.spec`: `%files` не совпадал с тем, что кладёт `make install` — переводы лежат в `/usr/share/DONTFLOAT/` (не `dontfloat`), иконки `dontfloat.png` не было вовсе, плагины и `README.md` не были перечислены; такой пакет падал на «Installed (but unpackaged) files found». Добавлены `%build`-флаги (CLAP/LV2 — ON, VST3/mini-DAW/plugin_tester — OFF, явный `CMAKE_INSTALL_LIBDIR`), `BuildRequires: gcc-c++, make`, `Requires: qt6-qtsvg` (иконки интерфейса — SVG).
- `build_deb.sh`: убрана лишняя предварительная сборка (`dpkg-buildpackage` собирает сам по `debian/rules`), версия читается из `CMakeLists.txt` и сверяется с `debian/changelog`, `debian/rules` получает бит выполнения. В `debian/rules` добавлены те же флаги конфигурации, в `debian/control` — `libqt6svg6` и `hicolor-icon-theme`.
- `CMakeLists.txt`: на Linux ставится иконка `resources/icons/logo.svg` → `share/icons/hicolor/scalable/apps/dontfloat.svg` — `Icon=dontfloat` из `.desktop` раньше ни на что не указывал.
- `macos_build.sh`: `deploy` создавал `.app` без `Info.plist`, из-за чего `macdeployqt` не находил исполняемый файл бандла — plist теперь генерируется (версия из `CMakeLists.txt`). `setup_macos.sh`: убрана вторая строка `export PATH` с зашитыми `/opt/homebrew` и `/usr/local` (перебивала префикс из `brew --prefix qt@6`), добавлена проверка Qt на `QT_MIN_VERSION`.
- Обновлён `tools/README.md` (состав Linux-пакетов, грабля с `%ERRORLEVEL%`, поведение macOS-скриптов).

> Проверено на Windows: `cmake` конфигурируется с новым правилом установки, `bash -n` для четырёх shell-скриптов, тестовый `.bat` на `%ERRORLEVEL%`. `dpkg-buildpackage` / `rpmbuild` / `macdeployqt` на этой машине не запускались.

## 2026-08-17 (мини-DAW по макету, аудио с дорожки, авто-анализ)

- Новая цель `dontfloat_mini_daw` (`tools/mini_daw/mini_daw_{window,player,plugin_host,gui_main}.*`) — GUI-хост по макету `MARKDOWN/example_window_minidaw.svg`: списки формата и редакции, поля BPM и размера такта, дорожка со звуковой волной и тактовой сеткой (проигранное — красным), плагин в панели с красной рамкой.
- Плагин грузится **в рантайме** (`LoadLibrary` + пути из `plugin_host_probe`), редактор встраивается в нативное окно: CLAP — `clap.gui` win32, LV2 — `lv2ui` с `ui:parent`, VST3 — `IPlugView::attached` (Steinberg SDK, цель `sdk_hosting`; модуль и фабрику берём сами: загрузчик SDK отказывал без описания, а `PluginFactory::classInfos()` при неудачном `getClassInfo` зовёт `back()` на пустом векторе — из-за этого падала редакция Full). Работают все девять комбинаций формат × редакция.
- Поля BPM и размера такта задают темп хоста: сетка на дорожке и транспорт плагина (`clap_event_transport` / `ProcessContext`).
- **Каретки DAW и плагина синхронны**: во время воспроизведения хост шлёт позицию **пустым** блоком `process()` (аудио в сессию не попадает, читается только транспорт), плагин двигает каретку пианоролла/волны через `setHostPlayhead()` на редакторах (`DontfloatPluginEditorShell` → Full / Scratch / Pitch). Позиция уходит в UI очередью Qt со склейкой — `process()` в реальной DAW идёт из аудиопотока. Для LV2 нужен `time:Position` — не реализовано.
- В мини-DAW добавлен ключ `--autoplay` (транспорт стартует сразу после загрузки трека), самопроверка `--selftest` прогоняет каретку по треку.
- VST3-обёртка плагина теперь сообщает своему редактору о приходе аудио (`notifyEditorsHostAudioAppended`): процессор и вьюха — разные объекты VST3, без этого редактор не видел дорожку и не запускал анализ.
- Плагины больше **не грузят аудио сами**: кнопки «Import WAV…» убраны, звук приходит с дорожки DAW, а по приходу аудио запускается **авто-анализ** (Pitcher — ноты и тональность без нажатия «Анализировать», Scratch — BPM); пианоролл сам подстраивает диапазон высот под найденные ноты.
- Исправлены падения при работе плагина внутри Qt-хоста: свой насос событий больше не ставится, если цикл Qt крутит хост (`ensureQtApplication`); результаты фоновых анализов идут мимо `QFutureWatcher::result()` (та же грабля, что в `MainWindow`); модули плагинов не выгружаются из процесса; `QAudioSink` не разрушается изнутри своего `stateChanged`.
- Тесты: `mini_daw_gui_<format>_<kind>` — самопроверка `--selftest` того же рантайм-пути (декод → загрузка модуля → редактор → блоки через `process()`), 6 комбинаций CLAP/LV2.
- Обновлён `tools/mini_daw/README.md`.

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
