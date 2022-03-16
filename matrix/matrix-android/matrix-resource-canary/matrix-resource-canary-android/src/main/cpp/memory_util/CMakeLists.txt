cmake_minimum_required(VERSION 3.10.2)
project(matrix-resource-canary-android)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(../../../../../../matrix_hprof_analyzer matrix_hprof_analyzer.out)

add_library(matrix_mem_util SHARED
        excludes/excludes.cpp symbol/dlsym/dlsym.cpp symbol/symbol.cpp log.cpp memory_util.cpp)

find_library(
        log-lib
        log)

target_include_directories(
        matrix_mem_util
        PRIVATE ${EXT_DEP}/include
        PRIVATE ${EXT_DEP}/include/backtrace/common)

target_link_libraries(
        matrix_mem_util
        hprof_analyzer
        ${log-lib}
        ${EXT_DEP}/lib/${ANDROID_ABI}/libenhance_dlsym.a
        ${EXT_DEP}/lib/${ANDROID_ABI}/libsemi_dlfcn.a)

if(${ANDROID_ABI} MATCHES "armeabi-v7a" OR ${ANDROID_ABI} MATCHES "arm64-v8a")
    target_link_libraries(
            matrix_mem_util
            ${EXT_DEP}/lib/${ANDROID_ABI}/libwechatbacktrace.so
    )
endif()


