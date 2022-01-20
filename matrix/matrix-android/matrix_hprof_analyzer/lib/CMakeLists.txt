cmake_minimum_required(VERSION 3.10.2)
project(hprof_analyzer)

set(CMAKE_CXX_STANDARD 17)

add_library(hprof_analyzer_reader STATIC reader/reader.cpp)
target_include_directories(hprof_analyzer_reader INTERFACE reader/include)

add_library(hprof_analyzer_heap STATIC heap/heap.cpp heap/primitive.cpp)
target_include_directories(hprof_analyzer_heap INTERFACE heap/include)
target_link_libraries(hprof_analyzer_heap hprof_analyzer_reader)

add_library(hprof_analyzer_parser STATIC parser/parser.cpp parser/engine.cpp)
target_include_directories(hprof_analyzer_parser INTERFACE parser/include)
target_link_libraries(hprof_analyzer_parser hprof_analyzer_heap)


if (BUILD_TESTING)
    add_library(hprof_analyzer_parser_testable STATIC parser/parser.cpp parser/engine.cpp)
    target_include_directories(hprof_analyzer_parser_testable INTERFACE parser/include parser/internal)
    target_link_libraries(hprof_analyzer_parser_testable hprof_analyzer_heap)
endif ()

add_library(hprof_analyzer_analyzer STATIC analyzer/analyzer.cpp)
target_include_directories(hprof_analyzer_analyzer INTERFACE analyzer/include)
target_link_libraries(hprof_analyzer_analyzer hprof_analyzer_heap)

add_library(hprof_analyzer SHARED main/analyzer.cpp main/chain.cpp main/heap.cpp)
target_include_directories(hprof_analyzer INTERFACE main/include ../include)
target_link_libraries(hprof_analyzer
        PRIVATE hprof_analyzer_heap
        PRIVATE hprof_analyzer_parser
        PRIVATE hprof_analyzer_analyzer)

if (BUILD_TESTING)
    add_library(hprof_analyzer_testable SHARED main/analyzer.cpp main/chain.cpp main/heap.cpp)
    target_include_directories(hprof_analyzer_testable INTERFACE main/include main/internal ../include)
    target_link_libraries(hprof_analyzer_testable
            PRIVATE hprof_analyzer_heap
            PRIVATE hprof_analyzer_parser
            PRIVATE hprof_analyzer_analyzer)
endif ()