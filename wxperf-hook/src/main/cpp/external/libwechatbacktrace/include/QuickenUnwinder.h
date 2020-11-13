#ifndef _LIBWECHATBACKTRACE_QUICKEN_UNWINDER_H
#define _LIBWECHATBACKTRACE_QUICKEN_UNWINDER_H

#include <unwindstack/Elf.h>

#include "Errors.h"

typedef uintptr_t uptr;

namespace wechat_backtrace {

    void StatisticWeChatQuickenUnwindTable(const char *const sopath, const char *const soname);

    QutErrorCode WeChatQuickenUnwind(unwindstack::ArchEnum arch, uptr *regs, uptr *backtrace,
                                     uptr frame_max_size, uptr &frame_size);

    QutErrorCode WeChatQuickenUnwindV2_WIP(unwindstack::ArchEnum arch, uptr *regs, uptr *backtrace,
                                           uptr frame_max_size, uptr &frame_size);

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_UNWINDER_H
