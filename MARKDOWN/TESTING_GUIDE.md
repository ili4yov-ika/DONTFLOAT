# Система тестирования DONTFLOAT

## Обзор

Система тестирования DONTFLOAT включает функциональные тесты, тесты BPM анализа, синтетические тесты и диагностические инструменты.

## Структура тестов

```
tests/
├── test_bpm/                    # Тестовые аудиофайлы
│   └── example_80BPM.mp3       # Пример файла для тестирования BPM
├── CMakeLists.txt              # Конфигурация CMake для тестов
├── README.md                   # Документация тестов
├── test_bpm_analysis.cpp       # Полная система тестирования BPM
├── simple_synthetic_test.cpp   # Синтетический тест
├── functional_test.bat         # Функциональный тест
├── ui_diagnostic_test.bat      # Диагностика UI
├── color_fix_test.bat          # Тест цветовых схем
├── dark_theme_fix_test.bat     # Тест темной темы
├── ui_fixes_test.bat           # Тест исправлений UI
└── updated_functional_test.bat # Обновленный функциональный тест
```

## Типы тестов

### 1. Функциональные тесты

#### functional_test.bat
- Проверка запуска GUI режима
- Проверка справки
- Проверка консольного режима
- Проверка параметров командной строки

#### updated_functional_test.bat
- Обновленная версия функционального теста
- Тестирование с синтетическими данными
- Проверка всех режимов работы

### 2. BPM тесты

#### test_bpm_analysis.cpp
- Полная система тестирования BPM анализатора
- Тестирование с реальными аудиофайлами
- Проверка различных алгоритмов анализа
- Валидация результатов

#### simple_synthetic_test.cpp
- Синтетический тест для проверки BPM анализатора
- Использование искусственных данных
- Независимость от внешних файлов
- Быстрое выполнение

### 3. Тесты тонкомпенсации (Time Stretch)

#### timestretchprocessor_test.cpp
- Юнит-тесты `TimeStretchProcessor`: длина выхода, `calculateStretchFactor`, синтетические сигналы

#### pitch_compensation_file_test.cpp
- Интеграционный тест на `tests/source4test/pitch-test_C140BPM.mp3` (одна нота ~165 Hz)
- `applyMarkerStretch()` с Rubber Band: сжатие/растяжение метками (×0.5 … ×2.0), проверка f0 автокорреляцией (±6%)
- Контроль без тонкомпенсации (ожидаемый сдвиг f0)
- Локально (~35–40 с); в CI — `QSKIP`

```powershell
cmake --build build/Debug --config Debug --target pitch_compensation_file_test
.\build\Debug\Debug\pitch_compensation_file_test.exe -v2
```

Подробнее: [tests/README.md](../tests/README.md), [TIMESTRETCH_FEATURE.md](TIMESTRETCH_FEATURE.md).

### 4. UI тесты

#### ui_responsiveness_test.cpp (Qt Test, рекомендуется)

Интеграционный тест на `tests/source4test/example_V80BPM.mp3`. Запуск **из корня репозитория** с `DONTFLOAT_RUN_UI_TEST=1` (в CI по умолчанию `QSKIP`).

| Слот | Сценарий |
|------|----------|
| `testLoadAnalyzeAndCreateMarkers` | MP3 → BPM/доли → метки выравнивания |
| `testMarkerDragUiResponsiveness` | Одно перетаскивание метки, лимит времени |
| `testMarkerDragWorkflowThreeRandom` | +2 случайные метки, 3 drag подряд |
| `testApplyTimeStretchAfterAlignment` | Полный time stretch по меткам |
| `testProcessedPlaybackSmoothness` | Плавность `QMediaPlayer` после обработки |

```powershell
cmake --build build/Desktop_Qt_6_9_3_MinGW_64_bit-Debug --target ui_responsiveness_test
$env:DONTFLOAT_RUN_UI_TEST = "1"
.\build\Desktop_Qt_6_9_3_MinGW_64_bit-Debug\ui_responsiveness_test.exe -v2
```

Переменные: `DONTFLOAT_UI_DRAG_MAX_MS` (45000), `DONTFLOAT_UI_DRAG_TOTAL_MAX_MS` (135000). Подробнее: [tests/README.md](../tests/README.md).

#### ui_diagnostic_test.bat (legacy)
- Диагностика проблем с интерфейсом
- Проверка реактивности UI
- Тестирование закрытия программы
- Проверка работы меню

#### ui_fixes_test.bat (legacy)
- Тест исправлений UI
- Проверка блокировки интерфейса
- Тестирование флагов окна
- Проверка темной палитры

### 5. Цветовые тесты

