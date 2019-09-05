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
        ${SOURCE_DIR}/memory/lock.cpp
        ${SOURCE_DIR}/memory/StackTrace.cpp
        ${SOURCE_DIR}/memory/utils.cpp
)

add_library( # Specifies the name of the library.
        wxperf

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        ${SOURCE_FILES}
        #            src/main/cpp/fd/FDInfoReader.cpp
        )

add_subdirectory(${SOURCE_DIR}/external/libxhook)
add_subdirectory(${SOURCE_DIR}/external/libunwindstack)

target_include_directories(
        wxperf
        PRIVATE ${SOURCE_DIR}/include
        PRIVATE ${SOURCE_DIR}/external/libxhook
        PRIVATE ${SOURCE_DIR}/external/libunwindstack/include
        PRIVATE ${EXT_DEP}/include
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
        wxperf
        xhook
        unwindstack
        # Links the target library to the log library
        # included in the NDK.
        ${log-lib}
        ${EXT_DEP}/lib/${ANDROID_ABI}/liblogger.a
        )