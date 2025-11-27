# Сборка и запуск проекта DONTFLOAT в VS Code и подобных средах (например, Cursor)

## Быстрый старт

### Windows

#### 1. Настройка CMake (первый раз)

Если проект ещё не настроен через CMake:

1. Откройте терминал в VS Code/Cursor (`Ctrl+`` ` или View → Terminal)
2. Выполните:
   ```bash
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   ```

Или используйте встроенную поддержку CMake Tools:
- VS Code автоматически определит CMake проект
- Нажмите на статус-бар "Configure" для настройки

### Linux

#### 1. Настройка CMake (первый раз)

**Требования**:
- CMake 3.16 или выше
- Qt 6.8+ или 6.9+ (установлен через пакетный менеджер или вручную)
- Компилятор: GCC 9+ или Clang 10+ с поддержкой C++17
- Зависимости Qt: `libqt6core6`, `libqt6gui6`, `libqt6widgets6`, `libqt6multimedia6`

**Установка зависимостей (Ubuntu/Debian)**:
```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev qt6-multimedia-dev
```

**Установка зависимостей (Fedora/RHEL)**:
```bash
sudo dnf install gcc-c++ cmake qt6-qtbase-devel qt6-qtmultimedia-devel
```

**Установка зависимостей (Arch Linux)**:
```bash
sudo pacman -S base-devel cmake qt6-base qt6-multimedia
```

**Настройка проекта**:
1. Откройте терминал в VS Code/Cursor (`Ctrl+`` ` или View → Terminal)
2. Выполните:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   ```

Или используйте встроенную поддержку CMake Tools:
- VS Code автоматически определит CMake проект
- Нажмите на статус-бар "Configure" для настройки

### macOS

#### 1. Настройка CMake (первый раз)

**Требования**:
- CMake 3.16 или выше
- Qt 6.8+ или 6.9+ (установлен через Homebrew или вручную)
- Компилятор: Xcode Command Line Tools (Clang с поддержкой C++17)

**Установка через Homebrew (рекомендуется)**:
```bash
# Установка Homebrew (если не установлен)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Установка зависимостей
brew install cmake qt@6
```

**Настройка переменных окружения**:
```bash
# Добавьте в ~/.zshrc или ~/.bash_profile
export CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@6"
export PATH="/opt/homebrew/opt/qt@6/bin:$PATH"
```

**Настройка проекта**:
1. Откройте терминал в VS Code/Cursor (`Ctrl+`` ` или View → Terminal)
2. Выполните:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   ```

Или используйте встроенную поддержку CMake Tools:
- VS Code автоматически определит CMake проект
- Нажмите на статус-бар "Configure" для настройки

### 2. Сборка проекта

#### Windows

**Вариант A: Через CMake Tools (рекомендуется)**
- Нажмите `F7` или кнопку "Build" в статус-баре CMake Tools
- Или выберите конфигурацию и нажмите на кнопку сборки

**Вариант B: Через Tasks**
- `Ctrl+Shift+P` → `Tasks: Run Task` → `CMake: Build Debug (if CMake installed)`

**Вариант C: Через командную строку**
```bash
cmake --build build --config Debug
```

#### Linux / macOS

**Вариант A: Через CMake Tools (рекомендуется)**
- Нажмите `F7` или кнопку "Build" в статус-баре CMake Tools
- Или выберите конфигурацию и нажмите на кнопку сборки

**Вариант B: Через Tasks**
- `Ctrl+Shift+P` (или `Cmd+Shift+P` на macOS) → `Tasks: Run Task` → `CMake: Build Debug`

**Вариант C: Через командную строку**
```bash
# Debug сборка
cmake --build build --config Debug

# Или для одноконфигурационных генераторов (Unix Makefiles)
cmake --build build
```

### 3. Запуск приложения

#### Windows

**Вариант A: Через Launch Configuration**
- Откройте вкладку "Run and Debug" (F5)
- Выберите "Debug DONTFLOAT (CMake build)"
- Нажмите F5

