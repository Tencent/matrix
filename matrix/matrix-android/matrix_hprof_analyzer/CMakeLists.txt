cmake_minimum_required(VERSION 3.10.2)
project(hprof_analyzer)

set(CMAKE_CXX_STANDARD 17)

option(BUILD_TESTING "Enable testing." OFF)

include_directories(include)

add_subdirectory(lib)

if (BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif ()