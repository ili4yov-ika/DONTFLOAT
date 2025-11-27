@echo off
REM Скрипт для запуска всех тестов DONTFLOAT
REM Использование: run_tests.bat [имя_теста]

setlocal

REM Переходим в директорию build
cd /d "%~dp0\..\build"

if not exist "Debug" (
    echo Ошибка: Директория build\Debug не найдена!
    echo Сначала соберите проект: cmake --build build
    exit /b 1
)

REM Если указан конкретный тест, запускаем его
if "%1" neq "" (
    if exist "Debug\%1.exe" (
        echo Запуск теста: %1
        echo.
        "Debug\%1.exe"
        exit /b %ERRORLEVEL%
    ) else (
        echo Ошибка: Тест %1.exe не найден в build\Debug\
        echo Доступные тесты:
        dir /b Debug\*_test.exe 2>nul
        exit /b 1
    )
)

REM Запускаем все тесты через CTest
echo Запуск всех тестов через CTest...
echo.
ctest --output-on-failure

if %ERRORLEVEL% equ 0 (
    echo.
    echo ========================================
    echo Все тесты успешно пройдены!
    echo ========================================
) else (
    echo.
    echo ========================================
    echo Некоторые тесты не прошли!
    echo ========================================
)

endlocal

