#ifndef _LIBWECHATBACKTRACE_TYPES_H
#define _LIBWECHATBACKTRACE_TYPES_H

#include <memory>
#include <unwindstack/Elf.h>

// *** Version ***
#define QUT_VERSION 0x1

#define MAX_FRAME_SHORT 16
#define MAX_FRAME_NORMAL 32
#define MAX_FRAME_LONG 64

#define QUT_ARCH_ARM 0x1
#define QUT_ARCH_ARM64 0x2

#ifdef __cplusplus__
#define QUT_EXTERN_C extern "C"
#else
#define QUT_EXTERN_C
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

    struct Frame {
        uptr pc;
//        bool is_dex_pc;
    };

    struct FrameDetail {
        const uptr rel_pc;
        const char *map_name;
        const char *function_name;
    };

    struct Backtrace {
        size_t max_frames;
        size_t frame_size;
        std::shared_ptr<Frame> frames;
    };


}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_TYPES_H
