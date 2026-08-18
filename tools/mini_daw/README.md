# DONTFLOAT mini-DAW hosts

Два вида хостов:

- **`dontfloat_mini_daw`** — GUI-окно по макету `MARKDOWN/example_window_minidaw.svg`:
  одно окно на все девять комбинаций, плагин грузится **в рантайме** (как в
  настоящей DAW). См. раздел «GUI mini-DAW» ниже.
- **`mini_daw_<format>_<kind>`** — headless-хосты, где плагин влинкован на этапе
  сборки; используются как регрессионные тесты (см. «Targets»).

## GUI mini-DAW (`dontfloat_mini_daw`, Windows)

Верхняя панель: кнопка открытия файла, список **формата** (CLAP / VST3 / LV2),
список **редакции** (DONTFLOAT / Scratch / Pitcher), поле **BPM**, **размер
такта**, воспроизведение и стоп. Под ними — дорожка со звуковой волной и
тактовой сеткой: проигранная часть залита красным, каретка идёт по дорожке,
слева/справа время. Ниже — область плагина в красной рамке, куда встраивается
его редактор.

Как это работает:

1. При выборе формата/редакции модуль плагина ищется (`resolvePluginPath`:
   сборочное дерево, затем установленные в Common Files) и грузится
   `LoadLibrary`; редактор встраивается в нативное окно панели
   (CLAP — `clap.gui` win32, LV2 — `lv2ui` с фичей `ui:parent`,
   VST3 — `IPlugView::attached` с `kPlatformTypeHWND`).
2. Открытый трек прогоняется через `process()` плагина — плагин видит аудио
   **с дорожки DAW** (собственного импорта у плагинов больше нет), а транспорт
   играет то, что вернул плагин.
3. Получив аудио, плагин **сам запускает анализ** — Pitcher показывает ноты без
   нажатия «Анализировать», Scratch считает BPM.
4. Пока идёт воспроизведение, хост шлёт плагину позицию каретки — **пустым**
   блоком `process()` (аудио в сессию не добавляется, читается только
   транспорт): CLAP — `clap_event_transport_t`, VST3 — `ProcessContext`.
   Каретка в пианоролле и на волне плагина идёт синхронно с кареткой DAW.
   Для LV2 нужен `time:Position` — не реализовано.

```powershell
# окно с Pitcher на CLAP и загруженным треком
.\dontfloat_mini_daw.exe --format clap --product pitcher --input tests\midi\test_1.wav

# то же, но транспорт стартует сам (удобно для проверки синхронной каретки)
.\dontfloat_mini_daw.exe --format vst3 --product full --autoplay --input tests\midi\test_1.wav

# самопроверка без окна: модуль + редактор + прогон блоков (код возврата 0/2/3)
.\dontfloat_mini_daw.exe --selftest --format lv2 --product full --seconds 2
```

VST3 хостится через Steinberg SDK (`sdk_hosting`): модуль грузится своим
`LoadLibrary` + `GetPluginFactory` (загрузчик SDK отказывал без описания),
классы перебираются сырым `IPluginFactory` (в `VST3::Hosting::PluginFactory
::classInfos()` есть ошибка — `back()` на пустом векторе), компонент и
контроллер соединяются `IConnectionPoint`, аудио идёт через
`IAudioProcessor::process`, темп и размер такта — в `ProcessContext`.

Поля BPM и размера такта задают темп хоста: по ним строится сетка на дорожке
и заполняется транспорт плагина (CLAP/VST3; для LV2 нужен `time:Position`).

### Правка клипов (как в DAW)

Дорожка собрана из клипов: у каждого своя позиция, кусок исходного файла и
коэффициент растяжения. После любой правки дорожка пересобирается и заново
прогоняется через плагин — плагин видит ровно то, что видела бы настоящая DAW.

| Действие | Как |
|----------|-----|
| Разрезать клип по каретке | `S` (когда фокус в пианоролле плагина, `S` режет **ноту** — там своя обработка) |
| Сдвинуть клип | `Ctrl+←` / `Ctrl+→` (секунда) или перетаскивание **правой кнопкой** по дорожке |
| Обрезать начало клипа | `Alt+←` / `Alt+→` (четверть секунды) |
| Обрезать конец клипа | `Shift+←` / `Shift+→` |
| Растянуть / сжать во времени | `Ctrl+↑` / `Ctrl+↓` (±5 %, линейная интерполяция) |

