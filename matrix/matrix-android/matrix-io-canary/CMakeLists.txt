cmake_minimum_required(VERSION 3.4.1)

project(IOCanary)

#aux_source_directory(../matrix-android-commons/src/main/cpp/elf_hook ELF_HOOK)
#include_directories(../matrix-android-commons/src/main/cpp/elf_hook)

add_subdirectory(../matrix-android-commons/src/main/cpp/libxhook ${CMAKE_CURRENT_BINARY_DIR}/libxhook)

aux_source_directory(src/main/cpp/detector/ SRC_DIR_DETECTOR)
aux_source_directory(src/main/cpp/comm/ SRC_DIR_COMM)
aux_source_directory(src/main/cpp/core/ SRC_DIR_CORE)
aux_source_directory(${PROJECT_SOURCE_DIR}/../matrix-android-commons/src/main/cpp/md5 MD5_SRCS)

include_directories(${PROJECT_SOURCE_DIR}/../matrix-android-commons/src/main/cpp/md5)
include_directories(src/main/cpp)

add_library( io-canary
             SHARED
             ${ELF_HOOK}
             ${SRC_DIR_COMM}
             ${SRC_DIR_DETECTOR}
             ${SRC_DIR_CORE}
             ${MD5_SRCS}
             src/main/cpp/io_canary_jni.cc
             )

find_library( log-lib
              log )

target_link_libraries( io-canary
                       ${log-lib}
                       xhook)