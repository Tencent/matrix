#ifndef _LIBWECHATBACKTRACE_TYPES_H
#define _LIBWECHATBACKTRACE_TYPES_H

namespace wechat_backtrace {

    typedef uintptr_t uptr;

    constexpr auto FILE_SEPERATOR = "/";

#ifdef __arm__
    typedef uint32_t addr_t;
#else
    typedef uint64_t addr_t;
#endif

}  // namespace wechat_backtrace


#define QUT_ARCH_ARM 0x1
#define QUT_ARCH_ARM64 0x2

#ifdef __arm__
    #define QUT_VERSION 0x1
    #define CURRENT_ARCH_ENUM QUT_ARCH_ARM

    #define QUT_TBL_ROW_SIZE 3  // 4 - 1
#else
    #define QUT_VERSION 1
    #define CURRENT_ARCH_ENUM QUT_ARCH_ARM64

    #define QUT_TBL_ROW_SIZE 7  // 8 - 1
#endif

#endif  // _LIBWECHATBACKTRACE_TYPES_H