**Вариант B: Через командную строку**
```bash
build\Debug\DONTFLOAT.exe
```

#### Linux

**Вариант A: Через Launch Configuration**
- Откройте вкладку "Run and Debug" (F5)
- Выберите "Debug DONTFLOAT (CMake build)"
- Нажмите F5

**Вариант B: Через командную строку**
```bash
./build/DONTFLOAT
```

#### macOS

**Вариант A: Через Launch Configuration**
- Откройте вкладку "Run and Debug" (F5)
- Выберите "Debug DONTFLOAT (CMake build)"
- Нажмите F5

**Вариант B: Через командную строку**
```bash
./build/DONTFLOAT.app/Contents/MacOS/DONTFLOAT
# или если собран как обычный исполняемый файл
./build/DONTFLOAT
```

## Тестирование

### Запуск теста на реальных файлах

**Вариант A: Через Launch Configuration**
1. Откройте вкладку "Run and Debug" (F5)
2. Выберите "Run BPM File Test"
3. Нажмите F5

**Вариант B: Через Task**
- `Ctrl+Shift+P` → `Tasks: Run Task` → `CMake: Run BPM File Test`

**Вариант C: Через командную строку**
```bash
# Сначала соберите тест
cmake --build build --config Debug --target donfloat_file_test

# Затем запустите
build\Debug\donfloat_file_test.exe
```

### Запуск всех тестов через CTest

**Вариант A: Через Task**
- `Ctrl+Shift+P` → `Tasks: Run Task` → `CMake: Run All Tests`

**Вариант B: Через командную строку**
```bash
ctest --test-dir build --output-on-failure
```

## Доступные Tasks

### Сборка
- `CMake: Configure` - Настройка CMake проекта
- `CMake: Build Debug` - Сборка основного приложения в Debug режиме
- `CMake: Build Tests` - Сборка тестовых исполняемых файлов

### Тестирование
- `CMake: Run BPM File Test` - Запуск теста на реальных файлах
- `CMake: Run All Tests` - Запуск всех тестов через CTest

### Старые задачи (qmake)
- `Qt: Build with qmake (Debug)` - Сборка через qmake (для совместимости)
- `Qt: Build with qmake (Release)` - Release сборка через qmake

## Доступные Launch Configurations

1. **Debug DONTFLOAT (CMake build)** - Запуск основного приложения с отладкой
2. **Run BPM File Test** - Запуск теста на реальных аудиофайлах
3. **Run Unit Tests** - Запуск unit тестов

## Требования

### Windows
- **CMake**: 3.16 или выше
- **Qt**: 6.8+ или 6.9+ (MSVC 2022 64-bit)
- **Visual Studio**: 2022 (для компилятора MSVC)
- **Компилятор**: MSVC 2022 с поддержкой C++17

### Linux
- **CMake**: 3.16 или выше
- **Qt**: 6.8+ или 6.9+ (через пакетный менеджер или вручную)
- **Компилятор**: GCC 9+ или Clang 10+ с поддержкой C++17
- **Зависимости**: 
  - `build-essential` (Ubuntu/Debian)
  - `qt6-base-dev qt6-multimedia-dev` (Ubuntu/Debian)
  - `qt6-qtbase-devel qt6-qtmultimedia-devel` (Fedora/RHEL)
  - `base-devel qt6-base qt6-multimedia` (Arch Linux)

### macOS
- **CMake**: 3.16 или выше
- **Qt**: 6.8+ или 6.9+ (через Homebrew или вручную)
- **Xcode Command Line Tools**: Установлены через `xcode-select --install`
- **Компилятор**: Clang с поддержкой C++17
- **Homebrew**: Рекомендуется для установки зависимостей

## Структура сборки

### Windows

После настройки CMake будет создана структура:
```
build/
├── Debug/
│   ├── DONTFLOAT.exe          # Основное приложение
│   ├── bpm_analyzer_test.exe  # Unit тесты
│   ├── key_analyzer_test.exe
│   ├── metronome_controller_test.exe
│   └── bpm_file_test.exe      # Тест на реальных файлах
└── [другие файлы сборки]
```

