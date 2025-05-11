# Поиск всех .ts-файлов
file(GLOB TS_FILES "*.ts")

# Генерация .qm файлов
foreach(TS_FILE ${TS_FILES})
    get_filename_component(TS_NAME ${TS_FILE} NAME_WE)
    set(QM_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TS_NAME}.qm")
    add_custom_command(OUTPUT ${QM_FILE}
        COMMAND ${Qt6_LRELEASE_EXECUTABLE} ${TS_FILE} -qm ${QM_FILE}
        DEPENDS ${TS_FILE}
        COMMENT "Generating translation: ${QM_FILE}"
        VERBATIM
    )
    list(APPEND QM_FILES ${QM_FILE})
endforeach()

# Цель для сборки всех .qm
add_custom_target(translations ALL DEPENDS ${QM_FILES})
