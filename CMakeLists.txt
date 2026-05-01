cmake_minimum_required(VERSION 3.10)

project(chat_project LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(server
    src/server.cpp
    src/chat_server.cpp
    src/client_session.cpp
    src/command_dispatcher.cpp
    src/utils.cpp
)

add_executable(client
    src/client.cpp
    src/utils.cpp
)

target_compile_options(server PRIVATE -Wall -Wextra)
target_compile_options(client PRIVATE -Wall -Wextra)

target_include_directories(server PRIVATE include)
target_include_directories(client PRIVATE include)

target_link_libraries(server PRIVATE Threads::Threads)
target_link_libraries(client PRIVATE Threads::Threads)
