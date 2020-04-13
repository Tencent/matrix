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

#include "ElfInterfaceArm64.h"

namespace unwindstack {

    uint64_t prolo_prologue[4] = {0};
    uint64_t dwarf_prologue[4] = {0};

    bool
    ElfInterfaceArm64::PreStep(uint64_t pc, uint64_t load_bias, Regs *regs, Memory *process_memory,
                               bool *finished) {

        RegsArm64 *cur_regs_arm64;
        cur_regs_arm64 = dynamic_cast<RegsArm64 *>(regs);

        void *cur_fp = reinterpret_cast<void *>(cur_regs_arm64->fp());
        memcpy(dwarf_prologue, cur_fp, 0x10);

        uint64_t next_sp = cur_regs_arm64->fp() + 0x10;

        dwarf_prologue[2] = next_sp;

        uint64_t next_pc = dwarf_prologue[1];
        dwarf_prologue[3] = next_pc;

        RegsArm64 cur_regs_arm64_prologue;
        if (!prologueFrames.empty()) {
            cur_regs_arm64_prologue = prologueFrames.back();
        } else {
            stackLimitMax           = cur_regs_arm64->sp() + 64 * 1024;
            stackLimitMin           = cur_regs_arm64->sp();
            cur_regs_arm64_prologue = *cur_regs_arm64;
        }

        cur_fp = reinterpret_cast<void *>(cur_regs_arm64_prologue.fp());
        if (FallbackPCRange::GetInstance()->ShouldFallback(cur_regs_arm64_prologue.pc()) ||
            (uint64_t) cur_fp > stackLimitMax || (uint64_t) cur_fp <= stackLimitMin) {
            memset(prolo_prologue, 0, sizeof(prolo_prologue));
            return true;
        }

        memcpy(prolo_prologue, cur_fp, 0x10); // filled prolo_prologue[0,1]

        // next sp
        prolo_prologue[2] = cur_regs_arm64_prologue.fp() + 0x10;

        // next pc
        prolo_prologue[3] = prolo_prologue[1];

        RegsArm64 next_regs;
        next_regs.set_fp(prolo_prologue[0]);
        next_regs.set_lr(prolo_prologue[1]);
        next_regs.set_sp(prolo_prologue[2]);
        next_regs.set_pc(prolo_prologue[3]);

        prologueFrames.push_back(next_regs);

        return true;
    }

    bool
    ElfInterfaceArm64::PostStep(uint64_t pc, uint64_t load_bias, Regs *regs, Memory *process_memory,
                                bool *finished) {

        RegsArm64 *regs_arm64 = dynamic_cast<RegsArm64 *>(regs);

        uint64_t rel_pc = 0;

        LOGD("Unwind-debug", "prolo_prolog [X29fp, X30lr, X31sp, X32pc] = [%lu,%lu,%lu,%lu,%lu]",
             prolo_prologue[0], prolo_prologue[1], prolo_prologue[2], prolo_prologue[3], rel_pc);
        LOGD("Unwind-debug", "dwarf_prolog [X29fp, X30lr, X31sp, X32pc] = [%lu,%lu,%lu,%lu]",
             dwarf_prologue[0], dwarf_prologue[1], dwarf_prologue[2], dwarf_prologue[3]);
        LOGD("Unwind-debug", "dwarf_unwind [X29fp, X30lr, X31sp, X32pc] = [%lu,%lu,%lu,%lu]",
             regs_arm64->fp(), regs_arm64->lr(), regs_arm64->sp(), regs_arm64->pc());


        if (0 == prolo_prologue[0]) {
            LOGD(TAG, "using dwarf step result");
            prologueFrames.push_back(*regs_arm64);
        }

        LOGD("Unwind-debug", "----------------");

        return true;
    }

