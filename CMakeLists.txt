cmake_minimum_required(VERSION 3.22)

project(chat_project LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CHAT_WARNING_OPTIONS
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
)

add_library(chat_common_lib
    ${CMAKE_SOURCE_DIR}/src/common/logger.cpp
)

target_include_directories(chat_common_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_compile_options(chat_common_lib PRIVATE ${CHAT_WARNING_OPTIONS})

add_library(chat_server_lib
    ${CMAKE_SOURCE_DIR}/src/server/chat_room.cpp
    ${CMAKE_SOURCE_DIR}/src/server/chat_service.cpp
    ${CMAKE_SOURCE_DIR}/src/server/client_connection.cpp
    ${CMAKE_SOURCE_DIR}/src/server/epoll_server.cpp
)

target_include_directories(chat_server_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_compile_options(chat_server_lib PRIVATE ${CHAT_WARNING_OPTIONS})

target_link_libraries(chat_server_lib PRIVATE chat_common_lib)

add_executable(server ${CMAKE_SOURCE_DIR}/src/server/main.cpp)

target_link_libraries(server PRIVATE chat_server_lib)

target_compile_options(server PRIVATE ${CHAT_WARNING_OPTIONS})

add_library(chat_client_lib
    ${CMAKE_SOURCE_DIR}/src/client/chat_client.cpp
)

target_include_directories(chat_client_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_compile_options(chat_client_lib PRIVATE ${CHAT_WARNING_OPTIONS})

target_link_libraries(chat_client_lib PRIVATE chat_common_lib)

add_executable(client ${CMAKE_SOURCE_DIR}/src/client/main.cpp)

target_link_libraries(client PRIVATE chat_client_lib)

target_compile_options(client PRIVATE ${CHAT_WARNING_OPTIONS})

include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
