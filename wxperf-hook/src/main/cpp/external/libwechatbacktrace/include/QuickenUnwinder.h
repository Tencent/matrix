#ifndef _LIBWECHATBACKTRACE_QUICKEN_UNWINDER_H
#define _LIBWECHATBACKTRACE_QUICKEN_UNWINDER_H

#include "Errors.h"

typedef uintptr_t uptr;

namespace wechat_backtrace {

QutErrorCode WeChatQuickenUnwind(uptr* regs, uptr* backtrace, uptr frame_max_size, uptr &frame_size);

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_UNWINDER_H
