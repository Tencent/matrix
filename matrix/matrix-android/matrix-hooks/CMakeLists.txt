cmake_minimum_required(VERSION 3.4.1)

option(EnableLOG "Enable QUT Logs" ON)
if(EnableLOG)
    add_definitions(-DEnableLOG)
endif()

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp)

find_library(log-lib, log)

add_subdirectory(${SOURCE_DIR}/external/libcJSON)

################################## Common Part ##################################
set(TARGET matrix-hookcommon)

add_library(
      ${TARGET}
      SHARED
      ${SOURCE_DIR}/common/JNICommon.cpp
      ${SOURCE_DIR}/common/HookCommon.cpp
      ${SOURCE_DIR}/common/ReentrantPrevention.cpp
      ${SOURCE_DIR}/common/Log.cpp
      ${SOURCE_DIR}/common/Maps.cpp
      ${SOURCE_DIR}/common/SoLoadMonitor.cpp
)

target_include_directories(
      ${TARGET}
      PUBLIC ${SOURCE_DIR}
      PUBLIC ${EXT_DEP}/include
      PUBLIC ${EXT_DEP}/include/backtrace
      PUBLIC ${EXT_DEP}/include/backtrace/common
)

target_compile_options(
      ${TARGET}
      PUBLIC $<$<COMPILE_LANGUAGE:C>:-std=c99>
      PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++17 -fno-exceptions -fno-rtti>
      PUBLIC -fdata-sections -ffunction-sections -fvisibility=hidden -fstack-protector
)

target_link_libraries(
      ${TARGET}
      PUBLIC -Wl,--gc-sections
      PUBLIC cJSON
      PUBLIC ${log-lib}
      PUBLIC ${EXT_DEP}/lib/${ANDROID_ABI}/libwechatbacktrace.so
      PRIVATE -Wl,--whole-archive ${EXT_DEP}/lib/${ANDROID_ABI}/libxhook.a -Wl,--no-whole-archive
      PRIVATE -Wl,--whole-archive ${EXT_DEP}/lib/${ANDROID_ABI}/libsemi_dlfcn.a -Wl,--no-whole-archive
      PRIVATE -Wl,--version-script=${SOURCE_DIR}/common/common.ver
)
#################################################################################

################################## Memory Hook ##################################
set(TARGET matrix-memoryhook)

add_library(
        ${TARGET}
        SHARED
        ${SOURCE_DIR}/memory/GCSemiSpaceTrimmer.cpp
        ${SOURCE_DIR}/memory/GCSemiSpaceTrimmerJNI.cpp
        ${SOURCE_DIR}/memory/MemoryHookJNI.cpp
        ${SOURCE_DIR}/memory/MemoryHook.cpp
        ${SOURCE_DIR}/memory/MemoryHookFunctions.cpp
        ${SOURCE_DIR}/memory/WVPreAllocTrimmer.cpp
        ${SOURCE_DIR}/memory/WVPreAllocHookJNI.cpp
)

target_link_libraries(
      ${TARGET}
      PRIVATE matrix-hookcommon
      PRIVATE -Wl,--version-script=${SOURCE_DIR}/memory/memory.ver
)
#################################################################################

################################# Pthread Hook ##################################
set(TARGET matrix-pthreadhook)

add_library(
      ${TARGET}
      SHARED
      ${SOURCE_DIR}/pthread/ThreadTrace.cpp
      ${SOURCE_DIR}/pthread/ThreadStackShink.cpp
      ${SOURCE_DIR}/pthread/PthreadHook.cpp
      ${SOURCE_DIR}/pthread/PthreadHookJNI.cpp
)

target_link_libraries(
      ${TARGET}
      PRIVATE matrix-hookcommon
      PRIVATE -Wl,--version-script=${SOURCE_DIR}/pthread/pthread.ver
)
#################################################################################
