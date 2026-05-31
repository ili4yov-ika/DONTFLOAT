# История изменений документации

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
