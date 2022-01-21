cmake_minimum_required(VERSION 3.10.2)

project("matrix-memorydump")

add_library(
        matrix-memorydump
        SHARED
        src/main/cpp/mem_dump.cpp
        src/main/cpp/dlfcn/self_dlfcn.cpp)

find_library(
        log-lib
        log)

target_include_directories(
        matrix-memorydump
        PRIVATE ${EXT_DEP}/include
        PRIVATE ${EXT_DEP}/include/backtrace/common)

target_link_libraries(
        matrix-memorydump
        ${log-lib}
        ${EXT_DEP}/lib/${ANDROID_ABI}/libenhance_dlsym.a
        ${EXT_DEP}/lib/${ANDROID_ABI}/libsemi_dlfcn.a)

if(${ANDROID_ABI} MATCHES "armeabi-v7a" OR ${ANDROID_ABI} MATCHES "arm64-v8a")
    target_link_libraries(
            matrix-memorydump
            ${EXT_DEP}/lib/${ANDROID_ABI}/libwechatbacktrace.so
    )
endif()