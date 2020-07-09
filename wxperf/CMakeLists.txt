# Sets the minimum version of CMake required to build your native library.
# This ensures that a certain set of CMake features is available to
# your build.

cmake_minimum_required(VERSION 3.4.1)

# Specifies a library name, specifies whether the library is STATIC or
# SHARED, and provides relative paths to the source code. You can
# define multiple libraries by adding multiple add.library() commands,
# and CMake builds them for you. When you build your app, Gradle
# automatically packages shared libraries with your APK.

set(TARGET wxperf)

set(SOURCE_DIR src/main/cpp)

set(
        SOURCE_FILES
        ${SOURCE_DIR}/fd/FDInfoReader.cpp
        ${SOURCE_DIR}/memory/MemoryHookJNI.cpp
        ${SOURCE_DIR}/memory/MemoryHook.cpp
        ${SOURCE_DIR}/memory/MemoryHookFunctions.cpp
        ${SOURCE_DIR}/common/Backtrace.cpp
        ${SOURCE_DIR}/common/StackTrace.cpp
        ${SOURCE_DIR}/common/Utils.cpp
        ${SOURCE_DIR}/common/JNICommon.cpp
        ${SOURCE_DIR}/common/HookCommon.cpp
        ${SOURCE_DIR}/common/PthreadExt.cpp
        ${SOURCE_DIR}/pthread/PthreadHook.cpp
        ${SOURCE_DIR}/pthread/PthreadHookJNI.cpp
        ${SOURCE_DIR}/dlsym/EnhanceDlsym.cpp
        ${SOURCE_DIR}/egl/EglHook.cpp
        ${SOURCE_DIR}/egl/EglHookJNI.cpp
)

set(
        TEST_SOURCE_FILES
        ${SOURCE_DIR}/test/UnwindTestJNI.cpp
        ${SOURCE_DIR}/test/UnwindTestCommon.cpp
        ${SOURCE_DIR}/test/UnwindTestCase_SelfSo.cpp
        ${SOURCE_DIR}/test/UnwindTestCase_ThroughJNI.cpp
        ${SOURCE_DIR}/test/UnwindTestCase_ThroughSystemSo.cpp
        ${SOURCE_DIR}/test/UnwindTestBenchmark.cpp
)


add_library( # Specifies the name of the library.
        ${TARGET}

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        ${SOURCE_FILES}
        #            src/main/cpp/fd/FDInfoReader.cpp

        # Test source files.
        ${TEST_SOURCE_FILES}
        )

add_subdirectory(${SOURCE_DIR}/external/libxhook)
add_subdirectory(${SOURCE_DIR}/external/libunwindstack)
add_subdirectory(${SOURCE_DIR}/external/libcJSON)

target_include_directories(
        ${TARGET}
        PRIVATE ${SOURCE_DIR}/common
        PRIVATE ${SOURCE_DIR}/external/libxhook
        PRIVATE ${SOURCE_DIR}/external/libunwindstack/include
        PRIVATE ${SOURCE_DIR}/external/libwechatbacktrace/include
        PRIVATE ${SOURCE_DIR}/external/libcJSON
)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
        log-lib

        # Specifies the name of the NDK library that
        # you want CMake to locate.
        log)

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
        ${TARGET}
        xhook
        unwindstack
        cJSON
        # Links the target library to the log library
        # included in the NDK.
        ${log-lib})

TARGET_COMPILE_OPTIONS(
        ${TARGET}
        PRIVATE $<$<COMPILE_LANGUAGE:C>:-std=c99>
        PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++17 -fno-exceptions -fno-rtti>
)