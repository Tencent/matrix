#ifndef _LIBUNWINDSTACK_FASTUNWINDER_H
#define _LIBUNWINDSTACK_FASTUNWINDER_H

typedef uintptr_t uptr;

namespace wechat_backtrace {

    std::vector<std::pair<uintptr_t, uintptr_t>>* GetSkipFunctions();

    void FpUnwind(uptr *regs, uptr *backtrace, uptr frame_max_size, uptr &frame_size, bool fallback);

}  // namespace wechat_backtrace

#endif  // _LIBUNWINDSTACK_FASTUNWINDER_H
