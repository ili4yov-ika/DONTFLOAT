# Тесты DONTFLOAT

Этот каталог содержит тесты для проекта DONTFLOAT, созданные согласно документации Qt Test.

## Структура тестов

В настоящее время реализованы следующие тесты:

- **bpm_analyzer_test.cpp** - Интеграционный тест анализатора BPM на реальных аудиофайлах из `source4test/`
- **beat_deviation_test.cpp** - Юнит-тесты для вычисления отклонений долей и поиска неровных долей (новое с 2026-01-12)
- **ui_responsiveness_test.cpp** - Интеграционный UI-тест: загрузка `example_V80BPM.mp3`, метки выравнивания, перетаскивание меток, `applyTimeStretch`, плавность `QMediaPlayer`
- **pitch_compensation_file_test.cpp** - Тонкомпенсация на `pitch-test_C140BPM.mp3` (одна нота): сжатие/растяжение метками, проверка высоты тона (автокорреляция)

## Тестовые данные

Тестовые аудиофайлы находятся в `tests/source4test/`:

- `example_C80BPM.mp3` - Файл с постоянными 80 BPM
- `example_V80BPM.mp3` - Файл с переменными 80 BPM
- `pitch-test_C140BPM.mp3` - ~140 BPM, одна устойчивая нота (тест тонкомпенсации)

### pitch_compensation_file_test

Интеграционный тест тонкомпенсации (Rubber Band R3) на реальном MP3. Локально, не в CI по умолчанию (~35–40 с).

**Файл:** `pitch-test_C140BPM.mp3` — эталон **f0 ≈ 165 Hz** (автокорреляция по средней части сигнала).

| Тест | Сценарий меток |
|------|----------------|
| `testWholeFileCompressHalf` | Весь файл ×0.5 |
| `testWholeFileStretchOneAndHalf` | Весь файл ×1.5 |
| `testWholeFileStretchDouble` | Весь файл ×2.0 |
| `testFirstHalfCompressSecondUnchanged` | Первая половина ×0.5, вторая ×1 |
| `testSecondHalfStretchFirstUnchanged` | Первая ×1, вторая ×1.5 |
| `testWithoutPitchCompensationPitchShifts` | ×0.5 без тонкомпенсации → f0 ×2 (~330 Hz) |

Допуск при тонкомпенсации: **±6%** от исходного f0.

```powershell
cmake --build build/Debug --config Debug --target pitch_compensation_file_test
.\build\Debug\Debug\pitch_compensation_file_test.exe -v2
```

Через CTest:

```powershell
ctest --test-dir build/Debug -C Debug -R pitch_compensation_file_test --output-on-failure
```

В CI/GitHub Actions тест пропускается (декодер MP3 и Rubber Band зависят от окружения).

### ui_responsiveness_test

Интеграционный UI-тест на `example_V80BPM.mp3` (локально, не в CI по умолчанию). Требует `QApplication`, декодер MP3 и (для части сценариев) мультимедиа-плеер. **Рабочая директория — корень репозитория** (`WORKING_DIRECTORY` задан в `CMakeLists.txt`).

#### Слоты теста

| Слот | Что проверяет |
|------|----------------|
| `testLoadAnalyzeAndCreateMarkers` | Загрузка MP3, BPM/доли (Mixxx), создание меток выравнивания |
| `testMarkerDragUiResponsiveness` | Одно перетаскивание метки (симуляция мыши), изменение `position`, время отклика |
| `testMarkerDragWorkflowThreeRandom` | Полный workflow: анализ долей → **2 случайные метки** → **3 последовательных drag** разных меток, суммарное время |
| `testApplyTimeStretchAfterAlignment` | `applyTimeStretch` по меткам, длина результата и время обработки |
| `testProcessedPlaybackSmoothness` | Воспроизведение обработанного WAV через `QMediaPlayer`, монотонность позиции |

Симуляция drag использует `QApplication::sendEvent` и hit-test по координатам метки (как в `WaveformView::getMarkerIndexAt`). Успех drag определяется по **изменению `position`**, а не только по сигналу `markerDragFinished` (при невалидной геометрии stretch сигнал может не прийти).

#### Сборка и запуск (Windows)

```powershell
# из корня репозитория
cmake --preset windows-mingw-debug
cmake --build --preset windows-mingw-debug --target ui_responsiveness_test

$env:DONTFLOAT_RUN_UI_TEST = "1"
.\build\Desktop_Qt_6_9_3_MinGW_64_bit-Debug\ui_responsiveness_test.exe -v2

# один сценарий
.\build\Desktop_Qt_6_9_3_MinGW_64_bit-Debug\ui_responsiveness_test.exe testMarkerDragWorkflowThreeRandom
```

