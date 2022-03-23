cmake_minimum_required(VERSION 3.10.2)
project(matrix_hprof_analyzer)

set(CMAKE_CXX_STANDARD 17)

option(TEST_MODE "Enable test mode." OFF)
if (TEST_MODE)
    add_definitions(-D__test_mode__)
endif ()


include_directories(include)

add_subdirectory(lib)

if (TEST_MODE)
    enable_testing()
    add_subdirectory(test)
endif ()