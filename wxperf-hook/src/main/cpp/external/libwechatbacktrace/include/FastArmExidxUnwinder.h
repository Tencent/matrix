#ifndef _LIBUNWINDSTACK_FAST_ARM_EXIDX_UNWINDER_H
#define _LIBUNWINDSTACK_FAST_ARM_EXIDX_UNWINDER_H

typedef uintptr_t uptr;

namespace wechat_backtrace {

    void FastExidxUnwind(uptr * regs, uptr * backtrace, uptr frame_max_size, uptr &frame_size);

}  // namespace wechat_backtrace

#endif  // _LIBUNWINDSTACK_FAST_ARM_EXIDX_UNWINDER_H
