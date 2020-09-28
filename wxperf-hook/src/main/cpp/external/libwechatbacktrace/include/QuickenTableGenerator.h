//
// Created by Carl on 2020-09-21.
//

#ifndef _LIBUNWINDSTACK_WECHAT_QUICKEN_UNWIND_TABLE_GENERATOR_H
#define _LIBUNWINDSTACK_WECHAT_QUICKEN_UNWIND_TABLE_GENERATOR_H

#include <deque>

#include "../../common/Log.h"
#include "../../libunwindstack/ArmExidx.h"
#include "Errors.h"

namespace wechat_backtrace {

enum QutInstruction : uint32_t {
    QUT_INSTRUCTION_R7_OFFSET = 0,  // 0
    QUT_INSTRUCTION_R11_OFFSET,     // 1
    QUT_INSTRUCTION_SP_OFFSET,      // 2
    QUT_INSTRUCTION_LR_OFFSET,      // 3
    QUT_INSTRUCTION_PC_OFFSET,      // 4
    QUT_INSTRUCTION_VSP_OFFSET,     // 5
    QUT_INSTRUCTION_VSP_SET_BY_R7,  // 6
    QUT_INSTRUCTION_VSP_SET_BY_R11, // 7
    QUT_INSTRUCTION_VSP_SET_BY_SP,  // 8
    FLUSH
};

struct QutSections {
    uint32_t* quidx;
    uint32_t* qutbl;

    size_t idx_size;
    size_t tbl_size;

    size_t idx_capacity;
    size_t tbl_capacity;

    size_t total_entries;
    uint64_t start_offset_ = 0;
};

struct ExidxContext {

public:
    int32_t vsp_ = 0;
    int32_t transformed_bits = 0;
    int32_t regs_[5] = {0};

    void reset() {
        vsp_ = 0;
        transformed_bits = 0;
        memset(regs_, 0, sizeof(int32_t) * 5);
    }

    void AddUpTransformed(uint32_t reg_idx, int32_t imm) {
        if (transformed_bits & (1 << reg_idx)) {
            regs_[reg_idx] += imm;
        }
    }

    void AddUpVSP(int32_t imm) {
        vsp_ += imm;

        AddUpTransformed(QUT_INSTRUCTION_R7_OFFSET, imm);
        AddUpTransformed(QUT_INSTRUCTION_R11_OFFSET, imm);
        AddUpTransformed(QUT_INSTRUCTION_SP_OFFSET, imm);
        AddUpTransformed(QUT_INSTRUCTION_LR_OFFSET, imm);
        AddUpTransformed(QUT_INSTRUCTION_PC_OFFSET, imm);
    }

    void Transform(uint32_t reg_idx) {
        transformed_bits = transformed_bits | (1 << reg_idx);
        regs_[reg_idx] = 0;
    }

};

class ExidxDecoder {

public:
    ExidxDecoder(unwindstack::Memory* memory, unwindstack::Memory* process_memory)
        : memory_(memory), process_memory_(process_memory) {
        instructions_.reset(new std::deque<uint64_t>);
    }
    ~ExidxDecoder() {};

    bool ExtractEntryData(uint32_t entry_offset);
    bool Eval();

    std::unique_ptr<std::deque<uint64_t>> instructions_;

    unwindstack::ArmStatus status_ = unwindstack::ARM_STATUS_NONE;
    uint64_t status_address_ = 0;

protected:
    unwindstack::Memory* memory_;
    unwindstack::Memory* process_memory_;

    bool DecodePrefix_10_00(uint8_t byte);
    bool DecodePrefix_10_01(uint8_t byte);
    bool DecodePrefix_10_10(uint8_t byte);
    bool DecodePrefix_10_11_0000();
    bool DecodePrefix_10_11_0001();
    bool DecodePrefix_10_11_0010();
    bool DecodePrefix_10_11_0011();
    bool DecodePrefix_10_11_01nn();
    bool DecodePrefix_10_11_1nnn(uint8_t byte);
    bool DecodePrefix_10(uint8_t byte);

    bool DecodePrefix_11_000(uint8_t byte);
    bool DecodePrefix_11_001(uint8_t byte);
    bool DecodePrefix_11_010(uint8_t byte);
    bool DecodePrefix_11(uint8_t byte);

    bool Decode();
    bool GetByte(uint8_t* byte);

    void FlushInstructions();
    void SaveInstructions(QutInstruction instruction);

    std::deque<uint8_t> data_;

    // context
    ExidxContext context_;

};

enum QutStatisticType : uint32_t {
    InstructionOp = 0,
    UnsupportArmExdix,
};
typedef void(*QutStatisticInstruction)(QutStatisticType, uint32_t, uint32_t);

QutStatisticInstruction GetQutStatistic();
void SetQutStatistic(QutStatisticInstruction statistic_func);

class QuickenTableGenerator {

public:
    QuickenTableGenerator(unwindstack::Memory* memory, unwindstack::Memory* process_memory)
        : memory_(memory), process_memory_(process_memory) {}
    ~QuickenTableGenerator() {};

    bool GenerateFutSections(size_t start_offset, size_t total_entry, QutSections* fut_sections,
            size_t &bad_entries_count);

    QutErrorCode last_error_code;

protected:
    bool GetPrel31Addr(uint32_t offset, uint32_t* addr);

    bool EncodeInstruction(std::deque<uint64_t> &instructions, std::deque<uint8_t > &encoded);
    bool CheckInstruction(uint64_t instruction);

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

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_WECHAT_QUICKEN_UNWIND_TABLE_GENERATOR_H