### Linux / macOS

После настройки CMake будет создана структура:
```
build/
├── DONTFLOAT                   # Основное приложение
├── bpm_analyzer_test           # Unit тесты
├── key_analyzer_test
├── metronome_controller_test
└── bpm_file_test               # Тест на реальных файлах
```

**Примечание для macOS**: Если настроен как bundle, приложение будет в `DONTFLOAT.app/Contents/MacOS/DONTFLOAT`

## Решение проблем

### CMake не находит Qt

#### Windows

**Проблема**: `Qt не найден! Установите Qt 6.8.3 или 6.9.3`

**Решение**: 
1. Убедитесь, что Qt установлен в `C:/Qt/6.9.3/msvc2022_64` или `C:/Qt/6.8.3/msvc2022_64`
2. Или укажите путь к Qt через переменную окружения:
   ```cmd
   set CMAKE_PREFIX_PATH=C:/Qt/6.9.3/msvc2022_64
   ```
3. Или укажите при конфигурации:
   ```bash
   cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.9.3/msvc2022_64 -G "Visual Studio 17 2022" -A x64
   ```

#### Linux

**Проблема**: `Could not find a package configuration file provided by "Qt6"`

**Решение**:
1. Убедитесь, что Qt6 установлен через пакетный менеджер:
   ```bash
   # Ubuntu/Debian
   sudo apt install qt6-base-dev qt6-multimedia-dev
   
   # Fedora/RHEL
   sudo dnf install qt6-qtbase-devel qt6-qtmultimedia-devel
   
   # Arch Linux
   sudo pacman -S qt6-base qt6-multimedia
   ```
2. Если Qt установлен вручную, укажите путь:
   ```bash
   cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/qt6 -DCMAKE_BUILD_TYPE=Debug
   ```

#### macOS

**Проблема**: `Could not find a package configuration file provided by "Qt6"`

**Решение**:
1. Установите Qt через Homebrew:
   ```bash
   brew install qt@6
   ```
2. Установите переменные окружения:
   ```bash
   export CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@6"
   export PATH="/opt/homebrew/opt/qt@6/bin:$PATH"
   ```
3. Добавьте в `~/.zshrc` или `~/.bash_profile`:
   ```bash
   echo 'export CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@6"' >> ~/.zshrc
   echo 'export PATH="/opt/homebrew/opt/qt@6/bin:$PATH"' >> ~/.zshrc
   source ~/.zshrc
   ```
4. Или укажите при конфигурации:
   ```bash
   cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt@6 -DCMAKE_BUILD_TYPE=Debug
   ```

### Ошибка компиляции

#### Windows

**Проблема**: Ошибки компиляции при сборке

**Решение**:
1. Убедитесь, что используется правильный компилятор (MSVC 2022)
2. Проверьте, что все зависимости Qt установлены
3. Очистите директорию build и пересоберите:
   ```cmd
   rmdir /s /q build
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Debug
   ```

#### Linux / macOS

**Проблема**: Ошибки компиляции при сборке (например, `undefined reference to kiss_fft`)

**Решение**:
1. Убедитесь, что используется правильный компилятор (GCC 9+ или Clang 10+)
2. Проверьте, что все зависимости Qt установлены
3. Очистите директорию build и пересоберите:
   ```bash
   rm -rf build
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   cmake --build build
   ```
4. Если ошибки связаны с kiss_fft, убедитесь, что проект настроен с поддержкой C и C++:
   - Проверьте, что в `CMakeLists.txt` указано `LANGUAGES C CXX`
   - Убедитесь, что C файлы компилируются с `LANGUAGE C`

### Тест не находит файлы

**Проблема**: `Test directory not found` или `No MP3 files found`

