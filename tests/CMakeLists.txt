enable_testing()

# Добавляем тестовый исполняемый файл
add_executable(DONTFLOAT_tests
)

# Подключаем нужные Qt-модули
target_link_libraries(DONTFLOAT_tests
    PRIVATE
        Qt6::Test
	dontfloat_core
)

# Указываем директорию includes для тестов
target_include_directories(DONTFLOAT_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)

# Регистрируем тесты
add_test(NAME DONTFLOAT_tests COMMAND DONTFLOAT_tests)
