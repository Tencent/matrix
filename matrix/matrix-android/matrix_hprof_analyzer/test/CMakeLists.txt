cmake_minimum_required(VERSION 3.10.2)
project(hprof_analyzer)

set(CMAKE_CXX_STANDARD 17)

if (DEFINED ANDROID_NDK)
    add_subdirectory(${ANDROID_NDK}/sources/third_party/googletest googletest.out)
elseif (DEFINED GOOGLE_TEST_DIR)
    add_subdirectory(${GOOGLE_TEST_DIR} googletest.out)
    include_directories(${GOOGLE_TEST_DIR}/googletest/include ${GOOGLE_TEST_DIR}/googletest)
else ()
    message("Parameter GOOGLE_TEST_DIR is not set. Please set it to the path to the Google Test framework source directory.")
endif ()
include(GoogleTest)

add_subdirectory(tools)

add_subdirectory(analyzer)
add_subdirectory(heap)
add_subdirectory(parser)
add_subdirectory(reader)

add_subdirectory(main)