Через CTest (тоже из корня, с переменной окружения):

```powershell
$env:DONTFLOAT_RUN_UI_TEST = "1"
ctest --test-dir build/Desktop_Qt_6_9_3_MinGW_64_bit-Debug -R ui_responsiveness_test --output-on-failure
```

Без `DONTFLOAT_RUN_UI_TEST=1` тест **пропускается** (`QSKIP`), если заданы `CI` или `GITHUB_ACTIONS`.

#### Переменные окружения (опционально)

| Переменная | По умолчанию | Назначение |
|------------|--------------|------------|
| `DONTFLOAT_RUN_UI_TEST` | — | Принудительный запуск (иначе `QSKIP` в CI) |
| `DONTFLOAT_UI_DRAG_MAX_MS` | 45000 | Лимит времени одного перетаскивания метки |
| `DONTFLOAT_UI_DRAG_TOTAL_MAX_MS` | 135000 | Лимит суммарного времени трёх drag в workflow-тесте |
| `DONTFLOAT_UI_DRAG_WAIT_MS` | 300 | Ожидание `markerDragFinished` после отпускания кнопки |
| `DONTFLOAT_UI_STRETCH_MAX_MS` | 120000 | Лимит `applyTimeStretch` |
| `DONTFLOAT_UI_MEDIA_LOAD_MS` | 10000 | Таймаут загрузки WAV в плеер |

## Сборка тестов

Тесты автоматически собираются при сборке проекта через CMake.

### Windows (Command Prompt или PowerShell)

```cmd
cd D:\Devs\C++\DONTFLOAT_exp\DONTFLOAT
cmake -B build
cmake --build build --config Debug
```

Или если build уже настроен:
```cmd
cmake --build build --config Debug
```

### Linux/Mac

```bash
cmake -B build
cmake --build build
```

**Важно:** Убедитесь, что проект собран перед запуском тестов!

## Запуск тестов

### Способ 1: Через скрипт (самый простой)

**Windows (PowerShell):**
```powershell
cd tests
.\run_tests.ps1
```

Или запуск конкретного теста:
```powershell
.\run_tests.ps1 bpm_analyzer_test
```

**Linux/Mac:**
```bash
cd tests
chmod +x run_tests.sh
./run_tests.sh
```

### Способ 2: Через CTest (рекомендуется)

**Windows (Command Prompt):**
```cmd
cd build
ctest
```

**Windows (PowerShell):**
```powershell
cd build
ctest
```

С подробным выводом:
```cmd
ctest --output-on-failure
```

Запуск конкретного теста:
```cmd
cd build
ctest -R bpm_analyzer_test
```

Если сборка лежит в подкаталоге пресета (например `build/Desktop_Qt_6_9_3_MSVC2022_64bit-Debug`), укажите его явно:

```powershell
ctest --test-dir build/Desktop_Qt_6_9_3_MSVC2022_64bit-Debug -C Debug
```

На Windows CMake подставляет Qt в `PATH` для CTest через скрипт `tests/RunQtTest.cmake` (иначе возможна ошибка загрузки DLL `0xc0000135`).

### Способ 3: Напрямую через исполняемый файл

**Windows:**
```cmd
cd build\Debug
bpm_analyzer_test.exe
```

**Linux/Mac:**
```bash
cd build
./bpm_analyzer_test
```

### Способ 4: Через Visual Studio

1. Откройте `build\DONTFLOAT.sln` в Visual Studio
2. В **Test Explorer** (Test → Test Explorer) вы увидите все тесты
3. Нажмите **Run All** или выберите конкретные тесты

### Способ 5: Через Qt Creator

1. Откройте проект в Qt Creator
2. Перейдите в меню **Build** → **Run Tests** (или нажмите `Ctrl+6`)
3. Выберите нужные тесты и запустите их
4. Результаты отобразятся в панели **Test Results**

### Способ 6: Через CMake (из командной строки)

```cmd
cd build
cmake --build . --target RUN_TESTS
```

Или:
```cmd
cmake --build build --target RUN_TESTS
```

## Описание тестов

### BPMAnalyzerTest

Тестирует функциональность анализа BPM на реальных аудиофайлах:

- `testAnalyzeBPMFromSourceFiles()` - Анализ BPM из файлов в `source4test/`
  - Тестирует оба файла: `example_C80BPM.mp3` и `example_V80BPM.mp3`
  - Проверяет работу упрощенного алгоритма
  - Проверяет работу алгоритма Mixxx (если доступен)
  - Проверяет корректность обнаруженного BPM
  - Проверяет наличие обнаруженных битов
  - Проверяет корректность уверенности (confidence)

