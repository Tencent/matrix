cmake_minimum_required(VERSION 3.10.2)
project(matrix_hprof_analyzer)

set(CMAKE_CXX_STANDARD 17)

add_library(test_tools STATIC buffer_generator.cpp)
target_include_directories(test_tools INTERFACE include)