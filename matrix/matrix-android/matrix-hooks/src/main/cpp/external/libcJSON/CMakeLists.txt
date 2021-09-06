cmake_minimum_required(VERSION 3.4.1)
project(libcJSON C)

add_library(cJSON STATIC cJSON.c)

find_library(
        log-lib
        log
)

target_include_directories(
        cJSON
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
        cJSON
        ${log-lib}
)