**Решение** (Windows / Linux / macOS):
1. Убедитесь, что файлы находятся в `tests/source4test/`
2. Проверьте рабочую директорию при запуске теста
3. В CMakeLists.txt установлена `WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}`
4. Запустите тест из корневой директории проекта:
   ```bash
   # Windows
   cd D:\Devs\C++\DONTFLOAT_exp\DONTFLOAT
   build\Debug\bpm_file_test.exe
   
   # Linux / macOS
   cd /path/to/DONTFLOAT
   ./build/bpm_file_test
   ```

### Отладка не запускается

#### Windows

**Проблема**: Launch configuration не запускается

**Решение**:
1. Убедитесь, что приложение собрано (`CMake: Build Debug`)
2. Проверьте путь к исполняемому файлу в `launch.json`
3. Убедитесь, что Qt DLL доступны (через PATH в environment)

#### Linux

**Проблема**: Launch configuration не запускается или не находит библиотеки

**Решение**:
1. Убедитесь, что приложение собрано (`CMake: Build Debug`)
2. Проверьте путь к исполняемому файлу в `launch.json`
3. Убедитесь, что Qt библиотеки доступны:
   ```bash
   # Проверка установки Qt
   pkg-config --modversion Qt6Core
   
   # Если библиотеки не найдены, добавьте в LD_LIBRARY_PATH
   export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH
   ```
4. Для отладки может потребоваться установить `gdb`:
   ```bash
   sudo apt install gdb  # Ubuntu/Debian
   sudo dnf install gdb  # Fedora/RHEL
   ```

#### macOS

**Проблема**: Launch configuration не запускается или не находит библиотеки

**Решение**:
1. Убедитесь, что приложение собрано (`CMake: Build Debug`)
2. Проверьте путь к исполняемому файлу в `launch.json`
3. Убедитесь, что Qt библиотеки доступны:
   ```bash
   # Проверка установки Qt
   brew list qt@6
   
   # Если библиотеки не найдены, добавьте в DYLD_LIBRARY_PATH
   export DYLD_LIBRARY_PATH=/opt/homebrew/opt/qt@6/lib:$DYLD_LIBRARY_PATH
   ```
4. Для отладки может потребоваться установить `lldb` (обычно входит в Xcode Command Line Tools)

## Дополнительная информация

- **Документация тестов**: `tests/BPM_FILE_TEST_README.md`
- **Правила разработки**: `MARKDOWN/INIT.MD`
- **Проблемы и планы**: `MARKDOWN/ISSUES_AND_PLANS.md`

## Платформо-специфичные заметки

### Linux

- **Генератор CMake**: По умолчанию используется "Unix Makefiles"
- **Пути к библиотекам**: Qt обычно устанавливается в `/usr/lib/x86_64-linux-gnu/` или `/usr/lib64/`
- **Отладчик**: Используйте `gdb` для отладки
- **Запуск приложения**: Может потребоваться установка переменной `LD_LIBRARY_PATH` для поиска Qt библиотек

### macOS

- **Генератор CMake**: По умолчанию используется "Unix Makefiles"
- **Пути к библиотекам**: При установке через Homebrew Qt находится в `/opt/homebrew/opt/qt@6/`
- **Отладчик**: Используйте `lldb` для отладки (входит в Xcode Command Line Tools)
- **Bundle**: Приложение может быть собрано как `.app` bundle или как обычный исполняемый файл
- **Запуск приложения**: Может потребоваться установка переменной `DYLD_LIBRARY_PATH` для поиска Qt библиотек

### Windows

- **Генератор CMake**: Рекомендуется "Visual Studio 17 2022" для MSVC
- **Пути к библиотекам**: Qt обычно устанавливается в `C:/Qt/6.x.x/msvc2022_64/`
- **Отладчик**: Используется встроенный отладчик Visual Studio через VS Code
- **DLL**: Qt DLL должны быть доступны через PATH или находиться рядом с исполняемым файлом

---

**Версия**: 0.2  
**Дата создания**: 2025-01-27  
**Обновлено**: 2025-01-27 - Добавлены инструкции для Linux и macOS
