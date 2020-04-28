#ifndef _LIBUNWINDSTACK_FASTUNWINDER_H
#define _LIBUNWINDSTACK_FASTUNWINDER_H

typedef uintptr_t uptr;

namespace wechat_backtrace {

    std::vector<std::pair<uintptr_t, uintptr_t>>* GetSkipFunctions();

    // Check if given pointer points into allocated stack area.
    static inline bool IsValidFrame(uptr frame, uptr stack_top, uptr stack_bottom) {
        return frame > stack_bottom && frame < stack_top - 2 * sizeof (uptr);
    }

    inline uptr GetPageSize() {
        return 4096;
    }

    inline bool IsAligned(uptr a, uptr alignment) {
        return (a & (alignment - 1)) == 0;
    }

    void FpUnwind(uptr *backtrace, uptr frame_max_size, uptr &frame_size, bool fallback);

}  // namespace wechat_backtrace

#endif  // _LIBUNWINDSTACK_FASTUNWINDER_H
