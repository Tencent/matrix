#include <stdint.h>
#include <bits/pthread_types.h>
#include <cstdlib>
#include <pthread.h>

#include <unwindstack/MachineArm64.h>
#include <unwindstack/RegsGetLocal.h>

#include "MinimalRegs.h"
#include "FpUnwinder.h"

#include "LocalMaps.h"

#include "android-base/macros.h"
#include "PthreadExt.h"

namespace wechat_backtrace {

    QUT_EXTERN_C_BLOCK

    using namespace std;

    // Check if given pointer points into allocated stack area.
    static inline bool IsValidFrame(uptr frame, uptr stack_top, uptr stack_bottom) {
        return frame > stack_bottom && frame < stack_top - 2 * sizeof(uptr);
    }

    // In GCC on ARM bp points to saved lr, not fp, so we should check the next
    // cell in stack to be a saved frame pointer. GetCanonicFrame returns the
    // pointer to saved frame pointer in any case.
    static inline uptr *GetCanonicFrame(uptr fp, uptr stack_top, uptr stack_bottom) {
        if (UNLIKELY(stack_top < stack_bottom)) {
            return 0;
        }
#ifdef __arm__
        if (!IsValidFrame(fp, stack_top, stack_bottom)) return 0;

        uptr *fp_prev = (uptr *) fp;

        if (IsValidFrame((uptr) fp_prev[0], stack_top, stack_bottom)) return fp_prev;

        // The next frame pointer does not look right. This could be a GCC frame, step
        // back by 1 word and try again.
        if (IsValidFrame((uptr) fp_prev[-1], stack_top, stack_bottom)) return fp_prev - 1;

        // Nope, this does not look right either. This means the frame after next does
        // not have a valid frame pointer, but we can still extract the caller PC.
        // Unfortunately, there is no way to decide between GCC and LLVM frame
        // layouts. Assume LLVM.
        return fp_prev;
#else
        return (uptr *) fp;
#endif
    }

    static inline uptr GetPageSize() {
        return 4096;
    }

    static inline bool IsAligned(uptr a, uptr alignment) {
        return (a & (alignment - 1)) == 0;
    }

    static inline void fpUnwindImpl(uptr pc, uptr fp, const uptr stack_top, const uptr stack_bottom,
                                    Frame *backtrace, const size_t frame_max_size, size_t &frame_size) {

        const uptr kPageSize = GetPageSize();
        backtrace[0].pc = pc;
        frame_size = 1;
        if (UNLIKELY(stack_top < 4096)) return;  // Sanity check for stack top.
        uptr *frame = GetCanonicFrame(fp, stack_top, stack_bottom);
        // Lowest possible address that makes sense as the next frame pointer.
        // Goes up as we walk the stack.
        uptr bottom = stack_bottom;
        // Avoid infinite loop when frame == frame[0] by using frame > prev_frame.
        while (IsValidFrame((uptr) frame, stack_top, bottom) &&
               IsAligned((uptr) frame, sizeof(*frame)) &&
               frame_size < frame_max_size) {
            uptr pc1 = frame[1];
            // Let's assume that any pointer in the 0th page (i.e. <0x1000 on i386 and
            // x86_64) is invalid and stop unwinding here.  If we're adding support for
            // a platform where this isn't true, we need to reconsider this check.
            if (pc1 < kPageSize)
                break;
            if (pc1 != pc) {
                backtrace[frame_size++].pc = (uptr) pc1;
            }
            bottom = (uptr) frame;
            frame = GetCanonicFrame((uptr) frame[0], stack_top, bottom);
        }
    }

    void
    FpUnwind(uptr *regs, Frame *backtrace, const size_t max_size, size_t &frame_size) {

        pthread_attr_t attr;
        BACKTRACE_FUNC_WRAPPER(pthread_getattr_ext)(pthread_self(), &attr);
        uptr stack_bottom = reinterpret_cast<uptr>(attr.stack_base);
        uptr tack_top = reinterpret_cast<uptr>(attr.stack_base) + attr.stack_size;

        uptr fp = regs[0]; // x29 or r7
        uptr pc = regs[3]; // x32 or r15

        fpUnwindImpl(pc, fp, tack_top, stack_bottom, backtrace, max_size, frame_size);

    }

    QUT_EXTERN_C_BLOCK_END
} // namespace wechat_backtrace