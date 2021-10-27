cmake_minimum_required(VERSION 3.4.1)

option(EnableLOG "Enable QUT Logs" ON)
if(EnableLOG)
    add_definitions(-DEnableLOG)
endif()

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp)

find_library(log-lib, log)

add_subdirectory(${SOURCE_DIR}/external/libcJSON)
add_subdirectory(${SOURCE_DIR}/external/fastunwind)

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
      ${SOURCE_DIR}/common/ProfileRecord.cpp
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
        ${SOURCE_DIR}/memory/MemoryBufferQueue.cpp
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

################################### MemGuard ####################################
set(TARGET memguard_base)

add_library(
  ${TARGET}
  STATIC
  ${SOURCE_DIR}/memguard/port/Hook.cpp
  ${SOURCE_DIR}/memguard/port/Log.cpp
  ${SOURCE_DIR}/memguard/port/Memory.cpp
  ${SOURCE_DIR}/memguard/port/Mutex.cpp
  ${SOURCE_DIR}/memguard/port/Paths.cpp
  ${SOURCE_DIR}/memguard/port/Random.cpp
  ${SOURCE_DIR}/memguard/port/Unwind.cpp
  ${SOURCE_DIR}/memguard/port/FdSanWrapper.cpp
  ${SOURCE_DIR}/memguard/util/SignalHandler.cpp
  ${SOURCE_DIR}/memguard/util/Interception.cpp
  ${SOURCE_DIR}/memguard/util/PagePool.cpp
  ${SOURCE_DIR}/memguard/util/Allocation.cpp
  ${SOURCE_DIR}/memguard/util/Thread.cpp
  ${SOURCE_DIR}/memguard/util/Issue.cpp
  ${SOURCE_DIR}/memguard/MemGuard.cpp
)

target_include_directories(
  ${TARGET}
  PRIVATE ${SOURCE_DIR}/memguard
  PUBLIC ${SOURCE_DIR}
  PUBLIC ${EXT_DEP}/include
  PUBLIC ${EXT_DEP}/include/backtrace
  PUBLIC ${EXT_DEP}/include/backtrace/common
)

target_compile_options(
  ${TARGET}
  PRIVATE -Wall -Wextra -Werror -Wno-unused-function
  PRIVATE $<$<COMPILE_LANGUAGE:C>:-std=c17>
  PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>
  PUBLIC -fvisibility=hidden -fno-exceptions -fno-rtti -fdata-sections -ffunction-sections
  PUBLIC -DMEMGUARD_LOG_LEVEL=4
)

target_link_libraries(
  ${TARGET}
  PUBLIC -Wl,--gc-sections
  PRIVATE ${log-lib}
  PUBLIC matrix-hookcommon
  PUBLIC wechatbacktrace
  PUBLIC ${EXT_DEP}/lib/${ANDROID_ABI}/libunwindstack.a
  PUBLIC fastunwind
)


set(TARGET matrix-memguard)

add_library(
  ${TARGET}
  SHARED
  ${SOURCE_DIR}/memguard/jni/com_tencent_mm_tools_memguard_MemGuard.cpp
  ${SOURCE_DIR}/memguard/jni/com_tencent_mm_tools_memguard_MemGuard_00024Options.cpp
  ${SOURCE_DIR}/memguard/jni/JNIAux.cpp
  ${SOURCE_DIR}/memguard/jni/C2Java.cpp
)

target_include_directories(
  ${TARGET}
  PRIVATE ${SOURCE_DIR}/memguard
  PRIVATE ${EXT_DEP}/include
  PRIVATE ${EXT_DEP}/include/backtrace
  PRIVATE ${EXT_DEP}/include/backtrace/common
)

target_compile_options(
  ${TARGET}
  PRIVATE -Wall -Wextra -Werror -Wno-unused-function
  PRIVATE -fvisibility=hidden -fno-exceptions -fno-rtti -fdata-sections -ffunction-sections
  PRIVATE $<$<COMPILE_LANGUAGE:C>:-std=c17>
  PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>
)

target_link_libraries(
  ${TARGET}
  PRIVATE -Wl,--gc-sections
  PRIVATE -Wl,--version-script=${SOURCE_DIR}/memguard/memguard.map
  PRIVATE ${log-lib}
  PRIVATE matrix-hookcommon
  PRIVATE memguard_base
)
#################################################################################