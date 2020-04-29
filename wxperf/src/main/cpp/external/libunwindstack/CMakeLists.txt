CMAKE_MINIMUM_REQUIRED(VERSION 3.4.1)

PROJECT(LibUnwindStack C CXX ASM)

SET(TARGET unwindstack)

SET(
        ${TARGET}_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/DwarfError.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ArmExidx.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DwarfCfa.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DwarfEhFrameWithHdr.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DwarfMemory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DwarfOp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DwarfSection.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Elf.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ElfInterface.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ElfInterfaceArm.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ElfInterfaceArm64.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/JitDebug.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Log.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/MapInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Maps.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Memory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Regs.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/RegsArm.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/RegsArm64.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/RegsX86.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/RegsX86_64.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/RegsMips.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/RegsMips64.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Unwinder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Symbols.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Global.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/MemFnWrapper.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/EnhanceDlsym.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/../libwechatbacktrace/FastUnwinder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libwechatbacktrace/FallbackUnwinder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libwechatbacktrace/MapsControll.cpp
#        ${CMAKE_CURRENT_SOURCE_DIR}/../libwechatbacktrace/WeChatElfInterfaceArm64.cpp
)

IF(${ANDROID_ABI} STREQUAL x86)
  SET(
          ${TARGET}_SOURCE_FILES
          ${${TARGET}_SOURCE_FILES}
          ${CMAKE_CURRENT_SOURCE_DIR}/AsmGetRegsX86.S
  )
ELSEIF(${ANDROID_ABI} STREQUAL x86_64)
  SET(
          ${TARGET}_SOURCE_FILES
          ${${TARGET}_SOURCE_FILES}
          ${CMAKE_CURRENT_SOURCE_DIR}/AsmGetRegsX86_64.S
  )
ELSEIF(${ANDROID_ABI} STREQUAL mips)
  SET(
          ${TARGET}_SOURCE_FILES
          ${${TARGET}_SOURCE_FILES}
          ${CMAKE_CURRENT_SOURCE_DIR}/AsmGetRegsMips.S
  )
ELSEIF(${ANDROID_ABI} STREQUAL mips64)
  SET(
          ${TARGET}_SOURCE_FILES
          ${${TARGET}_SOURCE_FILES}
          ${CMAKE_CURRENT_SOURCE_DIR}/AsmGetRegsMips64.S
  )
ENDIF()

ADD_SUBDIRECTORY(deps/android-base)
ADD_SUBDIRECTORY(deps/liblzma)
ADD_SUBDIRECTORY(deps/sys_compat)
ADD_SUBDIRECTORY(deps/libprocinfo)
ADD_SUBDIRECTORY(deps/demangle)

ADD_LIBRARY(
        ${TARGET}
        STATIC
        ${${TARGET}_SOURCE_FILES}
)

TARGET_COMPILE_OPTIONS(
        ${TARGET}
        PRIVATE -fvisibility=default
        PRIVATE -fdata-sections
        PRIVATE -ffunction-sections
        PRIVATE -Wexit-time-destructors
        PRIVATE -Wno-unused-value
        PRIVATE -DNO_LIBDEXFILE_SUPPORT
        PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>
)

TARGET_INCLUDE_DIRECTORIES(
        ${TARGET}
        PRIVATE .
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../libwechatbacktrace/include
)

FIND_LIBRARY(log-lib log)

TARGET_LINK_LIBRARIES(
        ${TARGET}
        PRIVATE ${log-lib}
        PRIVATE android-base
        PRIVATE lzma
        PRIVATE sys_compat
        PRIVATE procinfo
        PRIVATE demangle
        PRIVATE -Wl,--gc-sections
#        PRIVATE -Wl,--wrap=malloc
#        PRIVATE -Wl,--wrap=calloc
#        PRIVATE -Wl,--wrap=realloc
#        PRIVATE -Wl,--wrap=free
#        PRIVATE -Wl,--wrap=_Znwj
#        PRIVATE -Wl,--wrap=_ZnwjRKSt9nothrow_t
#        PRIVATE -Wl,--wrap=_Znaj
#        PRIVATE -Wl,--wrap=_ZnajRKSt9nothrow_t
#        PRIVATE -Wl,--wrap=_ZdlPv
#        PRIVATE -Wl,--wrap=_ZdlPvRKSt9nothrow_t
#        PRIVATE -Wl,--wrap=_ZdlPvj
#        PRIVATE -Wl,--wrap=_ZdaPv
#        PRIVATE -Wl,--wrap=_ZdaPvRKSt9nothrow_t
#        PRIVATE -Wl,--wrap=_ZdaPvj
)