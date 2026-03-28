# Сборка и запуск проекта DONTFLOAT в VS Code и подобных средах (например, Cursor)

## ⚡ Быстрый старт для Windows (3 шага)

Если вы работаете в Windows и только открыли проект:

1. **Откройте проект** в VS Code/Cursor
2. **Нажмите `Ctrl+Shift+B`** - проект автоматически настроится и соберется
3. **Нажмите `F5`** - приложение запустится

Готово! 🎉

### ⚠️ ВАЖНО: Используйте F5, а не кнопку "Launch"

Если вы нажимаете кнопку **"Launch"** (▶️) на нижней панели CMake Tools, программа может не запуститься с окном.

**Решение:**
- Используйте `F5` (рекомендуется)
- Или перезагрузите VS Code после открытия проекта (для обновления настроек CMake Tools)

Подробнее: [КАК_ЗАПУСТИТЬ.md](КАК_ЗАПУСТИТЬ.md)

---

## Подробная инструкция

### Быстрый старт

### Windows

#### 🚀 Самый простой способ (рекомендуется)

1. **Откройте проект в VS Code/Cursor**
2. **Нажмите `Ctrl+Shift+B`** (или выберите Terminal → Run Build Task)
   - Выберите задачу `CMake: Build (Windows)` (по умолчанию)
   - Это автоматически настроит и соберет проект
3. **Нажмите `F5`** для запуска
   - Выберите `▶️ (CMake) Запуск DONTFLOAT - MSVC Debug`
   - Приложение запустится с отладчиком

#### Альтернативный способ: Ручная настройка CMake

Если хотите настроить CMake вручную:

1. Откройте терминал в VS Code/Cursor (`Ctrl+`` ` или View → Terminal)
2. Выполните:
   ```bash
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.9.3/msvc2022_64
   ```

Или используйте задачу:
- `Ctrl+Shift+P` → `Tasks: Run Task` → `CMake: Configure (Windows)`

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

**🎯 Рекомендуемый способ:**
- Нажмите `Ctrl+Shift+B` → автоматически выполнится сборка через CMake

**Другие способы:**

**Вариант A: Через меню задач**
- `Ctrl+Shift+P` → `Tasks: Run Task` → Выберите задачу:
  - `CMake: Build (Windows)` - Debug сборка (по умолчанию)
  - `CMake: Build Release (Windows)` - Release сборка
  - `CMake: Full Rebuild (Windows)` - Полная пересборка с очисткой
  - `CMake: Clean (Windows)` - Очистка

**Вариант B: Через CMake Tools (если расширение установлено)**
- Нажмите `F7` или кнопку "Build" в статус-баре CMake Tools

**Вариант C: Через командную строку**
```powershell
# Debug сборка
cmake --build build --config Debug -j

# Release сборка
cmake --build build --config Release -j
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

**🎯 Самый простой способ:**
- Нажмите `F5` - приложение автоматически соберется и запустится

**Доступные конфигурации запуска:**

1. **▶️ (CMake) Запуск DONTFLOAT - MSVC Debug**
   - Запуск приложения с отладкой через CMake
   - Автоматическая сборка перед запуском
   - Рекомендуется для повседневной разработки

2. **🐛 (CMake) Отладка DONTFLOAT - MSVC Debug**
   - Запуск в интегрированном терминале
   - Подробный вывод отладочной информации

3. **▶️ (qmake) Запуск DONTFLOAT - MinGW Debug**
   - Если используете qmake вместо CMake
   - Требует предварительной сборки через qmake

4. **▶️ (qmake) Запуск DONTFLOAT - MSVC Debug**
   - Для сборки через qmake с MSVC

5. **🧪 (CMake) Запуск тестов BPM**
   - Запуск unit-тестов BPM анализатора

**Через командную строку:**
```powershell
# CMake Debug
.\build\Debug\DONTFLOAT.exe

# CMake Release
.\build\Release\DONTFLOAT.exe
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

## Доступные задачи (Tasks)

### 🪟 Windows - CMake (рекомендуется)

**Основные:**
- `CMake: Configure (Windows)` - Настройка CMake проекта
- `CMake: Build (Windows)` ⭐ - Сборка Debug (по умолчанию, Ctrl+Shift+B)
- `CMake: Build Release (Windows)` - Сборка Release
- `CMake: Clean (Windows)` - Очистка сборки
- `CMake: Full Rebuild (Windows)` - Полная пересборка (удаление build)

