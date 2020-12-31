#ifndef _LIBWECHATBACKTRACE_DEFINE_H
#define _LIBWECHATBACKTRACE_DEFINE_H

#include <memory>
#include <unwindstack/Elf.h>

#include "Predefined.h"

// *** Version ***
#define QUT_VERSION 0x1

#define MAX_FRAME_SHORT 16
#define MAX_FRAME_NORMAL 32
#define MAX_FRAME_LONG 64

#define QUT_ARCH_ARM 0x1
#define QUT_ARCH_ARM64 0x2

#ifdef __cplusplus__
#define QUT_EXTERN_C extern "C"
#define QUT_EXTERN_C_BLOCK extern "C" {
#define QUT_EXTERN_C_BLOCK_END }
};
#else
#define QUT_EXTERN_C
#define QUT_EXTERN_C_BLOCK
#define QUT_EXTERN_C_BLOCK_END
#endif

#ifdef __arm__
#define CURRENT_ARCH unwindstack::ArchEnum::ARCH_ARM
#define CURRENT_ARCH_ENUM QUT_ARCH_ARM

#define QUT_TBL_ROW_SIZE 3  // 4 - 1
#else
#define CURRENT_ARCH unwindstack::ArchEnum::ARCH_ARM64
#define CURRENT_ARCH_ENUM QUT_ARCH_ARM64

#define QUT_TBL_ROW_SIZE 7  // 8 - 1
#endif

#define BACKTRACE_INITIALIZER(MAX_FRAMES) \
    {MAX_FRAMES, 0, std::shared_ptr<wechat_backtrace::Frame>( \
    new wechat_backtrace::Frame[MAX_FRAMES], std::default_delete<wechat_backtrace::Frame[]>())}

namespace wechat_backtrace {

    typedef uintptr_t uptr;

    constexpr auto FILE_SEPERATOR = "/";

#ifdef __arm__
    typedef uint32_t addr_t;
#else
    typedef uint64_t addr_t;
#endif

    typedef unsigned long long int ullint_t;
    typedef long long int llint_t;

    typedef unsigned long int ulint_t;
    typedef long int lint_t;

    struct Frame {
        uptr pc;
        bool is_dex_pc;
    };

    struct FrameDetail {
        const uptr rel_pc;
        const char *map_name;
        const char *function_name;
    };

    struct Backtrace {
        size_t max_frames = 0;
        size_t frame_size = 0;
        std::shared_ptr<Frame> frames;
    };

    enum BacktraceMode {
        FramePointer = 0,
        Quicken = 1,
        DwarfBased = 2
    };

    typedef void(*quicken_generate_delegate_t)(const std::string &, const uint64_t);

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_DEFINE_H
