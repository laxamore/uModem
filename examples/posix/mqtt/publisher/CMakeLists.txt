cmake_minimum_required(VERSION 3.22)
project(mqtt_publisher_posix)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB UMODEM_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../drivers/*.c"
)

add_library(umodem STATIC ${UMODEM_SOURCES})

target_include_directories(umodem PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../"
)

add_executable(mqtt_publisher_posix)

target_sources(mqtt_publisher_posix PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/mqtt_publisher_posix.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../posix_hal.cpp"
)

target_include_directories(mqtt_publisher_posix PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../"
)

target_link_libraries(mqtt_publisher_posix PRIVATE
    umodem
)
