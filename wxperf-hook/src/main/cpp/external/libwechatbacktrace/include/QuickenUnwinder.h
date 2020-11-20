#ifndef _LIBWECHATBACKTRACE_QUICKEN_UNWINDER_H
#define _LIBWECHATBACKTRACE_QUICKEN_UNWINDER_H

#include <jni.h>

#include <unwindstack/Elf.h>

#include "Errors.h"

typedef uintptr_t uptr;

namespace wechat_backtrace {

    QUT_EXTERN_C void StatisticWeChatQuickenUnwindTable(const std::string &sopath);

    QUT_EXTERN_C void GenerateQutForLibrary(const std::string &sopath);

    QUT_EXTERN_C void ConsumeRequestingQut();

    QUT_EXTERN_C QutErrorCode WeChatQuickenUnwind(unwindstack::ArchEnum arch, uptr *regs, uptr *backtrace,
                                     uptr frame_max_size, uptr &frame_size);

    QUT_EXTERN_C QutErrorCode WeChatQuickenUnwindV2_WIP(unwindstack::ArchEnum arch, uptr *regs, uptr *backtrace,
                                           uptr frame_max_size, uptr &frame_size);

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_UNWINDER_H
