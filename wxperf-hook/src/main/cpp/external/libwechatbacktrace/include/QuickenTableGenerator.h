//
// Created by Carl on 2020-09-21.
//

#ifndef _LIBUNWINDSTACK_WECHAT_QUICKEN_UNWIND_TABLE_GENERATOR_H
#define _LIBUNWINDSTACK_WECHAT_QUICKEN_UNWIND_TABLE_GENERATOR_H

#include <deque>

#include "../../common/Log.h"
#include "../../libunwindstack/ArmExidx.h"
#include "Errors.h"
#include "../DwarfEhFrameWithHdrDecoder.h"
#include "MinimalRegs.h"
#include "QuickenInstructions.h"

namespace wechat_backtrace {

struct FrameInfo {
    uint64_t offset_ = 0;
    int64_t section_bias_ = 0;
    uint64_t size_ = 0;
};

struct QutSections {

    QutSections() = default;
    ~QutSections() {
        delete quidx;
        delete qutbl;

        idx_size = 0;
        tbl_size = 0;

        idx_capacity = 0;
        tbl_capacity = 0;
    }

    uptr* quidx = nullptr;
    uptr* qutbl = nullptr;

    size_t idx_size = 0;
    size_t tbl_size = 0;

    size_t idx_capacity = 0;
    size_t tbl_capacity = 0;

    size_t total_entries = 0;
    uint64_t start_offset_ = 0;
};

template <typename AddressType>
class QuickenTableGenerator {

public:
    QuickenTableGenerator(unwindstack::Memory* memory, unwindstack::Memory* process_memory)
        : memory_(memory), process_memory_(process_memory) {}
    ~QuickenTableGenerator() {};

    bool GenerateFutSections(size_t start_offset, size_t total_entry, QutSections* fut_sections,
            size_t &bad_entries_count);

    bool GenerateUltraQUTSections(
            FrameInfo eh_frame_hdr_info, FrameInfo eh_frame_info, FrameInfo debug_frame_info,
            FrameInfo gnu_eh_frame_hdr_info, FrameInfo gnu_eh_frame_info,
            FrameInfo gnu_debug_frame_info,
            FrameInfo arm_exidx_info, QutSections* fut_sections);

//    bool QuickenTableGenerator::GenerateDwarfBasedQUTSections(
//            FrameInfo eh_frame_hdr_info, FrameInfo eh_frame_info, FrameInfo debug_frame_info,
//            FrameInfo gnu_eh_frame_hdr_info, FrameInfo gnu_eh_frame_info, FrameInfo gnu_debug_frame_info,
//            QutSections* fut_sections);

    QutErrorCode last_error_code;

protected:
    void DecodeDebugFrameEntriesInstr(FrameInfo debug_frame_info,
            QutInstructionsOfEntries* entries_instructions, uint16_t regs_total);
    void DecodeEhFrameEntriesInstr(FrameInfo eh_frame_hdr_info, FrameInfo eh_frame_info,
            QutInstructionsOfEntries* entries_instructions, uint16_t regs_total);
    void DecodeExidxEntriesInstr(FrameInfo arm_exidx_info, QutInstructionsOfEntries* entries_instructions);

    std::shared_ptr<QutInstructionsOfEntries> MergeFrameEntries(
            std::shared_ptr<QutInstructionsOfEntries> to, std::shared_ptr<QutInstructionsOfEntries> from);

    bool PackEntriesToFutSections(
            QutInstructionsOfEntries* entries, QutSections* fut_sections);

    bool GetPrel31Addr(uint32_t offset, uint32_t* addr);

    unwindstack::Memory* memory_;
    unwindstack::Memory* process_memory_;
};

class QuickenTable {

public:
    QuickenTable(QutSections* fut_sections, uintptr_t* regs, unwindstack::Memory* memory,
            unwindstack::Memory* process_memory) : regs_(regs), memory_(memory),
            process_memory_(process_memory), fut_sections_(fut_sections) {}
    ~QuickenTable() {};

    bool Eval(uint32_t entry_offset);

    uint32_t cfa_ = 0;
    uint32_t dex_pc_ = 0;
    bool pc_set_ = false;

protected:

    bool Decode(uint32_t* instructions, const size_t amount, size_t start_pos = 3);

    uintptr_t* regs_ = nullptr;

    unwindstack::Memory* memory_ = nullptr;
    unwindstack::Memory* process_memory_ = nullptr;
    QutSections* fut_sections_ = nullptr;

//    status TODO
    unwindstack::ArmStatus status_ = unwindstack::ARM_STATUS_NONE;
};

}  // namespace wechat_backtrace

#endif  // _LIBUNWINDSTACK_WECHAT_QUICKEN_UNWIND_TABLE_GENERATOR_H
