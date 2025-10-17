cmake_minimum_required(VERSION 3.22)
project(mqtt_subscriber_posix)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB UMODEM_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../drivers/quectel_m65/*.c"
)

add_library(umodem STATIC ${UMODEM_SOURCES})

target_include_directories(umodem PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../include"
)

add_executable(mqtt_subscriber_posix)

target_sources(mqtt_subscriber_posix PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/mqtt_subscriber_posix.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../posix_hal.cpp"
)

target_include_directories(mqtt_subscriber_posix PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../include"
)

target_link_libraries(mqtt_subscriber_posix PRIVATE
    umodem
)
