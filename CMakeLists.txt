cmake_minimum_required(VERSION 3.22)

project(chat_project LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# file(GLOB SERVER_SRC ${CMAKE_SOURCE_DIR}/src/server/*.cpp)
set(SERVER_SRC
    ${CMAKE_SOURCE_DIR}/src/server/chat_room.cpp
    ${CMAKE_SOURCE_DIR}/src/server/chat_service.cpp
    ${CMAKE_SOURCE_DIR}/src/server/client_connection.cpp
    ${CMAKE_SOURCE_DIR}/src/server/epoll_server.cpp
    ${CMAKE_SOURCE_DIR}/src/server/main.cpp
)

add_executable(server ${SERVER_SRC})

target_include_directories(server PRIVATE ${CMAKE_SOURCE_DIR}/include)