**Тестирование:**
- `CMake: Build Tests (Windows)` - Сборка тестов
- `CMake: Run Tests (Windows)` - Запуск всех тестов через CTest

### 🪟 Windows - qmake (альтернатива)

**MSVC:**
- `Qt: Build with nmake (Windows MSVC2022 Debug)` - Debug сборка
- `Qt: Build with nmake (Windows MSVC2022 Release)` - Release сборка

**MinGW:**
- `Qt: Build with mingw32-make (Windows MinGW Debug)` - Debug сборка
- `Qt: Build with mingw32-make (Windows MinGW Release)` - Release сборка

### 🐧 Linux - CMake

- `CMake: Build Debug (Linux)` - Debug сборка
- `CMake: Build Release (Linux)` - Release сборка
- `CMake: Run Tests (Linux)` - Запуск тестов

## Доступные конфигурации запуска (Launch Configurations)

### 🪟 Windows

1. **▶️ (CMake) Запуск DONTFLOAT - MSVC Debug** ⭐ - Основной запуск с отладкой
2. **🐛 (CMake) Отладка DONTFLOAT - MSVC Debug** - Запуск в интегрированном терминале
3. **▶️ (qmake) Запуск DONTFLOAT - MinGW Debug** - Для qmake + MinGW
4. **▶️ (qmake) Запуск DONTFLOAT - MSVC Debug** - Для qmake + MSVC
5. **🧪 (CMake) Запуск тестов BPM** - Unit-тесты

### 🐧 Linux

1. **🐧 (Linux) CMake Debug** - Запуск с отладкой в Linux

## Требования

