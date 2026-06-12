# DONTFLOAT Tests/source4test

Каталог для тестовых аудиофайлов и эталонных меток.

## Утилита разметки

Соберите и запустите **`marker_testgen`** (CMake-цель):

```bash
cmake --build build --config Release --target marker_testgen
# Windows (Visual Studio multi-config): build/Release/Release/marker_testgen.exe
# или build/Release/marker_testgen.exe — в зависимости от генератора
```

В VS Code/Cursor каталог сборки: `build/${buildType}` (см. `.vscode/settings.json`).

### Workflow

1. **Файл → Открыть** — аудио с **постоянным BPM** (анализ как в основной программе).
2. На волне появляются **метки на каждой доле** тактовой сетки.
3. Корректировка:
   - перетаскивание меток;
   - **Shift** при перетаскивании метки — привязка к сетке;
   - **Shift + ЛКМ** на волне (не на метке) — **тонкая подстройка** начала тактовой сетки (метки сдвигаются вместе);
   - **◀ сетка / сетка ▶** — сдвиг сетки на один удар;
   - **Shift + ◀/▶** — сетка и метки сдвигаются вместе;
   - **M** — добавить метку на позиции воспроизведения.
4. **Файл → Сохранить** — выбор пути к аудио + рядом создаётся  
   `ИМЯ-ФАЙЛА_markers.txt`.

### Формат `*_markers.txt`

Заголовок:

```
# BPM=80 BeatsPerBar=4 SampleRate=44100 GridStartSample=1729
```

Строки (CRLF, время `M:SS:CC` — минуты:секунды:сотые):

```
1.1 - 0:00:00
1.2 - 0:00:01
1.3 - 0:00:02
1.4 - 0:00:03
2.1 - 0:00:04
```

При повторном открытии того же аудио, если рядом есть `*_markers.txt`, метки загружаются из файла.

## Именование файлов

- `*_C*BPM.mp3` — постоянный темп (Constant)
- `*_V*BPM.mp3` — переменный темп (Variable), для тестов выравнивания

## Примеры

### example_C80BPM.mp3
- Постоянные ~80 BPM
- Начало первого такта ≈ 00:00.039

### example_V80BPM.mp3
- Переменный темп ~80 BPM
- Начало первого такта ≈ 00:00.741
- Используется в **`ui_responsiveness_test`**: загрузка, поиск долей, метки выравнивания, перетаскивание меток, time stretch, плавность воспроизведения

```powershell
# из корня репозитория
cmake --build build/Desktop_Qt_6_9_3_MinGW_64_bit-Debug --target ui_responsiveness_test
$env:DONTFLOAT_RUN_UI_TEST = "1"
.\build\Desktop_Qt_6_9_3_MinGW_64_bit-Debug\ui_responsiveness_test.exe testMarkerDragWorkflowThreeRandom
```

Подробнее: [tests/README.md](../README.md#ui_responsiveness_test).

### pitch-test_C140BPM.mp3
- ~140 BPM, постоянный темп, **одна устойчивая нота** (~165 Hz, E3)
- Длительность ~8.6 с, 48 kHz stereo
- Назначение: интеграционный тест тонкомпенсации (`pitch_compensation_file_test`)
- Проверяется: `TimeStretchProcessor::applyMarkerStretch()` с Rubber Band — сжатие/растжение
  метками (×0.5 … ×2.0) не смещает f0 больше чем на 6%; без тонкомпенсации тон растёт ~×2

```powershell
cmake --build build/Debug --config Debug --target pitch_compensation_file_test
.\build\Debug\Debug\pitch_compensation_file_test.exe -v2
```
