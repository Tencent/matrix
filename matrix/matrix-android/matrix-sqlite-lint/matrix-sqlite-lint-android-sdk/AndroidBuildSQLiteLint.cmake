#
# This will be included when you pass a argument platform=android to CMake
# See AndroidSdk/build.gradle externalNativeBuild#cmake
# Created by liyongjie on 2017/6/22
#

#add_subdirectory(../matrix-android-commons/src/main/cpp/libxhook-deprecated ${CMAKE_CURRENT_BINARY_DIR}/libxhook)

# collects jni srcs
aux_source_directory(${PROJECT_SOURCE_DIR}/${ANDROID_SDK_DIR_NAME}/src/full/cpp/ SQLiteLint_Jni_SRCS)

# target lib

add_library(SqliteLint-lib
             SHARED
             ${SQLiteLint_Jni_SRCS}
             ${SQLiteLint_Library_SRCS}
            )

TARGET_INCLUDE_DIRECTORIES(SqliteLint-lib PRIVATE ${EXT_DEP}/include)

# link the log
find_library( log-lib log )
target_link_libraries(SqliteLint-lib ${log-lib} ${EXT_DEP}/lib/${ANDROID_ABI}/libxhook.a)