#include <stdint.h>
#include <bits/pthread_types.h>
#include <cstdlib>
#include <pthread.h>
#include <FallbackUnwinder.h>
#include <MapsControll.h>
#include <unwindstack/MachineArm64.h>
#include <unwindstack/RegsGetLocal.h>

#include "FastRegs.h"
#include "FastUnwinder.h"

#include "android-base/include/android-base/macros.h"
#include "../../common/PthreadExt.h"

namespace wechat_backtrace {

    using namespace std;

    static vector<pair<uintptr_t, uintptr_t>> gSkipFunctions;

    vector<pair<uintptr_t, uintptr_t>>* GetSkipFunctions() {
        return &gSkipFunctions;
    }

    static inline bool ShouldGoFallback(uptr pc) {
#ifdef __aarch64__
        for (const auto &range : gSkipFunctions) {
            if (range.first < pc && pc < range.second) {
                return true;
            }
        }
#endif
        return false;
    }


    // In GCC on ARM bp points to saved lr, not fp, so we should check the next
    // cell in stack to be a saved frame pointer. GetCanonicFrame returns the
    // pointer to saved frame pointer in any case.
    static inline uptr * GetCanonicFrame(uptr fp, uptr stack_top, uptr stack_bottom) {
        if (UNLIKELY(stack_top < stack_bottom)) {
            return 0;
        }
#ifdef __arm__
        if (!IsValidFrame(fp, stack_top, stack_bottom)) return 0;

        uptr *fp_prev = (uptr *)fp;

        if (IsValidFrame((uptr)fp_prev[0], stack_top, stack_bottom)) return fp_prev;

        // The next frame pointer does not look right. This could be a GCC frame, step
        // back by 1 word and try again.
        if (IsValidFrame((uptr)fp_prev[-1], stack_top, stack_bottom)) return fp_prev - 1;

        // Nope, this does not look right either. This means the frame after next does
        // not have a valid frame pointer, but we can still extract the caller PC.
        // Unfortunately, there is no way to decide between GCC and LLVM frame
        // layouts. Assume LLVM.
        return fp_prev;
#else
        return (uptr *) fp;
#endif
    }

    static inline void fpUnwindImpl(uptr pc, uptr fp, uptr stack_top, uptr stack_bottom,
                                    uptr * backtrace, uptr frame_max_size, uptr &frame_size) {

        const uptr kPageSize = GetPageSize();
        backtrace[0] = pc;
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
                backtrace[frame_size++] = (uptr) pc1;
            }
            bottom = (uptr) frame;
            frame = GetCanonicFrame((uptr) frame[0], stack_top, bottom);
        }
    }

    static inline uptr * StepFallback(uptr pc, uptr sp,
            bool &finished, unwindstack::FallbackUnwinder *&fallbackUnwinder, uptr &next_pc) {
#ifdef __arm__
        // TODO
        return 0;
#else

        if (fallbackUnwinder == NULL) {
            unwindstack::Regs *regs = unwindstack::Regs::CreateFromLocal();
            unwindstack::RegsGetLocal(regs);
            regs->set_pc(pc);
            regs->set_sp(sp);
            ((uptr *)regs->RawData())[unwindstack::ARM64_REG_LR] = pc;

            auto process_memory = unwindstack::Memory::CreateProcessMemory(getpid());

            unwindstack::LocalMaps *maps = GetMapsCache();

            if (!maps) {
                return 0;
            }

            fallbackUnwinder = new unwindstack::FallbackUnwinder(maps, regs, process_memory);
            unwindstack::JitDebug jit_debug(process_memory);
            fallbackUnwinder->SetJitDebug(&jit_debug, regs->Arch());
            fallbackUnwinder->SetResolveNames(false);
        }

        fallbackUnwinder->fallbackUnwindFrame(finished);

        unwindstack::Regs *regs = fallbackUnwinder->getRegs();

        switch (fallbackUnwinder->LastErrorCode()) {
            case unwindstack::ErrorCode::ERROR_MEMORY_INVALID:
            case unwindstack::ErrorCode::ERROR_UNWIND_INFO:
            case unwindstack::ErrorCode::ERROR_UNSUPPORTED:
            case unwindstack::ErrorCode::ERROR_INVALID_MAP:
            case unwindstack::ErrorCode::ERROR_INVALID_ELF:
                return 0;
        }

        next_pc = ((uptr *) regs->RawData())[unwindstack::ARM64_REG_PC];

        return (uptr *)(((uptr *) regs->RawData())[unwindstack::ARM64_REG_FP]);
#endif
    }

    static inline void fpUnwindWithFallbackImpl(uptr pc, uptr fp, uptr stack_top, uptr stack_bottom,
                                                uptr * backtrace, uptr frame_max_size,
                                                uptr &frame_size) {

        const uptr kPageSize = GetPageSize();
        backtrace[0] = pc;
        frame_size = 1;

        unwindstack::FallbackUnwinder *fallbackUnwinder = NULL;
        bool finished = false;

        if (UNLIKELY(stack_top < 4096)) return;  // Sanity check for stack top.

        uptr *frame = GetCanonicFrame(fp, stack_top, stack_bottom);

        // Lowest possible address that makes sense as the next frame pointer.
        // Goes up as we walk the stack.
        uptr bottom = stack_bottom;

        uptr next_pc = 0;
        while (frame_size < frame_max_size) {

            if (ShouldGoFallback(backtrace[frame_size - 1]) ||
                    !(IsValidFrame((uptr) frame, stack_top, bottom) &&
                        IsAligned((uptr) frame, sizeof(*frame)))) {

                frame = StepFallback(backtrace[frame_size - 1], bottom, finished,
                        fallbackUnwinder, next_pc);

                if (frame == 0) {
                    break;
                }

                if (next_pc != pc) {
                    backtrace[frame_size++] = next_pc;
                }

                if (finished) break;

                continue;
            }

            uptr pc1 = frame[1];
            // Let's assume that any pointer in the 0th page (i.e. <0x1000 on i386 and
            // x86_64) is invalid and stop unwinding here.  If we're adding support for
            // a platform where this isn't true, we need to reconsider this check.
            if (pc1 < kPageSize)
                break;
            if (pc1 != pc) {
                backtrace[frame_size++] = (uptr) pc1;
            }
            bottom = (uptr) frame;
            frame = GetCanonicFrame((uptr) frame[0], stack_top, bottom);
        }

        if (fallbackUnwinder != NULL) {
            delete fallbackUnwinder;
        }
    }

    void FpUnwind(uptr * regs, uptr * backtrace, uptr frame_max_size, uptr &frame_size, bool fallback) {

        pthread_attr_t attr;
        pthread_getattr_ext(pthread_self(), &attr);
        uptr stack_bottom = reinterpret_cast<uptr>(attr.stack_base);
        uptr tack_top = reinterpret_cast<uptr>(attr.stack_base) + attr.stack_size;
        uptr fp = regs[0]; // x29
        uptr pc = regs[3]; // x32

        if (!fallback) {
            fpUnwindImpl(pc, fp, tack_top, stack_bottom, backtrace, frame_max_size, frame_size);
        } else {
            fpUnwindWithFallbackImpl(pc, fp, tack_top, stack_bottom, backtrace, frame_max_size,
                    frame_size);
        }
    }



} // namespace wechat_backtrace