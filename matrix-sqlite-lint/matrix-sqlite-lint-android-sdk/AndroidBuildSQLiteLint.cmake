#
# This will be included when you pass a argument platform=android to CMake
# See AndroidSdk/build.gradle externalNativeBuild#cmake
# Created by liyongjie on 2017/6/22
#

# collects jni srcs
aux_source_directory(${PROJECT_SOURCE_DIR}/${ANDROID_SDK_DIR_NAME}/src/full/cpp/ SQLiteLint_Jni_SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/../matrix-android-commons/src/main/cpp/elf_hook Elf_Hook_SRCS)

include_directories(${PROJECT_SOURCE_DIR}/../matrix-android-commons/src/main/cpp/elf_hook)

# target lib

add_library(SqliteLint-lib
             SHARED
             ${Elf_Hook_SRCS}
             ${SQLiteLint_Jni_SRCS}
             ${SQLiteLint_Library_SRCS}
            )

# link the log
find_library( log-lib log )
target_link_libraries(SqliteLint-lib ${log-lib})