# История изменений документации

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
