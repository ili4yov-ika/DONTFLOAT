#!/bin/bash
# Скрипт для запуска всех тестов DONTFLOAT (Linux/Mac)
# Использование: ./run_tests.sh [имя_теста]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/../build"

cd "$BUILD_DIR" || exit 1

if [ ! -d "Debug" ] && [ ! -d "build" ]; then
    echo "Ошибка: Директория build не найдена!"
    echo "Сначала соберите проект: cmake --build build"
    exit 1
fi

# Определяем директорию с исполняемыми файлами
if [ -d "Debug" ]; then
    TEST_DIR="Debug"
elif [ -d "build" ]; then
    TEST_DIR="build"
else
    TEST_DIR="."
fi

# Если указан конкретный тест, запускаем его
if [ -n "$1" ]; then
    TEST_EXE="$TEST_DIR/$1"
    if [ -f "$TEST_EXE" ] || [ -f "${TEST_EXE}.exe" ]; then
        echo "Запуск теста: $1"
        echo ""
        if [ -f "${TEST_EXE}.exe" ]; then
            ./"${TEST_EXE}.exe"
        else
            ./"$TEST_EXE"
        fi
        exit $?
    else
        echo "Ошибка: Тест $1 не найден в $TEST_DIR/"
        echo "Доступные тесты:"
        find "$TEST_DIR" -name "*_test" -o -name "*_test.exe" 2>/dev/null
        exit 1
    fi
fi

# Запускаем все тесты через CTest
echo "Запуск всех тестов через CTest..."
echo ""
ctest --output-on-failure

if [ $? -eq 0 ]; then
    echo ""
    echo "========================================"
    echo "Все тесты успешно пройдены!"
    echo "========================================"
else
    echo ""
    echo "========================================"
    echo "Некоторые тесты не прошли!"
    echo "========================================"
fi

