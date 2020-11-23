#ifndef _LIBWECHATBACKTRACE_FP_UNWINDER_H
#define _LIBWECHATBACKTRACE_FP_UNWINDER_H

#include "Predefined.h"

namespace wechat_backtrace {

    std::vector<std::pair<uintptr_t, uintptr_t>> *GetSkipFunctions();

    void
    FpUnwind(uptr *regs, Frame *backtrace, const size_t frame_max_size, size_t &frame_size);

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_FP_UNWINDER_H
