cmake_minimum_required(VERSION 3.15)

project(MiniRedisCpp
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(miniredis_core
    src/aof_log.cpp
    src/command_handler.cpp
    src/command_parser.cpp
    src/key_value_store.cpp
    src/resp_response.cpp
    src/tcp_server.cpp
)

target_include_directories(miniredis_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

if (WIN32)
    target_link_libraries(miniredis_core PUBLIC ws2_32)
endif()

target_link_libraries(miniredis_core PUBLIC Threads::Threads)

add_executable(miniredis
    src/main.cpp
)

target_link_libraries(miniredis PRIVATE miniredis_core)

enable_testing()

add_executable(miniredis_tests
    tests/test_aof_log.cpp
    tests/test_command_parser.cpp
    tests/test_command_handler.cpp
    tests/test_key_value_store.cpp
)

target_link_libraries(miniredis_tests PRIVATE miniredis_core)

add_test(NAME miniredis_tests COMMAND miniredis_tests)