    bool ElfInterfaceArm64::StepPrologue(uint64_t pc, uint64_t load_bias, Regs *regs,
                                         Memory *process_memory, bool *finished) {

//        if (true) {
//            return false;
//        }

        RegsArm64 *regs_arm64 = dynamic_cast<RegsArm64 *>(regs);
        LOGD("Unwind-debug", "begin StrepPrologue cur [X29fp, X30lr, X31sp, X32pc] = [%lu,%lu,%lu,%lu]", regs_arm64->fp(), regs_arm64->lr(), regs_arm64->sp(), regs_arm64->pc());

        *finished = regs_arm64->pc() == 0
                || regs_arm64->fp() == 0
                || regs_arm64->lr() == 0;

        if (*finished) {
            LOGD("Unwind-debug", "finish");
            return false;
        }

        if (FallbackPCRange::GetInstance()->ShouldFallback(regs_arm64->pc())) {
            LOGE("Unwind-debug", "fallback for PC range ");
            return false;
        }

        void *cur_fp = reinterpret_cast<void *>(regs_arm64->fp());

        // TODO check fp ?

        uintptr_t next_fp_lr[2] = {0, 0};
        memcpy(next_fp_lr, cur_fp, sizeof(next_fp_lr));

        uintptr_t next_sp = regs_arm64->fp() + 0x10;
        uintptr_t next_pc = next_fp_lr[1];

        LOGD("Unwind-debug",
             "StepPrologue [X29fp, X30lr, X31sp, X32pc] = [%lu,%lu,%lu,%lu] -> cur [X29fp, X30lr, X31sp, X32pc] = [%lu,%lu,%lu,%lu]",
             next_fp_lr[0], next_fp_lr[1], next_sp, next_pc,
             regs_arm64->fp(), regs_arm64->lr(), regs_arm64->sp(), regs_arm64->pc());

        if (next_fp_lr[0] < regs_arm64->fp() || next_sp < regs_arm64->sp()) {
            LOGE("Unwind-debug", "fallback for illegal fp sp");
            return false;
        }

        LOGD("Unwind-debug", "Stepped");

        regs_arm64->set_sp(next_sp);
        regs_arm64->set_pc(next_pc);
        regs_arm64->set_fp(next_fp_lr[0]);
        regs_arm64->set_lr(next_fp_lr[1]);



        return true;
    }

    bool
    ElfInterfaceArm64::Step(uint64_t pc, uint64_t load_bias, Regs *regs, Memory *process_memory,
                            bool *finished) {
//         For Arm64 devices
//        LOGD(TAG, "steping");
        bool stepped = StepPrologue(pc, load_bias, regs, process_memory, finished);

        if (!stepped) {
//            LOGD("Unwind-debug", "not stepped, fallback to dwarf");
            ElfInterface64::Step(pc, load_bias, regs, process_memory, finished);
        }

        return stepped;

//        return StepPrologue(pc, load_bias, regs, process_memory, finished)
//            || ElfInterface64::Step(pc, load_bias, regs, process_memory, finished);

    }

    FallbackPCRange *FallbackPCRange::INSTANCE = nullptr;

    mutex FallbackPCRange::mMutex;

    FallbackPCRange::FallbackPCRange() {

        void *trampoline = EnhanceDlsym::getInstance()->dlsym(
                "/apex/com.android.runtime/lib64/libart.so",
                "art_quick_generic_jni_trampoline");
        if (nullptr != trampoline) {
            mSkipFunctions.push_back({reinterpret_cast<uintptr_t>(trampoline),
                                      reinterpret_cast<uintptr_t>(trampoline) + 0x27C});
        }

//        LOGD(TAG, "trampoline %p", trampoline);

        void *art_quick_invoke_static_stub = EnhanceDlsym::getInstance()->dlsym(
                "/apex/com.android.runtime/lib64/libart.so",
                "art_quick_invoke_static_stub");
        if (nullptr != art_quick_invoke_static_stub) {
            mSkipFunctions.push_back(
                    {reinterpret_cast<uintptr_t >(art_quick_invoke_static_stub),
                     reinterpret_cast<uintptr_t >(art_quick_invoke_static_stub) + 0x280});
        }

//        LOGD(TAG, "art_quick_invoke_static_stub %p", art_quick_invoke_static_stub);

//        void *mterp_op_invoke_static = EnhanceDlsym::getInstance()->dlsym(
//                "/apex/com.android.runtime/lib64/libart.so", "mterp_op_invoke_static");
//
//        if (nullptr != mterp_op_invoke_static) {
//            mSkipFunctions.push_back({reinterpret_cast<uintptr_t>(mterp_op_invoke_static),
//                                      reinterpret_cast<uintptr_t>(mterp_op_invoke_static) + 0x7C});
//        }

//        void *jvalue = EnhanceDlsym::getInstance()->dlsym(
//                "/apex/com.android.runtime/lib64/libart.so",
//                "_ZN3art9ArtMethod6InvokeEPNS_6ThreadEPjjPNS_6JValueEPKc");
//        if (nullptr != jvalue) {
//            mSkipFunctions.push_back({reinterpret_cast<uintptr_t>(jvalue),
//                                      reinterpret_cast<uintptr_t>(jvalue) + 0x228});
//        }

        SkipDexPC();

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

    bool FallbackPCRange::ShouldFallback(uintptr_t pc) {
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
}
