//
// Created by Carl on 2020-09-21.
//

#include <deps/android-base/include/android-base/macros.h>
#include <MinimalRegs.h>
#include "../../common/Log.h"
#include "QuickenInterface.h"
#include "QuickenMaps.h"

namespace wechat_backtrace {

using namespace std;
using namespace unwindstack;

uint64_t QuickenInterface::GetLoadBias() {
    return load_bias_;
}

bool QuickenInterface::GenerateQuickenTable(unwindstack::Memory* process_memory) {

    lock_guard<mutex> lock(lock_);

    if (qut_sections_) {
        return true;
    }

    QuickenTableGenerator generator(memory_, process_memory);
    QutSections* qut_sections = new QutSections();

    size_t bad_enties = 0;
    bool ret = generator.GenerateFutSections(start_offset_, total_entries_, qut_sections, bad_enties);

    bad_enties_ = bad_enties;

    if (ret) {
        qut_sections_.reset(qut_sections);
    } else {
        delete qut_sections;
    }

    return ret;
}

bool QuickenInterface::FindEntry(uint32_t pc, uint64_t* entry_offset) {
    size_t first = 0;
    size_t last = qut_sections_->idx_size;

    while (first < last) {
        size_t current = ((first + last) / 2) & 0xfffffffe;
        uint32_t addr = qut_sections_->quidx[current];
        if (pc == addr) {
            *entry_offset = current;
            return true;
        }
        if (pc < addr) {
            last = current;
        } else {
            first = current + 2;
        }
    }
    if (last != 0) {
        *entry_offset = last - 2;
        return true;
    }
    last_error_code_ = QUT_ERROR_UNWIND_INFO;
    return false;
}

bool QuickenInterface::Step(uint64_t pc, uintptr_t* regs, unwindstack::Memory* process_memory, bool* finished) {

    // Adjust the load bias to get the real relative pc.
    if (UNLIKELY(pc < load_bias_)) {
        last_error_code_ = QUT_ERROR_UNWIND_INFO;
        return false;
    }

    if (!qut_sections_) {
        if (GenerateQuickenTable(process_memory)) {
            last_error_code_ = QUT_ERROR_QUT_SECTION_INVALID;
            return false;
        }
    }

    QuickenTable quicken(qut_sections_.get(), regs, memory_, process_memory);
    uint64_t entry_offset;

    pc -= load_bias_;

    if (UNLIKELY(!FindEntry(pc, &entry_offset))) {
        return false;
    }

    quicken.cfa_ = SP(regs);
    bool return_value = false;

    if (quicken.Eval(entry_offset)) {
        if (!quicken.pc_set_) {
            PC(regs) = LR(regs);
        }
        SP(regs) = quicken.cfa_;
        return_value = true;
    } else {
        last_error_code_ = QUT_ERROR_INVALID_QUT_INSTR;
    }

    // If the pc was set to zero, consider this the final frame.
    *finished = (PC(regs) == 0) ? true : false;

    return return_value;
}

}  // namespace unwindstack
