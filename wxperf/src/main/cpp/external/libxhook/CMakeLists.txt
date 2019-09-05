cmake_minimum_required(VERSION 3.4.1)
project(libxhook C)

set(xhook_source xh_core.c xh_elf.c xh_jni.c xh_log.c xh_util.c xh_version.c xhook.c)

add_library(xhook STATIC ${xhook_source})

find_library(
        log-lib
        log
)

target_include_directories(
        xhook
        PUBLIC .
        PRIVATE ${EXT_DEP}/include
)

target_link_libraries(
        xhook
        ${log-lib}
        ${EXT_DEP}/lib/${ANDROID_ABI}/liblogger.a
)

