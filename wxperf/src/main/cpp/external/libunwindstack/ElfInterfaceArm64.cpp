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

#include "ElfInterfaceArm64.h"

namespace unwindstack {

    uint64_t prolo_prologue[4] = {0};
    uint64_t dwarf_prologue[4] = {0};
//    uint64_t dwarf_unwind[4] = {0};

//    bool IsOnStack(void *) {
//
//    }


    bool ElfInterfaceArm64::PreStep(uint64_t pc, uint64_t load_bias, Regs *regs, Memory *process_memory,
                 bool *finished) {

        RegsArm64 *cur_regs_arm64;
        cur_regs_arm64 = dynamic_cast<RegsArm64 *>(regs);

        void * cur_fp = reinterpret_cast<void *>(cur_regs_arm64->fp());
        memcpy(dwarf_prologue, cur_fp, 0x10);

        uint64_t next_sp = cur_regs_arm64->fp() + 0x10;

        dwarf_prologue[2] = next_sp;

        uint64_t next_pc = dwarf_prologue[1];
        dwarf_prologue[3] = next_pc;

//        dwarf_prolog

        RegsArm64 cur_regs_arm64_prologue;
        if (!prologueFrames.empty()) {
            cur_regs_arm64_prologue = prologueFrames.back();
        } else {
            stackLimitMax = cur_regs_arm64->sp() + 64 * 1024;
            stackLimitMin = cur_regs_arm64->sp();
            cur_regs_arm64_prologue = *cur_regs_arm64;
        }

        cur_fp = reinterpret_cast<void *>(cur_regs_arm64_prologue.fp());
        if ((uint64_t)cur_fp > stackLimitMax || (uint64_t)cur_fp <= stackLimitMin) {
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

    bool ElfInterfaceArm64::PostStep(uint64_t pc, uint64_t load_bias, Regs *regs, Memory *process_memory,
                  bool *finished) {

        RegsArm64 *regs_arm64 = dynamic_cast<RegsArm64 *>(regs);

//        dwarf_unwind[0] = regs_arm64->fp();
//        dwarf_unwind[1] = regs_arm64->lr();

        uint64_t rel_pc = 0;

//        MapInfo *mapInfo = maps_->Find(prolo_prologue[3]);
//        if (mapInfo) {
//            Memory memory_copy = *process_memory;
//            LOGD("Yves-Test", "mapinfo = %s", mapInfo->name.c_str());
//            std::shared_ptr<Memory> sp_memory = std::shared_ptr<Memory>(&memory_copy);
//            Elf *elf = mapInfo->GetElf(sp_memory, maps_);
//            LOGD("Yves-Test", "elf = %p", elf);
//            rel_pc = elf->GetRelPc(prolo_prologue[3], mapInfo);
//        }

        LOGD("Unwind-debug", "prolo_prolog [X29fp, X30lr, X31sp, X32pc] = [%lu,%lu,%lu,%lu,%lu]", prolo_prologue[0], prolo_prologue[1], prolo_prologue[2], prolo_prologue[3], rel_pc);
        LOGD("Unwind-debug", "dwarf_prolog [X29fp, X30lr, X31sp, X32pc] = [%lu,%lu,%lu,%lu]", dwarf_prologue[0], dwarf_prologue[1], dwarf_prologue[2], dwarf_prologue[3]);
        LOGD("Unwind-debug", "dwarf_unwind [X29fp, X30lr, X31sp, X32pc] = [%lu,%lu,%lu,%lu]", regs_arm64->fp(), regs_arm64->lr(), regs_arm64->sp(), regs_arm64->pc());


        if (FallbackPCRange::GetInstance()->ShouldFallback(regs_arm64->pc())) {
            LOGD(TAG, "should fallback %lu", regs_arm64->pc());
        }

        LOGD("Unwind-debug", "------ %d %d", dwarf_prologue[0] == regs_arm64->fp(), dwarf_prologue[1] == regs_arm64->lr());

        return true;
    }

    bool
    ElfInterfaceArm64::Step(uint64_t pc, uint64_t load_bias, Regs *regs, Memory *process_memory,
                            bool *finished) {
        // For Arm64 devices
//        return StepPrologue(pc, load_bias, regs, process_memory, finished)
//               || ElfInterface64::Step(pc, load_bias, regs, process_memory, finished);

        return PreStep(pc, load_bias, regs, process_memory, finished)
        && ElfInterface64::Step(pc, load_bias, regs, process_memory, finished)
        && PostStep(pc, load_bias, regs, process_memory, finished);
    }

    bool ElfInterfaceArm64::StepPrologue(uint64_t pc, uint64_t load_bias, Regs *regs,
                                         Memory *process_memory, bool *finished) {

        RegsArm64 *regs_arm64 = dynamic_cast<RegsArm64 *>(regs);

        void * cur_fp = reinterpret_cast<void *>(regs_arm64->fp());

        uint64_t pair[2] = {0, 0};

        memcpy(pair, cur_fp, sizeof(pair));

        LOGD("Unwind-debug", "regs[X29fp, X30lr, SP, PC] = [%lu, %lu, %lu, %lu]", regs_arm64->fp(), regs_arm64->lr(), regs_arm64->sp(), regs_arm64->pc());
//        LOGD("Unwind-debug", "mem [X29, X30] = [%" PRIxPTR ", %" PRIxPTR "]", pair[0], pair[1]);
        LOGD("Unwind-debug", "mem [X29fp, X30lr] = [%lu,%lu]", pair[0], pair[1]);

        LOGD("Unwind-debug", "-------");


        return false;
    }

    FallbackPCRange *FallbackPCRange::INSTANCE = nullptr;
    mutex FallbackPCRange::mMutex;
}
