# Тесты DONTFLOAT

Этот каталог содержит тесты для проекта DONTFLOAT, созданные согласно документации Qt Test.

## Структура тестов

В настоящее время реализованы следующие тесты:

- **bpm_analyzer_test.cpp** - Интеграционный тест анализатора BPM на реальных аудиофайлах из `source4test/`
- **beat_deviation_test.cpp** - Юнит-тесты для вычисления отклонений долей и поиска неровных долей (новое с 2026-01-12)

## Тестовые данные

Тестовые аудиофайлы находятся в `tests/source4test/`:

- `example_C80BPM.mp3` - Файл с постоянными 80 BPM
- `example_V80BPM.mp3` - Файл с переменными 80 BPM

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
