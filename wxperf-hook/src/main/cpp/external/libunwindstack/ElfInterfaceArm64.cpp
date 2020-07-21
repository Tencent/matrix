//
// Created by Yves on 2020-04-02.
//

#include <elf.h>
#include <cstdint>

#include <unwindstack/ElfInterface.h>
#include <unwindstack/Regs.h>
#include <unwindstack/Memory.h>
#include <unwindstack/RegsArm64.h>
#include <unwindstack/Maps.h>
#include <android/log.h>
#include <cinttypes>
#include <string>
#include <fstream>
#include <fcntl.h>

#include "ElfInterfaceArm64.h"
#include "TimeUtil.h"

namespace unwindstack {


    inline bool ElfInterfaceArm64::StepPrologue(uint64_t rel_pc, Regs *regs,
                                                Memory *process_memory, bool *finished) {
//        return false;

        RegsArm64 *regs_arm64 = dynamic_cast<RegsArm64 *>(regs);
        LOGE("Unwind-debug",
             "+++++++++++++++++ begin StepPrologue cur [X29fp, X30lr, X31sp, X32pc] = [%lu,%lu,%lu,%lu]",
             regs_arm64->fp(), regs_arm64->lr(), regs_arm64->sp(), regs_arm64->pc());

        if (FallbackPCRange::GetInstance()->ShouldPCFallback(regs_arm64->pc())) {
            LOGE("Unwind-debug", "fallback for PC range ");
            return false;
        }

        if (FallbackPCRange::GetInstance()->ShouldFPSPFallback(regs_arm64->fp(),
                                                               regs_arm64->sp(), regs_arm64)) {
            LOGE(TAG, "fallback for fp sp");
            return false;
        }

        void *cur_fp = reinterpret_cast<void *>(regs_arm64->fp());

        uintptr_t next_fp_lr[2] = {0, 0};
        memcpy(next_fp_lr, cur_fp, sizeof(next_fp_lr));

        uintptr_t next_sp = regs_arm64->fp() + 0x10;
        uintptr_t next_pc = next_fp_lr[1];

        LOGD("Unwind-debug",
             "StepPrologue [X29fp, X30lr, X31sp, X32pc] = [%lu,%lu,%lu,%lu]",
             next_fp_lr[0], next_fp_lr[1], next_sp, next_pc);

        if (next_fp_lr[0] < regs_arm64->fp() || next_sp < regs_arm64->sp()) {

            LOGE("Unwind-debug",
                 "CUR: fallback for illegal [fp, sp, stack, size] = [%lu, %lu, %lu, %zu]",
                 next_fp_lr[0], next_sp, regs_arm64->fp(), regs_arm64->sp());

//            *finished = true;
//            return true;
            return false;
        }

        LOGE("Unwind-debug", "---------------------- end StepPrologue");

        regs_arm64->set_sp(next_sp);
        regs_arm64->set_pc(next_pc);
        regs_arm64->set_fp(next_fp_lr[0]);
        regs_arm64->set_lr(next_fp_lr[1]);

        *finished = regs_arm64->pc() == 0
                    || regs_arm64->fp() == 0
                    || regs_arm64->lr() == 0;

        return true;
    }

    bool
    ElfInterfaceArm64::Step(uint64_t rel_pc, Regs *regs, Memory *process_memory,
                            bool *finished) {
//         For Arm64 devices
//        long begin = CurrentNano();

        bool ret = StepPrologue(rel_pc, regs, process_memory, finished)
                   || ElfInterface64::Step(rel_pc, regs, process_memory, finished);

//        LOGE("Unwind-debug", "step arm64 cost: %ld", (CurrentNano() - begin));

        return ret;

    }

    FallbackPCRange *FallbackPCRange::INSTANCE = nullptr;

//    mutex FallbackPCRange::mMutex;
//    mutex mMutex;
    pthread_mutex_t pthread_mutex = PTHREAD_MUTEX_INITIALIZER;

    FallbackPCRange::FallbackPCRange() {

        void *handle = enhance::dlopen("libart.so", 0);
        if (handle) {
            void *trampoline = enhance::dlsym(handle, "art_quick_generic_jni_trampoline");
            if (nullptr != trampoline) {
                mSkipFunctions.emplace_back(reinterpret_cast<uintptr_t>(trampoline),
                                          reinterpret_cast<uintptr_t>(trampoline) + 0x27C);
            }

            void *art_quick_invoke_static_stub = enhance::dlsym(handle, "art_quick_invoke_static_stub");
            if (nullptr != art_quick_invoke_static_stub) {

                mSkipFunctions.emplace_back(reinterpret_cast<uintptr_t >(art_quick_invoke_static_stub),
                         reinterpret_cast<uintptr_t >(art_quick_invoke_static_stub) + 0x280);
            }
        }
        enhance::dlclose(handle);

        SkipDexPC();

        fd = open("/dev/random", O_WRONLY | O_CLOEXEC);
    }

    static inline bool EndWith(std::string const &value, std::string const &ending) {
        if (ending.size() > value.size()) {
            return false;
        }
        return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
    }

    void FallbackPCRange::SkipDexPC() {

        uintptr_t map_base_addr;
        uintptr_t map_end_addr;
        char      map_perm[5];

        ifstream    input("/proc/self/maps");
        std::string aline;

        while (getline(input, aline)) {
            if (sscanf(aline.c_str(),
                       "%" PRIxPTR "-%" PRIxPTR " %4s",
                       &map_base_addr,
                       &map_end_addr,
                       map_perm) != 3) {
                continue;
            }

            if ('r' == map_perm[0]
                && 'x' == map_perm[2]
                && 'p' == map_perm[3]
                &&
                (EndWith(aline, ".dex")
                 || EndWith(aline, ".odex")
                 || EndWith(aline, ".vdex"))) {

                mSkipFunctions.push_back({map_base_addr, map_end_addr});
            }
        }
    }

    inline bool FallbackPCRange::ShouldPCFallback(uintptr_t pc) {
//        LOGD(TAG, "checking %lu", pc);

        for (const auto &range : mSkipFunctions) {
//            LOGD(TAG, "{%lu, %lu}", range.first, range.second);
            if (range.first < pc && pc < range.second) {
//                LOGD(TAG, "ShouldFallback");
                return true;
            }
        }
        return false;
    }

    bool FallbackPCRange::ShouldFPSPFallback(uintptr_t fp, uintptr_t sp, RegsArm64* regs) {
        uint64_t stack_top    = regs->get_stack_top();
        uint64_t stack_bottom = regs->get_stack_bottom();

        LOGD(TAG, "top %lu, bot %lu", stack_top, stack_bottom);
        if (stack_top && stack_bottom) {
            // 在栈范围内则返回 false
            LOGD(TAG, "test ShouldFPSPFallback [fp, sp, top, bottom] = [%lu, %lu, %lu, %lu]", fp, sp, stack_top, stack_bottom);
            return !(stack_top < fp && fp < stack_bottom
                     && stack_top < sp && sp < stack_bottom);
        }

        bool ret = write(fd, reinterpret_cast<const void *>(fp), 0x10) < 0;

        return ret;
    }
}