Правки идут по клипу под кареткой — щелчок по дорожке одновременно выбирает
клип. Границы клипов рисуются на дорожке пунктиром.

Что при этом делает плагин: перенос клипа он распознаёт как сдвиг содержимого и
двигает метки растяжения и ноты за ним, а рез, обрезка и растяжение меняют сам
материал — тогда анализ пересчитывается (см. «Захват дорожки по таймлайну» в
`plugins/README.md`).

Окно хоста живёт в оформлении DONTFLOAT (Fusion + тёмная палитра из
`plugins/ui/dontfloat_plugin_theme.h`) — так вид плагина сверяется с главным
окном без поправки на стиль хоста. Загрузка плагина защищена от повторного
входа: редактор внутри может провернуть вложенный цикл событий, и без флага
`reloadingPlugin_` хост успевал удалить `PluginHost` прямо во время
`embedEditor()`.

Выходные WAV headless-хостов пишутся в **`build/temp`** (каталог создаётся сам),
а не в корень репозитория; свой путь по-прежнему задаётся ключом `--output`.

## Headless-хосты

Each host loads an audio file (default `tests/midi/test_1.wav`),
instantiates the DONTFLOAT plugin for its product, streams the whole file through
the plugin, writes the processed output to a WAV, and exercises the shared plugin
core session (prepare + analyze). These are headless command-line hosts.

## Targets

One executable per format × product (built when the matching format is enabled):

| Format | Full | Scratch | Pitcher |
|--------|------|---------|---------|
| CLAP | `mini_daw_clap_full` | `mini_daw_clap_scratch` | `mini_daw_clap_pitcher` |
| LV2  | `mini_daw_lv2_full`  | `mini_daw_lv2_scratch`  | `mini_daw_lv2_pitcher`  |
| VST3 | `mini_daw_vst3_full` | `mini_daw_vst3_scratch` | `mini_daw_vst3_pitcher` |

- **CLAP** hosts the real plugin via `clap_entry` → factory → `process()`.
- **LV2** hosts the real plugin via `lv2_descriptor` → `connect_port`/`run()`.
- **VST3** streams through the shared plugin **core session** (the same path the
  VST3 wrapper feeds). A full realtime VST3 module requires the proprietary
  Steinberg SDK (`DONTFLOAT_VST3_SDK_ROOT`); without it the DONTFLOAT VST3
  binary is not built, so the VST3 mini-DAW uses the core engine.

Disable with `-DDONTFLOAT_BUILD_MINI_DAW=OFF`.

## Usage

```bash
# load test_1.wav, stream through the plugin, print a summary (no display needed)
QT_QPA_PLATFORM=offscreen ./mini_daw_clap_full --seconds 4 --no-output

# write the processed output to a WAV
QT_QPA_PLATFORM=offscreen ./mini_daw_lv2_scratch \
    --input tests/midi/test_1.wav --output out.wav

# CLAP: open the plugin editor embedded in a host window (needs a display),
# just like a DAW — the Pitcher shows the piano roll and an interactive Analyze
QT_QPA_PLATFORM=xcb ./mini_daw_clap_pitcher --gui --seconds 5
```

The `--gui` flag (CLAP hosts only) acts as a tiny DAW: it creates a host window,
loads the plugin, feeds it `test_1.wav`, then embeds the plugin editor via the
CLAP `clap.gui` **X11** extension and runs it — proving the plugin UI shows and
is interactive (Melodyne-style) inside a host.

### Options

| Flag | Default | Meaning |
|------|---------|---------|
| `--input, -i <path>` | `tests/midi/test_1.wav` | audio file to load |
| `--output, -o <path>` | derived | processed output WAV path |
| `--block <n>` | `512` | host processing block size (frames) |
| `--seconds <s>` | `8` | cap processed length (`--full` = whole file) |
| `--no-output` | write | skip writing the output WAV |
| `--gui` / `--headless` | headless | CLAP only: embed & show the plugin editor |

> Note: these hosts need a Qt platform plugin; run headless with
> `QT_QPA_PLATFORM=offscreen`.

## Tests

Each CLAP/LV2/VST3 × product mini-DAW is registered as a CTest that runs headless
on `tests/midi/test_1.wav` (labels `plugins;mini-daw;<format>;<kind>`).