#### color_fix_test.bat
- Тест исправления цветового оформления
- Проверка единообразности стилей
- Тестирование светлой темы
- Проверка смешения цветов

#### dark_theme_fix_test.bat
- Тест исправления темной темы
- Проверка единообразного темного оформления
- Тестирование запоминания настроек
- Проверка переключения тем

## Запуск тестов

### Windows (Batch файлы)
```cmd
cd tests
.\functional_test.bat
.\ui_diagnostic_test.bat
.\color_fix_test.bat
.\dark_theme_fix_test.bat
```

### CMake тесты
```cmd
cd tests
mkdir build
cd build
cmake ..
make
./test_bpm_analysis
./simple_synthetic_test
```

## Результаты тестирования

### Ожидаемые результаты

#### Функциональные тесты
- ✅ GUI режим: Работает
- ✅ Справка: Работает
- ✅ Консольный режим: Реальная загрузка и анализ через `AudioFileService`
- ✅ Параметры командной строки: Работают

#### UI тесты
- ✅ Открытие окна программы
- ✅ Реакция на клики мыши
- ✅ Закрытие программы на крестик
- ✅ Работа меню и кнопок

#### Цветовые тесты
- ✅ Единообразное оформление
- ✅ Отсутствие смешения цветов
- ✅ Запоминание выбранной темы
- ✅ Корректное переключение тем

### Диагностика проблем

#### Если тесты не проходят
1. **Проверьте наличие исполняемого файла**
   ```cmd
   dir build\Desktop_Qt_6_8_3_MSVC2022_64bit-Debug\debug\DONTFLOAT.exe
   ```

2. **Пересоберите проект**
   ```cmd
   # В Qt Creator:
   Build → Clean All
   Build → Rebuild All
   ```

3. **Проверьте Qt библиотеки**
   ```cmd
   dir Qt6*.dll
   dir Qt6Core*.dll
   dir Qt6Gui*.dll
   dir Qt6Widgets*.dll
   ```

## Синтетические тесты

### Генерация тестовых данных
```cpp
// Создание синусоидального сигнала с битами
const float testBPM = 80.0f;
const float duration = 5.0f;
const int totalSamples = static_cast<int>(sampleRate * duration);

for (int i = 0; i < totalSamples; ++i) {
    float sample = 0.0f;
    
    // Основной тон (440 Hz)
    sample += 0.3f * std::sin(2.0f * M_PI * 440.0f * i / sampleRate);
    
    // Биты (короткие импульсы)
    if (i % samplesPerBeat < samplesPerBeat / 10) {
        sample += 0.8f * std::sin(2.0f * M_PI * 880.0f * i / sampleRate);
    }
    
    // Шум для реалистичности
    sample += 0.1f * ((float)rand() / RAND_MAX - 0.5f);
    
    samples.append(sample);
}
```

### Преимущества синтетических тестов
- Независимость от внешних файлов
- Предсказуемые результаты
- Быстрое выполнение
- Легкая настройка параметров

## Консольный режим

### Тестирование консольного режима
```cmd
# Базовый анализ
DONTFLOAT.exe -c -f test.mp3

# Анализ с алгоритмом Mixxx
DONTFLOAT.exe -c -f test.mp3 --mixxx

# Анализ с параметрами
DONTFLOAT.exe -c -f test.mp3 --min-bpm 70 --max-bpm 90

# Переменный темп
DONTFLOAT.exe -c -f test.mp3 --variable-tempo
```

### Ожидаемый вывод
```
=== Консольный режим DONTFLOAT ===
Анализ файла: test.mp3
Загрузка аудиофайла...
Файл загружен успешно:
  - Размер: … сэмплов
  - Частота дискретизации: … Hz
  - Длительность: … секунд

=== РЕЗУЛЬТАТЫ АНАЛИЗА ===
Определенный BPM: ~80
Уверенность: >70%
Количество обнаруженных битов: >20

=== АНАЛИЗ ЗАВЕРШЕН ===
```

## Отладка тестов

### Логирование
- Использование qDebug() для логирования
- Проверка корректности аудиофайлов
- Валидация входных параметров
- Обработка исключений

### Производительность
- Отрисовка только видимой области
- Агрегация сэмплов при масштабировании
- Кэширование промежуточных вычислений
- Асинхронная обработка

## Непрерывная интеграция

### Автоматизация тестов
- Запуск тестов при коммитах
- Проверка всех компонентов
- Валидация входных данных
- Тестирование граничных случаев

### Отчеты
- Генерация отчетов о тестировании
- Отслеживание покрытия кода
- Мониторинг производительности
- Анализ ошибок
