cmake_minimum_required(VERSION 3.4.1)

option(EnableLOG "Enable Logs" ON)
if(EnableLOG)
    add_definitions(-DEnableLOG)
endif()

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp)

find_library(log-lib log)

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
)

target_link_libraries(
  ${TARGET}
  PUBLIC -Wl,--gc-sections
  PUBLIC ${EXT_DEP}/lib/${ANDROID_ABI}/libmatrix-hookcommon.so
  PUBLIC ${EXT_DEP}/lib/${ANDROID_ABI}/libwechatbacktrace.so
  PUBLIC ${EXT_DEP}/lib/${ANDROID_ABI}/libunwindstack.a
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
  PRIVATE ${EXT_DEP}/include/fastunwind
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
  PRIVATE memguard_base
)
#################################################################################