**Что проверяется:**
- BPM должен быть больше 0 и меньше 300
- Должно быть обнаружено хотя бы несколько битов
- Уверенность должна быть в диапазоне [0, 1]
- Для файлов с постоянным темпом (C80BPM) BPM должен быть близок к 80 (с допуском)

**Примечание**: Тест использует `QAudioDecoder` для загрузки реальных MP3 файлов, поэтому требует наличия соответствующих кодеков в системе.

### BeatDeviationTest

Тестирует новую функциональность вычисления отклонений долей (добавлено 2026-01-12):

- `testCalculateDeviations()` - Проверяет корректность вычисления отклонений через `BPMAnalyzer::calculateDeviations()`
  - Создает тестовые доли с известными позициями
  - Проверяет правильность вычисления `expectedPosition`
  - Проверяет правильность вычисления `deviation` (отклонения)
  - Проверяет знаки отклонений (положительные/отрицательные)

- `testFindUnalignedBeats()` - Проверяет поиск неровных долей через `BPMAnalyzer::findUnalignedBeats()`
  - Проверяет работу с разными порогами отклонения (2%, 5%, 15%)
  - Проверяет правильность определения неровных долей
  - Проверяет, что доли с малым отклонением не попадают в список

- `testExpectedPositionInitialization()` - Проверяет корректность инициализации поля `expectedPosition`
  - Проверяет инициализацию в одиночном `BeatInfo`
  - Проверяет инициализацию в векторе долей

**Что проверяется:**
- Правильность вычисления ожидаемых позиций долей
- Правильность вычисления отклонений в долях (normalized deviation)
- Корректность определения неровных долей по порогу
- Инициализация нового поля `expectedPosition` в структуре `BeatInfo`

**Примечание**: Это юнит-тест, не требует внешних файлов, работает с синтетическими данными.

## Добавление новых тестов

Для добавления нового теста согласно документации Qt Test:

1. Создайте новый файл `tests/your_test.cpp`
2. Используйте структуру:

```cpp
#include <QtTest/QtTest>

class YourTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testYourFunction();
};

void YourTest::initTestCase()
{
    // Инициализация перед всеми тестами
}

void YourTest::cleanupTestCase()
{
    // Очистка после всех тестов
}

void YourTest::init()
{
    // Инициализация перед каждым тестом
}

void YourTest::cleanup()
{
    // Очистка после каждого теста
}

void YourTest::testYourFunction()
{
    // Ваш тест
    QVERIFY(condition);
    QCOMPARE(actual, expected);
}

QTEST_MAIN(YourTest)
#include "your_test.moc"
```

3. Добавьте тест в `CMakeLists.txt`:

```cmake
add_qt_test(your_test
    tests/your_test.cpp
    src/your_source.cpp
)

set_tests_properties(your_test PROPERTIES
    LABELS "your;label"
    DESCRIPTION "Your test description"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}  # Если нужен доступ к файлам
)
```

## Макросы Qt Test

Основные макросы для тестирования:

- `QVERIFY(condition)` - Проверяет условие, прерывает тест при ошибке
- `QVERIFY2(condition, message)` - С сообщением об ошибке
- `QCOMPARE(actual, expected)` - Сравнивает два значения
- `QSKIP(message)` - Пропускает тест с сообщением
- `QTEST_MAIN(TestClass)` - Создает точку входа для теста

## Документация

Подробная документация по Qt Test:
- [Qt Test Documentation](https://doc.qt.io/qtcreator/creator-how-to-create-qt-tests.html)
- [Qt Test Framework](https://doc.qt.io/qt-6/qtest-overview.html)

## Решение проблем

### Тест не находит файлы

**Проблема**: `Тестовый файл не найден`

**Решение**:
1. Убедитесь, что файлы находятся в `tests/source4test/`
2. Запускайте тест из корневой директории проекта (рабочая директория установлена в CMakeLists.txt)
3. Проверьте, что файлы существуют и доступны для чтения

### Ошибка декодирования аудио

**Проблема**: `Ошибка декодирования` или `неподдерживаемый формат`

**Решение**:
1. Убедитесь, что установлены кодеки для MP3 (обычно входят в Qt Multimedia)
2. Проверьте, что файлы не повреждены
3. Убедитесь, что Qt Multimedia правильно настроен в системе

### Тест пропускается (QSKIP)

**Проблема**: Тест пропускается с сообщением

**Решение**:
- Проверьте сообщение в выводе теста
- Убедитесь, что все зависимости доступны
- Проверьте, что тестовые данные на месте
