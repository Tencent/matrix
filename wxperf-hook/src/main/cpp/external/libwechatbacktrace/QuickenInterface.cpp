//
// Created by Carl on 2020-09-21.
//

#include <deps/android-base/include/android-base/macros.h>
#include <MinimalRegs.h>
#include "../../common/Log.h"
#include "QuickenInterface.h"
#include "QuickenMaps.h"
#include "DwarfEhFrameWithHdrDecoder.h"
#include "DwarfEhFrameDecoder.h"
#include "DwarfDebugFrameDecoder.h"

namespace wechat_backtrace {

using namespace std;
using namespace unwindstack;

uint64_t QuickenInterface::GetLoadBias() {
    return load_bias_;
}

template <typename AddressType>
bool QuickenInterface::GenerateQuickenTableUltra(unwindstack::Memory* process_memory) {

    lock_guard<mutex> lock(lock_);

    if (qut_sections_) {
        return true;
    }

    QUT_DEBUG_LOG("QuickenInterface::GenerateQuickenTableUltra");

    QuickenTableGenerator<AddressType> generator(memory_, process_memory);
    QutSections* qut_sections = new QutSections();

    size_t bad_entries = 0; // TODO

    bool ret = generator.GenerateUltraQUTSections(eh_frame_hdr_info_, eh_frame_info_,
            debug_frame_info_, gnu_eh_frame_hdr_info_, gnu_eh_frame_info_, gnu_debug_frame_info_,
            arm_exidx_info_, qut_sections);

    bad_entries_ = bad_entries;

    if (ret) {
        qut_sections_.reset(qut_sections);
    } else {
        delete qut_sections;
    }

    return ret;
}

bool QuickenInterface::GenerateQuickenTable(unwindstack::Memory* process_memory) {

    lock_guard<mutex> lock(lock_);

    if (qut_sections_) {
        return true;
    }

    QuickenTableGenerator<uint32_t> generator(memory_, process_memory);
    QutSections* qut_sections = new QutSections();

    size_t bad_entries = 0;
    bool ret = generator.GenerateFutSections(arm_exidx_info_.offset_, arm_exidx_info_.size_,
            qut_sections, bad_entries);

    bad_entries_ = bad_entries;

    if (ret) {
        qut_sections_.reset(qut_sections);
    } else {
        delete qut_sections;
    }

    return ret;
}

bool QuickenInterface::FindEntry(uptr pc, uint64_t* entry_offset) {
    size_t first = 0;
    size_t last = qut_sections_->idx_size;

    while (first < last) {
        size_t current = ((first + last) / 2) & 0xfffffffe;
        uptr addr = qut_sections_->quidx[current];
//        QUT_DEBUG_LOG("QuickenInterface::FindEntry first:%u last:%u current:%u, addr:%x, pc:%x", first, last, current, addr, pc);
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
        QUT_DEBUG_LOG("QuickenInterface::FindEntry found entry_offset: %u, addr: %x", last - 2, qut_sections_->quidx[last - 2]);
        *entry_offset = last - 2;
        return true;
    }
    last_error_code_ = QUT_ERROR_UNWIND_INFO;
    return false;
}

bool QuickenInterface::Step(uptr pc, uptr* regs, unwindstack::Memory* process_memory, bool* finished) {

    // Adjust the load bias to get the real relative pc.
    if (UNLIKELY(pc < load_bias_)) {
        last_error_code_ = QUT_ERROR_UNWIND_INFO;
        return false;
    }

    if (!qut_sections_) {
        if (!GenerateQuickenTableUltra<addr_t>(process_memory)) {
            last_error_code_ = QUT_ERROR_QUT_SECTION_INVALID;
            return false;
        }
    }

    QuickenTable quicken(qut_sections_.get(), regs, memory_, process_memory);
    uint64_t entry_offset;

    QUT_DEBUG_LOG("QuickenInterface::Step pc:%x, load_bias_:%llu", pc, load_bias_);

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
