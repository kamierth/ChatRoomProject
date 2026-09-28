add_executable(chat_unit_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_chat_room.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_chat_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_client_connection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_logger.cpp
)

target_link_libraries(chat_unit_tests PRIVATE
    chat_server_lib
)

target_include_directories(chat_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/support
)

target_compile_options(chat_unit_tests PRIVATE
    ${CHAT_WARNING_OPTIONS}
)

add_test(
    NAME chat_unit_tests
    COMMAND chat_unit_tests
)

add_executable(chat_integration_tests
    integration/test_chat_server.cpp
)

target_link_libraries(chat_integration_tests PRIVATE
    chat_server_lib
)

target_compile_options(chat_integration_tests PRIVATE
    ${CHAT_WARNING_OPTIONS}
)

add_test(
    NAME chat_integration_tests
    COMMAND chat_integration_tests
)

set_tests_properties(chat_integration_tests PROPERTIES
    TIMEOUT 15
)
