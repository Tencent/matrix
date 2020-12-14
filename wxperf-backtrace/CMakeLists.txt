# Sets the minimum version of CMake required to build your native library.
# This ensures that a certain set of CMake features is available to
# your build.

CMAKE_MINIMUM_REQUIRED(VERSION 3.4.1)

# Specifies a library name, specifies whether the library is STATIC or
# SHARED, and provides relative paths to the source code. You can
# define multiple libraries by adding multiple add.library() commands,
# and CMake builds them for you. When you build your app, Gradle
# automatically packages shared libraries with your APK.

SET(TARGET wechatbacktrace)

SET(SOURCE_DIR src/main/cpp)

OPTION(EnableLOG "Enable QUT Logs" ON)
OPTION(QUT_STATISTIC_ENABLE "Enable QUT Statistics" OFF)

IF(EnableLOG)
    ADD_DEFINITIONS(-DEnableLOG)
ENDIF()
IF(QUT_STATISTIC_ENABLE)
    ADD_DEFINITIONS(-DQUT_STATISTIC_ENABLE)
ENDIF()

SET(
        SOURCE_FILES
        ${SOURCE_DIR}/common/Utils.cpp
        ${SOURCE_DIR}/common/PthreadExt.cpp
#        ${SOURCE_DIR}/dlsym/EnhanceDlsym.cpp

        ${SOURCE_DIR}/libwechatbacktrace/QutStatistics.cpp
        ${SOURCE_DIR}/libwechatbacktrace/ExidxDecoder.cpp
        ${SOURCE_DIR}/libwechatbacktrace/DwarfCfa.cpp
        ${SOURCE_DIR}/libwechatbacktrace/DwarfSectionDecoder.cpp
        ${SOURCE_DIR}/libwechatbacktrace/DwarfEhFrameWithHdrDecoder.cpp
        ${SOURCE_DIR}/libwechatbacktrace/DwarfOp.cpp
        ${SOURCE_DIR}/libwechatbacktrace/QuickenTable.cpp
        #        ${SOURCE_DIR}/QuickenTableReverse.cpp
        ${SOURCE_DIR}/libwechatbacktrace/QuickenTableManager.cpp
        ${SOURCE_DIR}/libwechatbacktrace/QuickenMaps.cpp
        ${SOURCE_DIR}/libwechatbacktrace/QuickenTableGenerator.cpp
        ${SOURCE_DIR}/libwechatbacktrace/QuickenInterface.cpp
        ${SOURCE_DIR}/libwechatbacktrace/QuickenUnwinder.cpp
        ${SOURCE_DIR}/libwechatbacktrace/QuickenJNI.cpp
        ${SOURCE_DIR}/libwechatbacktrace/LocalMaps.cpp
        ${SOURCE_DIR}/libwechatbacktrace/FpUnwinder.cpp
        ${SOURCE_DIR}/libwechatbacktrace/Backtrace.cpp
)

ADD_LIBRARY( # Specifies the name of the library.
        ${TARGET}

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        ${SOURCE_FILES}
        )

ADD_SUBDIRECTORY(
        ${SOURCE_DIR}/external/libunwindstack
)

INCLUDE_DIRECTORIES(
        ${SOURCE_DIR}/external/libunwindstack
)

TARGET_INCLUDE_DIRECTORIES(
        ${TARGET}
        PUBLIC ${SOURCE_DIR}/common
        PUBLIC ${SOURCE_DIR}/libwechatbacktrace/include
        PUBLIC ${SOURCE_DIR}/external/libunwindstack/include
)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

FIND_LIBRARY( # Sets the name of the path variable.
        log-lib
        log
)

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

TARGET_LINK_LIBRARIES( # Specifies the target library.
        ${TARGET}
        # Links the target library to the log library
        # included in the NDK.
        PRIVATE ${log-lib}
        PRIVATE unwindstack
        PRIVATE android-base
        PRIVATE lzma
        PRIVATE sys_compat
        PRIVATE procinfo
        PRIVATE demangle
        PRIVATE -Wl,--gc-sections
)

TARGET_COMPILE_OPTIONS(
        ${TARGET}
        PRIVATE $<$<COMPILE_LANGUAGE:C>:-std=c99>
        PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++17 -fno-exceptions -frtti>
)