### Windows
- **CMake**: 3.16 или выше ([скачать](https://cmake.org/download/))
- **Qt**: 6.8+ или 6.9+ (MSVC 2022 64-bit) ([скачать](https://www.qt.io/download-qt-installer))
  - При установке выберите: `Qt 6.9.3` → `MSVC 2022 64-bit`
  - Также установите: `Qt Creator` (опционально)
- **Visual Studio**: 2022 Community/Professional/Enterprise ([скачать](https://visualstudio.microsoft.com/))
  - При установке выберите рабочую нагрузку: "Desktop development with C++"
  - Включите компоненты: MSVC v143, Windows 10/11 SDK
- **Компилятор**: MSVC 2022 с поддержкой C++17 (устанавливается с Visual Studio)
- **PowerShell**: 5.1 или выше (обычно уже установлен в Windows)

**Примечание**: Если у вас уже установлен Qt 6.8.3, проект будет работать с ним. CMakeLists.txt автоматически определит версию.

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

### Windows (CMake с Visual Studio 2022)

После настройки CMake будет создана структура:
```
build/
├── Debug/
│   ├── DONTFLOAT.exe          # Основное приложение (Debug)
│   ├── bpm_analyzer_test.exe  # Unit тесты
│   └── qm_dsp.lib             # Библиотека qm-dsp от Mixxx
├── Release/
│   ├── DONTFLOAT.exe          # Основное приложение (Release)
│   └── qm_dsp.lib
├── DONTFLOAT.sln              # Solution файл Visual Studio
├── DONTFLOAT.vcxproj          # Проект Visual Studio
└── [другие файлы сборки CMake]
```

### Windows (qmake с MSVC)

При использовании qmake:
```
build/Desktop_Qt_6_9_3_MSVC2022_64bit-Debug/
├── debug/
│   └── DONTFLOAT.exe          # Основное приложение
└── Makefile
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

### 🆘 Часто встречающиеся проблемы в Windows

#### ❌ Окно программы не появляется (программа запускается и сразу закрывается)

**Причина:** Qt не может найти необходимые DLL или плагины платформы (qwindows.dll в папке platforms).

**Диагностика:**
```powershell
# Запустите с отладкой Qt плагинов
$env:QT_DEBUG_PLUGINS = "1"
$env:PATH = "C:\Qt\6.9.3\msvc2022_64\bin;C:\Qt\6.9.3\msvc2022_64\plugins\platforms;C:\Qt\6.9.3\msvc2022_64\plugins;$env:PATH"
.\build\Desktop_Qt_6_9_3_MSVC2022_64bit-Debug\debug\DONTFLOAT.exe
```

**Решение 1 (РЕКОМЕНДУЕТСЯ):** Используйте F5 в VS Code/Cursor
- Все пути уже настроены в `.vscode/launch.json`
- Qt DLL и плагины будут найдены автоматически

**Решение 2:** Используйте скрипт запуска:
```powershell
# Запуск qmake + MSVC Debug
.\run_dontfloat.ps1

# Или другие варианты
.\run_dontfloat.ps1 qmake-mingw
.\run_dontfloat.ps1 cmake-debug
.\run_dontfloat.ps1 cmake-release
```

**Решение 3:** Настройте PATH вручную (для ручного запуска):
```powershell
# Правильный порядок: Qt пути ПЕРЕД системным PATH
$env:PATH = "C:\Qt\6.9.3\msvc2022_64\bin;C:\Qt\6.9.3\msvc2022_64\plugins\platforms;C:\Qt\6.9.3\msvc2022_64\plugins;$env:PATH"
$env:QT_PLUGIN_PATH = "C:\Qt\6.9.3\msvc2022_64\plugins"

# Теперь запустите
.\build\Desktop_Qt_6_9_3_MSVC2022_64bit-Debug\debug\DONTFLOAT.exe
```

**ВАЖНО:** Qt пути должны быть ПЕРЕД `${env:PATH}`, чтобы использовались правильные версии DLL.

#### ❌ Не запускается приложение / Ошибка "Не удается найти Qt6Core.dll"

**Решение:** См. решение выше - это та же проблема с PATH к Qt DLL

#### ❌ CMake не находит Visual Studio

**Решение:**
1. Убедитесь, что Visual Studio 2022 установлена
2. Проверьте установку компонента "Desktop development with C++"
3. Запустите VS Code/Cursor из "Developer Command Prompt for VS 2022"

#### ❌ Ошибка при сборке: "ninja: error" или "MSBuild не найден"

**Решение:**
Используйте генератор Visual Studio вместо Ninja:
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.9.3/msvc2022_64
```

### CMake не находит Qt

#### Windows

**Проблема**: `Qt не найден! Установите Qt 6.8.3 или 6.9.3`

**Решение**:
1. Убедитесь, что Qt установлен в `C:/Qt/6.9.3/msvc2022_64` или `C:/Qt/6.8.3/msvc2022_64`
2. Или укажите путь к Qt через переменную окружения:
   ```powershell
   $env:CMAKE_PREFIX_PATH = "C:/Qt/6.9.3/msvc2022_64"
   ```
3. Или укажите при конфигурации:
   ```powershell
   cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.9.3/msvc2022_64 -G "Visual Studio 17 2022" -A x64
   ```
4. Или измените путь в задаче `CMake: Configure (Windows)` в `.vscode/tasks.json`

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

## 💡 Рекомендации

### Для Windows разработчиков

1. **Используйте CMake** вместо qmake - это проще и быстрее
2. **Горячие клавиши:**
   - `Ctrl+Shift+B` - Сборка проекта
   - `F5` - Запуск с отладкой
   - `Ctrl+F5` - Запуск без отладки
   - `Shift+F5` - Остановить отладку
   - `F9` - Поставить/убрать точку останова
3. **IntelliSense**: C++ IntelliSense автоматически настроен в `c_cpp_properties.json`
4. **Логи сборки**: Все логи сборки отображаются в панели Terminal
5. **Чистая пересборка**: Если что-то пошло не так, используйте `CMake: Full Rebuild (Windows)`

### Расширения VS Code (рекомендуемые)

- **C/C++** (ms-vscode.cpptools) - IntelliSense и отладка
- **CMake** (twxs.cmake) - Подсветка синтаксиса CMake
- **CMake Tools** (ms-vscode.cmake-tools) - Интеграция с CMake (опционально)
- **Qt tools** (tonka3000.qtvsctools) - Поддержка Qt (опционально)

---

**Версия**: 0.3
**Дата создания**: 2025-01-27
**Обновлено**: 2025-01-07 - Упрощены инструкции для Windows, добавлены быстрые команды
