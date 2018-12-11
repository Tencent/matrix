#
# Cross-Platform base cmake configure
# Include a certain platform's file: **.cmake
# eg. see AndroidBuildSQLiteLint.cmake
#
# Created by liyongjie on 2017/6/21
#

cmake_minimum_required(VERSION 3.4.1)

# We called it SQLiteLint which is to find improper usages of SQLite3
project(SQLiteLint)

set(ANDROID_SDK_DIR_NAME matrix-sqlite-lint-android-sdk)

# Collect src to SQLiteLint_Library_SRCS
aux_source_directory(src/core/ SRC_DIR_CORE)
aux_source_directory(src/checker/ SRC_DIR_CHECKER)
aux_source_directory(src/checker/ SRC_DIR_CHECKER)
aux_source_directory(src/checker/eqp SRC_DIR_EQP_CHECKER)
aux_source_directory(src/lemon/ SRC_DIR_LEMON)
aux_source_directory(src/comm/ SRC_DIR_COMM)
aux_source_directory(${PROJECT_SOURCE_DIR}/../matrix-android-commons/src/main/cpp/md5 MD5_SRCS)

include_directories(${PROJECT_SOURCE_DIR}/../matrix-android-commons/src/main/cpp/md5)

set(SQLiteLint_Library_SRCS
    ${SRC_DIR_CORE}
    ${SRC_DIR_CHECKER}
    ${SRC_DIR_LEMON}
    ${SRC_DIR_COMM}
    ${SRC_DIR_EQP_CHECKER}
    ${MD5_SRCS}
    src/sqlite_lint.cc
    src/comm/log/logger.cc
)

include_directories(src)

# Pass a argument platform to determine which platform to build
# Then different platforms can share this same CMakeLists.txt and its configure
message("building platform:" ${platform})
if (${platform} STREQUAL "android")
   include(${ANDROID_SDK_DIR_NAME}/AndroidBuildSQLiteLint.cmake)
endif()
