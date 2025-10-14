cmake_minimum_required(VERSION 3.22)
project(socket_posix)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB UMODEM_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../drivers/*.c"
)

add_library(umodem STATIC ${UMODEM_SOURCES})

target_include_directories(umodem PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../include"
)

add_executable(socket_posix)

target_sources(socket_posix PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/socket_posix.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../posix_hal.cpp"
)

target_include_directories(socket_posix PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../include"
)

target_link_libraries(socket_posix PRIVATE
    umodem
)