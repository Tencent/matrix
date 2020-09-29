//
// Created by Carl on 2020-09-21.
//

#include <cstdint>
#include <deps/android-base/include/android-base/logging.h>
#include <include/unwindstack/MachineArm.h>
#include <include/unwindstack/Memory.h>
#include <vector>
#include <utility>
#include "QuickenTableGenerator.h"
#include "MinimalRegs.h"
#include "../../common/Log.h"

// WeChat Quicken Unwind Table encode:
//      00nn nnnn           : vsp = vsp + (nnnnnn << 2)                     ; # (nnnnnnn << 2) in [0, 0xfc]
//      01nn nnnn           : vsp = vsp - (nnnnnn << 2)                     ; # (nnnnnnn << 2) in [0, 0xfc]
//      1000 0000           : vsp = r7                                      ;
//      1000 0001           : vsp = r7 + 8, ls = [vsp - 4], sp = [vsp - 8]  ; # have prologue
//      1001 0000           : vsp = r11                                     ;
//      1001 0001           : vsp = r11 + 8, ls = [vsp - 4], sp = [vsp - 8] ; # have prologue
//      1010 0000           : vsp = [sp]                                    ;
//      1100 nnnn           : r7 = [vsp - (nnnn << 2)]                      ; # (nnnn << 2) in [0, 0x3c]
//      1101 nnnn           : r11 = [vsp - (nnnn << 2)]                     ; # (nnnn << 2) in [0, 0x3c]
//      1110 nnnn           : lr = [vsp - (nnnn << 2)]                      ; # (nnnn << 2) in [0, 0x3c]
//      1111 0000           : Finish                                        ;
//      1111 1001 + SLEB128 : sp = [vsp - SLEB128]                          ; # [addr] means get value from pointed by addr
//      1111 1010 + SLEB128 : lr = [vsp - SLEB128]                          ; # [addr] means get value from pointed by addr
//      1111 1011 + SLEB128 : pc = [vsp - SLEB128]                          ; # [addr] means get value from pointed by addr
//      1111 1100 + SLEB128 : r7 = [vsp - SLEB128]                          ; # [addr] means get value from pointed by addr
//      1111 1101 + SLEB128 : r11 = [vsp - SLEB128]                         ; # [addr] means get value from pointed by addr
//      1111 1111 + SLEB128 : vsp = vsp + SLEB128                           ;

#define FUT_FINISH 0xf0

namespace wechat_backtrace {

using namespace std;
using namespace unwindstack;


static QutStatisticInstruction statistic_;

QutStatisticInstruction GetQutStatistic() {
    return statistic_;
}

void SetQutStatistic(QutStatisticInstruction statistic_func) {
    statistic_ = statistic_func;
}

void ExidxDecoder::FlushInstructions() {

    if (context_.vsp_ != 0) {
        CHECK((context_.vsp_ & 0x3) == 0);
        uint64_t instr = (((uint64_t)QUT_INSTRUCTION_VSP_OFFSET) << 32) | context_.vsp_;
        instructions_->push_back(instr);
    }

    vector<pair<size_t, int32_t>> sorted_regs;

    // sort by vsp offset ascend, convenient for search prologue later.
    for (size_t i = 0; i < 5; i++) {
        if (!(context_.transformed_bits & (1 << i))) {
            continue;
        }
        int32_t vsp_offset = context_.regs_[i];
        size_t insert_pos = 0;
        for (size_t j = 0; j < sorted_regs.size(); j++) {
            if (vsp_offset >= 0 &&
                (sorted_regs.at(j).second > vsp_offset || sorted_regs.at(j).second < 0)) {
                break;
            }
            insert_pos = j + 1;
        }
        sorted_regs.insert(sorted_regs.begin() + insert_pos, make_pair(i, vsp_offset));
    }

    for (size_t j = 0; j < sorted_regs.size(); j++) {
        uint32_t FUT_INSTR = (sorted_regs.at(j).first);
        if (context_.transformed_bits & (1 << FUT_INSTR)) {
            CHECK((context_.regs_[FUT_INSTR] & 0x3) == 0);
            uint64_t instr = (((uint64_t)FUT_INSTR) << 32) | context_.regs_[FUT_INSTR];
            instructions_->push_back(instr);
        }
    }

    if (context_.transformed_bits & (1 << QUT_INSTRUCTION_SP_OFFSET)) {
        uint64_t instr = (((uint64_t)QUT_INSTRUCTION_VSP_SET_BY_SP) << 32);
        instructions_->push_back(instr);
    }

    // reset
    context_.reset();
}

void ExidxDecoder::SaveInstructions(QutInstruction instruction) {

    FlushInstructions();

    if (instruction == QUT_INSTRUCTION_VSP_SET_BY_R7 ||
            instruction == QUT_INSTRUCTION_VSP_SET_BY_R11) {
        instructions_->push_back((((uint64_t)instruction) << 32));
    }
}

inline bool ExidxDecoder::GetByte(uint8_t* byte) {
    if (data_.empty()) {
        return false;
    }
    *byte = data_.front();
    data_.pop_front();
    return true;
}

bool ExidxDecoder::Decode() {

    uint8_t byte;
    if (!GetByte(&byte)) {
        return false;
    }

    switch (byte >> 6) {
        case 0:
            // 00xxxxxx: vsp = vsp + (xxxxxxx << 2) + 4
            context_.AddUpVSP(((byte & 0x3f) << 2) + 4);
            break;
        case 1:
            // 01xxxxxx: vsp = vsp - (xxxxxxx << 2) + 4
            context_.AddUpVSP(-(((byte & 0x3f) << 2) + 4));
            break;
        case 2:
            return DecodePrefix_10(byte);
        default:
            return DecodePrefix_11(byte);
    }
    return true;
}

bool ExidxDecoder::Eval() {

    while (Decode());

    SaveInstructions(FLUSH);

    return status_ == ARM_STATUS_FINISH;
}

bool ExidxDecoder::ExtractEntryData(uint32_t entry_offset) {
    data_.clear();

    status_ = ARM_STATUS_NONE;

    status_address_ = entry_offset;

    if (entry_offset & 1) {
        // The offset needs to be at least two byte aligned.
        status_ = ARM_STATUS_INVALID_ALIGNMENT;
        return false;
    }

    // Each entry is a 32 bit prel31 offset followed by 32 bits
    // of unwind information. If bit 31 of the unwind data is zero,
    // then this is a prel31 offset to the start of the unwind data.
    // If the unwind data is 1, then this is a cant unwind entry.
    // Otherwise, this data is the compact form of the unwind information.
    uint32_t data;
    if (!memory_->Read32(entry_offset + 4, &data)) {
        status_ = ARM_STATUS_READ_FAILED;
        status_address_ = entry_offset + 4;
        return false;
    }

    if (data == 1) {
        // This is a CANT UNWIND entry.
        status_ = ARM_STATUS_NO_UNWIND;
        return false;
    }

    if (data & (1UL << 31)) {
        // This is a compact table entry.
        if ((data >> 24) & 0xf) {
            // This is a non-zero index, this code doesn't support
            // other formats.
            status_ = ARM_STATUS_INVALID_PERSONALITY;
            return false;
        }
        data_.push_back((data >> 16) & 0xff);
        data_.push_back((data >> 8) & 0xff);
        uint8_t last_op = data & 0xff;
        data_.push_back(last_op);
        if (last_op != ARM_OP_FINISH) {
            // If this didn't end with a finish op, add one.
            data_.push_back(ARM_OP_FINISH);
        }
        return true;
    }

    // Get the address of the ops.
    // Sign extend the data value if necessary.
    int32_t signed_data = static_cast<int32_t>(data << 1) >> 1;
    uint32_t addr = (entry_offset + 4) + signed_data;
    if (!memory_->Read32(addr, &data)) {
        status_ = ARM_STATUS_READ_FAILED;
        status_address_ = addr;
        return false;
    }

    size_t num_table_words;
    if (data & (1UL << 31)) {
        // Compact model.
        switch ((data >> 24) & 0xf) {
            case 0:
                num_table_words = 0;
                data_.push_back((data >> 16) & 0xff);
                break;
            case 1:
            case 2:
                num_table_words = (data >> 16) & 0xff;
                addr += 4;
                break;
            default:
                // Only a personality of 0, 1, 2 is valid.
                status_ = ARM_STATUS_INVALID_PERSONALITY;
                return false;
        }
        data_.push_back((data >> 8) & 0xff);
        data_.push_back(data & 0xff);
    } else {
        // Generic model.

        // Skip the personality routine data, it doesn't contain any data
        // needed to decode the unwind information.
        addr += 4;
        if (!memory_->Read32(addr, &data)) {
            status_ = ARM_STATUS_READ_FAILED;
            status_address_ = addr;
            return false;
        }

        num_table_words = (data >> 24) & 0xff;
        data_.push_back((data >> 16) & 0xff);
        data_.push_back((data >> 8) & 0xff);
        data_.push_back(data & 0xff);
        addr += 4;
    }

    if (num_table_words > 5) {
        status_ = ARM_STATUS_MALFORMED;
        return false;
    }

    for (size_t i = 0; i < num_table_words; i++) {
        if (!memory_->Read32(addr, &data)) {
            status_ = ARM_STATUS_READ_FAILED;
            status_address_ = addr;
            return false;
        }

        data_.push_back((data >> 24) & 0xff);
        data_.push_back((data >> 16) & 0xff);
        data_.push_back((data >> 8) & 0xff);
        data_.push_back(data & 0xff);
        addr += 4;
    }

    if (data_.back() != ARM_OP_FINISH) {
        // If this didn't end with a finish op, add one.
        data_.push_back(ARM_OP_FINISH);
    }
    return true;
}

inline bool ExidxDecoder::DecodePrefix_10_00(uint8_t byte) {

    uint16_t registers = (byte & 0xf) << 8;
    if (!GetByte(&byte)) {
        return false;
    }

    registers |= byte;
    if (registers == 0) {
        // 10000000 00000000: Refuse to unwind
        return false;
    }
    // 1000iiii iiiiiiii: Pop up to 12 integer registers under masks {r15-r12}, {r11-r4}
    registers <<= 4;

    for (size_t reg = ARM_REG_R4; reg < ARM_REG_R7; reg++) {
        if (registers & (1 << reg)) {
            context_.AddUpVSP(4);
        }
    }

    // r7
    if (registers & (1 << ARM_REG_R7)) {
        context_.Transform(QUT_INSTRUCTION_R7_OFFSET);
        context_.AddUpVSP(4);
    }

    for (size_t reg = ARM_REG_R8; reg < ARM_REG_R11; reg++) {
        if (registers & (1 << reg)) {
            context_.AddUpVSP(4);
        }
    }

    // r11
    if (registers & (1 << ARM_REG_R11)) {
        context_.Transform(QUT_INSTRUCTION_R11_OFFSET);
        context_.AddUpVSP(4);
    }

    if (registers & (1 << ARM_REG_R12)) {
        context_.AddUpVSP(4);
    }

    // If the sp register is modified, change the cfa value.
    if (registers & (1 << ARM_REG_SP)) {
        context_.Transform(QUT_INSTRUCTION_SP_OFFSET);
        context_.AddUpVSP(4);
    }

    // lr
    if (registers & (1 << ARM_REG_LR)) {
        context_.Transform(QUT_INSTRUCTION_LR_OFFSET);
        context_.AddUpVSP(4);
    }

    // PC
    if (registers & (1 << ARM_REG_PC)) {
        context_.Transform(QUT_INSTRUCTION_PC_OFFSET);
        context_.AddUpVSP(4);
    }

    if (registers & (1 << ARM_REG_SP)) {
        // sp register changed, save instructions.
        SaveInstructions(FLUSH);
    }

    return true;
}

inline bool ExidxDecoder::DecodePrefix_10_01(uint8_t byte) {
    CHECK((byte >> 4) == 0x9);

    uint8_t bits = byte & 0xf;
    if (bits == 13 || bits == 15) {
        // 10011101: Reserved as prefix for ARM register to register moves
        // 10011111: Reserved as prefix for Intel Wireless MMX register to register moves
        status_ = ARM_STATUS_RESERVED;
        return false;
    }

    // 1001nnnn: Set vsp = r[nnnn] (nnnn != 13, 15)

    // It is impossible for bits to be larger than the total number of
    // arm registers, so don't bother checking if bits is a valid register.

    if (bits == ARM_REG_R7) {   // r7
        if (context_.transformed_bits == 0) {
            // No register transformed, ignore vsp offset.
            context_.reset();
        }
        SaveInstructions(QUT_INSTRUCTION_VSP_SET_BY_R7);
    } else if (bits == ARM_REG_R11) {   // r11
        if (context_.transformed_bits == 0) {
            // No register transformed, ignore vsp offset.
            context_.reset();
        }
        SaveInstructions(QUT_INSTRUCTION_VSP_SET_BY_R11);
    } else {
        if (statistic_) {
            statistic_(UnsupportArmExdix, bits, byte);
        }
        return false;
    }

    return true;
}

inline bool ExidxDecoder::DecodePrefix_10_10(uint8_t byte) {
    CHECK((byte >> 4) == 0xa);

    // 10100nnn: Pop r4-r[4+nnn]
    // 10101nnn: Pop r4-r[4+nnn], r14

    for (size_t i = 4; i <= 4 + (byte & 0x7); i++) {
        if (i == ARM_REG_R7) {  // r7
            context_.Transform(QUT_INSTRUCTION_R7_OFFSET);
        } else if (i == ARM_REG_R11) {  // r11
            context_.Transform(QUT_INSTRUCTION_R11_OFFSET);
        }

        context_.AddUpVSP(4);
    }
    if (byte & 0x8) {
        context_.Transform(QUT_INSTRUCTION_LR_OFFSET);
        context_.AddUpVSP(4);
    }
    return true;
}

inline bool ExidxDecoder::DecodePrefix_10_11_0000() {
    // 10110000: Finish
    status_ = ARM_STATUS_FINISH;
    return false;
}

inline bool ExidxDecoder::DecodePrefix_10_11_0001() {
    uint8_t byte;
    if (!GetByte(&byte)) {
        return false;
    }

    if (byte == 0) {
        // 10110001 00000000: Spare
        status_ = ARM_STATUS_SPARE;
        return false;
    }
    if (byte >> 4) {
        // 10110001 xxxxyyyy: Spare (xxxx != 0000)
        status_ = ARM_STATUS_SPARE;
        return false;
    }

    // 10110001 0000iiii: Pop integer registers under mask {r3, r2, r1, r0}

    for (size_t reg = 0; reg < 4; reg++) {
        if (byte & (1 << reg)) {
            context_.AddUpVSP(4);
        }
    }
    return true;
}

inline bool ExidxDecoder::DecodePrefix_10_11_0010() {
    // 10110010 uleb128: vsp = vsp + 0x204 + (uleb128 << 2)
    uint32_t result = 0;
    uint32_t shift = 0;
    uint8_t byte;
    do {
        if (!GetByte(&byte)) {
            return false;
        }

        result |= (byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);
    result <<= 2;
    context_.AddUpVSP(0x204 + result);
    return true;
}

inline bool ExidxDecoder::DecodePrefix_10_11_0011() {
    // 10110011 sssscccc: Pop VFP double precision registers D[ssss]-D[ssss+cccc] by FSTMFDX
    uint8_t byte;
    if (!GetByte(&byte)) {
        return false;
    }

    context_.AddUpVSP((byte & 0xf) * 8 + 12);
    return true;
}

inline bool ExidxDecoder::DecodePrefix_10_11_01nn() {
    // 101101nn: Spare
    status_ = ARM_STATUS_SPARE;
    return false;
}

inline bool ExidxDecoder::DecodePrefix_10_11_1nnn(uint8_t byte) {
    CHECK((byte & ~0x07) == 0xb8);

    // 10111nnn: Pop VFP double-precision registers D[8]-D[8+nnn] by FSTMFDX
    // Only update the cfa.
    context_.AddUpVSP((byte & 0x7) * 8 + 12);
    return true;
}

inline bool ExidxDecoder::DecodePrefix_11_000(uint8_t byte) {
    CHECK((byte & ~0x07) == 0xc0);

    uint8_t bits = byte & 0x7;
    if (bits == 6) {
        if (!GetByte(&byte)) {
            return false;
        }

        // 11000110 sssscccc: Intel Wireless MMX pop wR[ssss]-wR[ssss+cccc]
        // Only update the cfa.
        context_.AddUpVSP((byte & 0xf) * 8 + 8);
    } else if (bits == 7) {
        if (!GetByte(&byte)) {
            return false;
        }

        if (byte == 0) {
            // 11000111 00000000: Spare
            status_ = ARM_STATUS_SPARE;
            return false;
        } else if ((byte >> 4) == 0) {
            // 11000111 0000iiii: Intel Wireless MMX pop wCGR registers {wCGR0,1,2,3}
            // Only update the cfa.
            context_.AddUpVSP(__builtin_popcount(byte) * 4);
        } else {
            // 11000111 xxxxyyyy: Spare (xxxx != 0000)
            status_ = ARM_STATUS_SPARE;
            return false;
        }
    } else {
        // 11000nnn: Intel Wireless MMX pop wR[10]-wR[10+nnn] (nnn != 6, 7)
        // Only update the cfa.
        context_.AddUpVSP((byte & 0x7) * 8 + 8);
    }
    return true;
}

inline bool ExidxDecoder::DecodePrefix_11_001(uint8_t byte) {
    CHECK((byte & ~0x07) == 0xc8);

    uint8_t bits = byte & 0x7;
    if (bits == 0) {
        // 11001000 sssscccc: Pop VFP double precision registers D[16+ssss]-D[16+ssss+cccc] by VPUSH
        if (!GetByte(&byte)) {
            return false;
        }

        // Only update the cfa.
        context_.AddUpVSP((byte & 0xf) * 8 + 8);
    } else if (bits == 1) {
        // 11001001 sssscccc: Pop VFP double precision registers D[ssss]-D[ssss+cccc] by VPUSH
        if (!GetByte(&byte)) {
            return false;
        }

        // Only update the cfa.
        context_.AddUpVSP((byte & 0xf) * 8 + 8);
    } else {
        // 11001yyy: Spare (yyy != 000, 001)
        status_ = ARM_STATUS_SPARE;
        return false;
    }
    return true;
}

inline bool ExidxDecoder::DecodePrefix_11_010(uint8_t byte) {
    CHECK((byte & ~0x07) == 0xd0);

    // 11010nnn: Pop VFP double precision registers D[8]-D[8+nnn] by VPUSH
    context_.AddUpVSP((byte & 0x7) * 8 + 8);
    return true;
}

inline bool ExidxDecoder::DecodePrefix_10(uint8_t byte) {

    switch ((byte >> 4) & 0x3) {
        case 0:
            return DecodePrefix_10_00(byte);
        case 1:
            return DecodePrefix_10_01(byte);
        case 2:
            return DecodePrefix_10_10(byte);
        default:
            switch (byte & 0xf) {
                case 0:
                    return DecodePrefix_10_11_0000();
                case 1:
                    return DecodePrefix_10_11_0001();
                case 2:
                    return DecodePrefix_10_11_0010();
                case 3:
                    return DecodePrefix_10_11_0011();
                default:
                    if (byte & 0x8) {
                        return DecodePrefix_10_11_1nnn(byte);
                    } else {
                        return DecodePrefix_10_11_01nn();
                    }
            }
    }
}

inline bool ExidxDecoder::DecodePrefix_11(uint8_t byte) {

    switch ((byte >> 3) & 0x7) {
        case 0:
            return DecodePrefix_11_000(byte);
        case 1:
            return DecodePrefix_11_001(byte);
        case 2:
            return DecodePrefix_11_010(byte);
        default:
            // 11xxxyyy: Spare (xxx != 000, 001, 010)
            return false;
    }
}

inline unsigned encodeSLEB128(int64_t Value, uint8_t *p, unsigned PadTo = 0) {
    uint8_t *orig_p = p;
    unsigned Count = 0;
    bool More;
    do {
        uint8_t Byte = Value & 0x7f;
        // NOTE: this assumes that this signed shift is an arithmetic right shift.
        Value >>= 7;
        More = !((((Value == 0 ) && ((Byte & 0x40) == 0)) ||
                  ((Value == -1) && ((Byte & 0x40) != 0))));
        Count++;
        if (More || Count < PadTo)
            Byte |= 0x80; // Mark this byte to show that more bytes will follow.
        *p++ = Byte;
    } while (More);

    // Pad with 0x80 and emit a terminating byte at the end.
    if (Count < PadTo) {
        uint8_t PadValue = Value < 0 ? 0x7f : 0x00;
        for (; Count < PadTo - 1; ++Count)
            *p++ = (PadValue | 0x80);
        *p++ = PadValue;
    }
    return (unsigned)(p - orig_p);
}

inline bool QuickenTableGenerator::CheckInstruction(uint64_t instruction) {

    int32_t op = instruction >> 32;
    int32_t imm = instruction & 0xFFFFFFFF;

    if (op == QUT_INSTRUCTION_VSP_OFFSET) {
        if (imm < 0 || imm > 0xfc || imm < -0xfc) {
            if (statistic_) {
                statistic_(InstructionOp, QUT_INSTRUCTION_VSP_OFFSET, imm);
            }
        }
    } else if (op == QUT_INSTRUCTION_R7_OFFSET) {
        if (imm < 0 || imm > 0x3c) {
            if (statistic_) {
                statistic_(InstructionOp, QUT_INSTRUCTION_R7_OFFSET, imm);
            }
        }
    } else if (op == QUT_INSTRUCTION_R11_OFFSET) {
        if (imm < 0 || imm > 0x3c) {
            if (statistic_) {
                statistic_(InstructionOp, QUT_INSTRUCTION_R11_OFFSET, imm);
            }
        }
    }

    return true;
}

#define EncodeSLEB128_PUSHBACK(EncodedQueue, IMM) \
    uint8_t encoded_bytes[5]; \
    size_t size = encodeSLEB128(IMM, encoded_bytes); \
    for (size_t i = 0; i < size; i++) { \
        EncodedQueue.push_back(encoded_bytes[i]); \
    }

bool QuickenTableGenerator::EncodeInstruction(std::deque<uint64_t> &instructions, std::deque<uint8_t > &encoded) {

// FUT encode:
//      00nn nnnn           : vsp = vsp + (nnnnnn << 2)                     ; # (nnnnnnn << 2) in [0, 0xfc]
//      01nn nnnn           : vsp = vsp - (nnnnnn << 2)                     ; # (nnnnnnn << 2) in [0, 0xfc]
//      1000 0000           : vsp = r7                                      ;
//      1000 0001           : vsp = r7 + 8, ls = [vsp - 4], sp = [vsp - 8]  ; # have prologue
//      1001 0000           : vsp = r11                                     ;
//      1001 0001           : vsp = r11 + 8, ls = [vsp - 4], sp = [vsp - 8] ; # have prologue
//      1010 0000           : vsp = [sp]                                    ;
//      1100 nnnn           : r7 = [vsp - (nnnn << 2)]                      ; # (nnnn << 2) in [0, 0x3c]
//      1101 nnnn           : r11 = [vsp - (nnnn << 2)]                     ; # (nnnn << 2) in [0, 0x3c]
//      1110 nnnn           : lr = [vsp - (nnnn << 2)]                      ; # (nnnn << 2) in [0, 0x3c]
//      1111 0000           : Finish                                        ;
//      1111 1001 + SLEB128 : sp = [vsp - SLEB128]                          ; # [addr] means get value from pointed by addr
//      1111 1010 + SLEB128 : lr = [vsp - SLEB128]                          ; # [addr] means get value from pointed by addr
//      1111 1011 + SLEB128 : pc = [vsp - SLEB128]                          ; # [addr] means get value from pointed by addr
//      1111 1100 + SLEB128 : r7 = [vsp - SLEB128]                          ; # [addr] means get value from pointed by addr
//      1111 1101 + SLEB128 : r11 = [vsp - SLEB128]                         ; # [addr] means get value from pointed by addr
//      1111 1111 + SLEB128 : vsp = vsp + SLEB128                           ;

    while (!instructions.empty()) {

        uint64_t instruction = instructions.front();

        instructions.pop_front();

        CheckInstruction(instruction);

        int32_t op = instruction >> 32;
        int32_t imm = instruction & 0xFFFFFFFF;

        if (op == QUT_INSTRUCTION_VSP_OFFSET) {
            // imm ~ [-((1 << 6) - 1) << 2, ((1 << 6) - 1) << 2]
            if (imm >= -0xfc & imm <= 0xfc) {

                if (imm > 0) {
                    // 00nn nnnn : vsp = vsp + (nnnnnn << 2)
                    uint8_t byte = 0x3f & (((int8_t) imm) >> 2);

                    encoded.push_back(byte);
                } else if (imm < 0) {
                    // 01nn nnnn : vsp = vsp - (nnnnnn << 2)
                    uint8_t byte = 0x3f & (((int8_t) (-imm)) >> 2);
                    byte |= 0x40;

                    encoded.push_back(byte);
                }

            } else {
                // 1111 1111 + SLEB128 : vsp = vsp + SLEB128
                uint8_t byte = 0xff;

                encoded.push_back(byte);
                EncodeSLEB128_PUSHBACK(encoded, imm);
            }
        } else if (op == QUT_INSTRUCTION_VSP_SET_BY_R7 || op == QUT_INSTRUCTION_VSP_SET_BY_R11) {

            bool have_prologue = false;

            do {
                if (instructions.size() >= 3) {
                    uint64_t next = instructions.at(0);
                    int32_t next_op = next >> 32;
                    int32_t next_imm = next & 0xFFFFFFFF;
                    if (!(next_op == QUT_INSTRUCTION_VSP_OFFSET)) {
                        break;
                    }

                    next = instructions.at(1);
                    next_op = next >> 32;
                    int32_t next_imm_lr = next & 0xFFFFFFFF;
                    if (!(next_op == QUT_INSTRUCTION_LR_OFFSET && next_imm_lr == (next_imm - 4))) {
                        break;
                    }

                    next = instructions.at(2);
                    next_op = next >> 32;
                    int32_t next_imm_fp = next & 0xFFFFFFFF;

                    if (((op == QUT_INSTRUCTION_VSP_SET_BY_R7 &&
                          next_op == QUT_INSTRUCTION_R7_OFFSET)
                         || (op == QUT_INSTRUCTION_VSP_SET_BY_R11 &&
                             next_op == QUT_INSTRUCTION_R11_OFFSET))
                        && (next_imm_fp == next_imm)) {
                        have_prologue = true;
                        instructions.pop_front(); // vsp + 8
                        instructions.pop_front(); // pop lr
                        instructions.pop_front(); // pop r7/r11
                    }
                }
            } while (false);

            // 1000 0000 : vsp = r7                                      ;
            // 1000 0001 : vsp = r7 + 8, ls = [vsp - 4], sp = [vsp - 8]  ; # have prologue
            // 1001 0000 : vsp = r11                                     ;
            // 1001 0001 : vsp = r11 + 8, ls = [vsp - 4], sp = [vsp - 8] ; # have prologue
            uint8_t byte = op == QUT_INSTRUCTION_VSP_SET_BY_R7 ? 0x80 : 0x90;
            // x = have_prologue ? 1 : 0
            if (have_prologue) {
                byte |= 0x1;
            }

            encoded.push_back(byte);
        } else if (op == QUT_INSTRUCTION_VSP_SET_BY_SP) {
            // 1010 0000 : vsp = [sp] ;
            uint8_t byte = 0xa0;

            encoded.push_back(byte);
        } else if (op == QUT_INSTRUCTION_R7_OFFSET) {
            // imm ~ [0, ((1 << 4) - 1) << 2]
            if (imm >= 0 && imm <= 0x3c) {
                // 1100 nnnn : r7 = [vsp - (nnnn << 2)] ; # (nnnn << 2) in [0, 0x3c]
                uint8_t byte = 0xc0 | ((((int8_t) imm) >> 2) & 0xf);

                encoded.push_back(byte);
            } else {
                // 1111 1100 + SLEB128 : r7 = [vsp - SLEB128] ; # [addr] means get value from pointed by addr
                uint8_t byte = 0xfc;
                encoded.push_back(byte);
                EncodeSLEB128_PUSHBACK(encoded, imm);
            }
        } else if (op == QUT_INSTRUCTION_R11_OFFSET) {
            // imm ~ [0, ((1 << 4) - 1) << 2]
            if (imm >= 0 && imm <= 0x3c) {
                // 1101 nnnn : r11 = [vsp - (nnnn << 2)] ; # (nnnn << 2) in [0, 0x3c]
                uint8_t byte = 0xd0 | ((((int8_t) imm) >> 2) & 0xf);

                encoded.push_back(byte);

            } else {
                // 1111 1101 + SLEB128 : r11 = [vsp - SLEB128] ; # [addr] means get value from pointed by addr
                uint8_t byte = 0xfd;

                encoded.push_back(byte);
                EncodeSLEB128_PUSHBACK(encoded, imm);
            }
        } else if (op == QUT_INSTRUCTION_LR_OFFSET) {
            // imm ~ [0, ((1 << 4) - 1) << 2]
            if (imm >= 0 && imm <= 0x3c) {
                // 1110 nnnn : lr = [vsp - (nnnn << 2)] ; # (nnnn << 2) in [0, 0x3c]
                uint8_t byte = 0xe0 | ((((int8_t) imm) >> 2) & 0xf);

                encoded.push_back(byte);
            } else {
                // 1111 1010 + SLEB128 : lr = [vsp - SLEB128] ; # [addr] means get value from pointed by addr
                uint8_t byte = 0xfa;

                encoded.push_back(byte);
                EncodeSLEB128_PUSHBACK(encoded, imm);
            }
        } else if (op == QUT_INSTRUCTION_SP_OFFSET) {
            // 1111 1001 + SLEB128 : sp = [vsp - SLEB128] ; # [addr] means get value from pointed by addr
            uint8_t byte = 0xf9;

            encoded.push_back(byte);
            EncodeSLEB128_PUSHBACK(encoded, imm);
        } else if (op == QUT_INSTRUCTION_PC_OFFSET) {
            // 1111 1011 + SLEB128 : pc = [vsp - SLEB128] ; # [addr] means get value from pointed by addr
            uint8_t byte = 0xfb;

            encoded.push_back(byte);
            EncodeSLEB128_PUSHBACK(encoded, imm);
        }
    }

    return true;
}

struct TempEntryPair {
    uint32_t entry_point = 0;
    deque<uint8_t> encoded_instructions;
};

bool QuickenTableGenerator::GenerateFutSections(size_t start_offset, size_t total_entries,
        QutSections* fut_sections, size_t &bad_entries_count) {

    if (fut_sections == nullptr) {
        return false;
    }

    deque<shared_ptr<TempEntryPair>> entries_encoded;

    size_t instr_tbl_count = 0;
    TempEntryPair* prev_entry = nullptr;
    size_t bad_entries = 0;
    for (size_t i = 0; i < total_entries; i++) {

        // Compute entry_offset
        uint32_t entry_offset = start_offset + i * 8;
        uint32_t addr = 0;

        // Read entry
        if (!GetPrel31Addr(entry_offset, &addr)) {
            prev_entry = nullptr;
            bad_entries++;
            continue;
        }

        shared_ptr<TempEntryPair> entry_pair(new TempEntryPair());

        entry_pair->entry_point = addr;

        ExidxDecoder decoder(memory_, process_memory_);

        // Extract data, evaluate instructions and re-encode it.
        if (decoder.ExtractEntryData(entry_offset) && decoder.Eval()
            && EncodeInstruction(*decoder.instructions_.get(), entry_pair->encoded_instructions)) {
            // Well done.
        } else {
            bad_entries_count++;
            // Error occurred if we reach here. Add FUT_FINISH for this entry point.
            entry_pair->encoded_instructions.push_back(FUT_FINISH);
        }

        bool same_instr = false;
        if (prev_entry) {
            // Compare two instructions
            if (prev_entry->encoded_instructions.size() == entry_pair->encoded_instructions.size()) {
                same_instr = true;
                for (size_t i = 0; i < prev_entry->encoded_instructions.size(); i++) {
                    if (prev_entry->encoded_instructions.at(i) != entry_pair->encoded_instructions.at(i)) {
                        same_instr = false;
                        break;
                    };
                }
            }
        }

        // Merge same entry
        if (same_instr) {
            // No need save this entry
            continue;
        }

        // Only instructions less or equal than 3 need a table entry.
        if (entry_pair->encoded_instructions.size() > 3) {
            instr_tbl_count += (entry_pair->encoded_instructions.size() + 3) / 4;
        }

        // Finally pushed.
        entries_encoded.push_back(entry_pair);

        prev_entry = entry_pair.get();
    }

    // init fut_sections
    size_t idx_size = 0;
    size_t tbl_size = 0;

    size_t temp_idx_capacity = entries_encoded.size() * 2; // Double size of total_entries.
    size_t temp_tbl_capacity = instr_tbl_count; // Half size of total_entries.
    uint32_t* temp_quidx = new uint32_t[temp_idx_capacity];
    uint32_t* temp_qutbl = new uint32_t[temp_tbl_capacity];

    // Compress instructions.
    while (!entries_encoded.empty()) {
        shared_ptr<TempEntryPair> current_entry = entries_encoded.front();
        entries_encoded.pop_front();

        // part 1.
        temp_quidx[idx_size++] = current_entry->entry_point;
        size_t size_of_deque = current_entry->encoded_instructions.size();
        // Less or equal than 3 instructions can be compact in 2nd part of the index bit set.
        if (size_of_deque <= 3) {
            // Compact instruction has '1' in highest bit.
            uint32_t compact = 0x80000000;
            for (size_t i = 0; i < 3; i++) {
                size_t left_shift = (2 - i) * 8;
                if (size_of_deque > i) {
                    compact |= current_entry->encoded_instructions.at(i) << left_shift;
                } else {
                    compact |= (FUT_FINISH << left_shift);
                }
            }

            // part 2.
            temp_quidx[idx_size++] = compact;
        } else {

            size_t last_tbl_size = tbl_size;

            uint32_t row = 0;
            uint32_t size_ceil = (size_of_deque + 0x3) & 0xfffffff4;
            for (size_t i = 0; i < size_ceil; i++) {

                // Assemble every 4 instructions into a row(32-bit).
                size_t left_shift = (3 - (i % 4)) * 8;
                if (size_of_deque > i) {
                    row |= current_entry->encoded_instructions.at(i) << left_shift;
                } else {
                    // Padding FUT_FINISH behind.
                    row |= (FUT_FINISH << left_shift);
                }

                if (left_shift == 0) {
                    temp_qutbl[tbl_size++] = row;
                    row = 0;
                }
            }

            size_t row_count = tbl_size - last_tbl_size;

            CHECK(row_count <= 0x7f);
            CHECK(last_tbl_size <= 0xffffff);

            uint32_t combined = (row_count & 0x7f) << 24; // Append row count.
            combined |= (last_tbl_size & 0xffffff); // Append entry table offset.

            // part 2.
            temp_quidx[idx_size++] = combined;
        }

        CHECK(idx_size <= temp_idx_capacity);
        CHECK(tbl_size <= temp_tbl_capacity);
    }

    fut_sections->idx_capacity = temp_idx_capacity;
    fut_sections->tbl_capacity = temp_tbl_capacity;
    fut_sections->quidx = temp_quidx;
    fut_sections->qutbl = temp_qutbl;

    fut_sections->idx_size = idx_size;
    fut_sections->tbl_size = tbl_size;

    fut_sections->total_entries = idx_size / 2;

    bad_entries_count = bad_entries;

    return true;
}

bool QuickenTableGenerator::GetPrel31Addr(uint32_t offset, uint32_t* addr) {
    uint32_t data;
    if (!memory_->Read32(offset, &data)) {
        return false;
    }

    // Sign extend the value if necessary.
    int32_t value = (static_cast<int32_t>(data) << 1) >> 1;
    *addr = offset + value;
    return true;
}

#define IterateNextByteIdx(_j, _i, _amount)  \
    _j--; \
    if (_j < 0) { \
        _j = 3; \
        _i++; \
        if (_i >= _amount) { \
            return false; \
        } \
    }

#define ReadByte(_byte, _instructions, _j, _i) \
    _byte = _instructions[_i] >> (8 * _j) & 0xff;

#define DecodeSLEB128(_value, _instructions, _amount, _j, _i) \
    { \
        unsigned Shift = 0; \
        uint8_t Byte; \
        do { \
            IterateNextByteIdx(_j, _i, _amount) \
            ReadByte(Byte, _instructions, _j, _i) \
            _value |= (uint64_t(Byte & 0x7f) << Shift); \
            Shift += 7; \
        } while (Byte >= 0x80); \
        if (Shift < 64 && (Byte & 0x40)) _value |= (-1ULL) << Shift; \
    }

inline bool QuickenTable::Decode(uint32_t* instructions, const size_t amount, size_t start_pos) {

// QUT encode:
//      00nn nnnn           : vsp = vsp + (nnnnnn << 2)                     ; # (nnnnnnn << 2) in [0, 0xfc]
//      01nn nnnn           : vsp = vsp - (nnnnnn << 2)                     ; # (nnnnnnn << 2) in [0, 0xfc]
//      1000 0000           : vsp = r7                                      ;
//      1000 0001           : vsp = r7 + 8, ls = [vsp - 4], sp = [vsp - 8]  ; # have prologue
//      1001 0000           : vsp = r11                                     ;
//      1001 0001           : vsp = r11 + 8, ls = [vsp - 4], sp = [vsp - 8] ; # have prologue
//      1010 0000           : vsp = [sp]                                    ;
//      1100 nnnn           : r7 = [vsp - (nnnn << 2)]                      ; # (nnnn << 2) in [0, 0x3c]
//      1101 nnnn           : r11 = [vsp - (nnnn << 2)]                     ; # (nnnn << 2) in [0, 0x3c]
//      1110 nnnn           : lr = [vsp - (nnnn << 2)]                      ; # (nnnn << 2) in [0, 0x3c]
//      1111 0000           : Finish                                        ;
//      1111 1001 + SLEB128 : sp = [vsp - SLEB128]                          ; # [addr] means get value from pointed by addr
//      1111 1010 + SLEB128 : lr = [vsp - SLEB128]                          ; # [addr] means get value from pointed by addr
//      1111 1011 + SLEB128 : pc = [vsp - SLEB128]                          ; # [addr] means get value from pointed by addr
//      1111 1100 + SLEB128 : r7 = [vsp - SLEB128]                          ; # [addr] means get value from pointed by addr
//      1111 1101 + SLEB128 : r11 = [vsp - SLEB128]                         ; # [addr] means get value from pointed by addr
//      1111 1111 + SLEB128 : vsp = vsp + SLEB128                           ;

#ifndef __arm__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wint-to-pointer-cast"
#endif

    int32_t j = start_pos;
    for (size_t i = 0; i < amount;) {
        uint8_t byte;
        ReadByte(byte, instructions, j, i);

        switch (byte >> 6) {
        case 0:
            // 00nn nnnn : vsp = vsp + (nnnnnn << 2) ; # (nnnnnnn << 2) in [0, 0xfc]
            cfa_ += (byte << 2);
            break;
        case 1:
            // 01nn nnnn : vsp = vsp - (nnnnnn << 2) ; # (nnnnnnn << 2) in [0, 0xfc]
            cfa_ -= (byte << 2);
            break;
        case 2:
            switch (byte) {
                case 0x80:
                    // 1000 0000 : vsp = r7 ;
                    cfa_ = R7(regs_);
                    break;
                case 0x81:
                    // 1000 0001 : vsp = r7 + 8, ls = [vsp - 4], sp = [vsp - 8] ; # have prologue
                    cfa_ = R7(regs_) + 8;
                    LR(regs_) = *((uint32_t *)(cfa_ - 4));
                    R7(regs_) = *((uint32_t *)(cfa_ - 8));
                    break;
                case 0x90:
                    // 1001 0000 : vsp = r11 ;
                    cfa_ = R11(regs_);
                    break;
                case 0x91:
                    // 1001 0001 : vsp = r11 + 8, ls = [vsp - 4], sp = [vsp - 8] ; # have prologue
                    cfa_ = R11(regs_) + 8;
                    LR(regs_) = *((uint32_t *)(cfa_ - 4));
                    R11(regs_) = *((uint32_t *)(cfa_ - 8));
                    break;
                case 0xa0:
                    // 1010 0000 : vsp = [sp] ;
                    cfa_ = SP(regs_);
                    break;
            }
            break;
        default:
            switch (byte >> 4) {
                case 0xc:
                    // 1100 nnnn : r7 = [vsp - (nnnn << 2)] ; # (nnnn << 2) in [0, 0x3c]
                    R7(regs_) = *((uint32_t *)(cfa_ - ((byte & 0xf) << 2)));
                    break;
                case 0xd:
                    // 1101 nnnn : r11 = [vsp - (nnnn << 2)] ; # (nnnn << 2) in [0, 0x3c]
                    R11(regs_) = *((uint32_t *)(cfa_ - ((byte & 0xf) << 2)));
                    break;
                case 0xe:
                    // 1110 nnnn : lr = [vsp - (nnnn << 2)] ; # (nnnn << 2) in [0, 0x3c]
                    LR(regs_) = *((uint32_t *)(cfa_ - ((byte & 0xf) << 2)));
                    break;
                default:
                    uint64_t value = 0;
                    switch (byte) {
                        case 0xf0:
                            // finished
                            return true;
                        case 0xf9:
                            // 1111 1001 + SLEB128 : sp = [vsp - SLEB128] ; # [addr] means get value from pointed by addr
                            DecodeSLEB128(value, instructions, amount, j, i)
                            SP(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
                            break;
                        case 0xfa:
                            // 1111 1010 + SLEB128 : lr = [vsp - SLEB128] ; # [addr] means get value from pointed by addr
                            DecodeSLEB128(value, instructions, amount, j, i)
                            LR(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
                            break;
                        case 0xfb:
                            // 1111 1011 + SLEB128 : pc = [vsp - SLEB128] ; # [addr] means get value from pointed by addr
                            DecodeSLEB128(value, instructions, amount, j, i)
                            PC(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
                            pc_set_ = true;
                            break;
                        case 0xfc:
                            // 1111 1100 + SLEB128 : r7 = [vsp - SLEB128] ; # [addr] means get value from pointed by addr
                            DecodeSLEB128(value, instructions, amount, j, i)
                            R7(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
                            break;
                        case 0xfd:
                            // 1111 1101 + SLEB128 : r11 = [vsp - SLEB128] ; # [addr] means get value from pointed by addr
                            DecodeSLEB128(value, instructions, amount, j, i)
                            R11(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
                            break;
                        case 0xff:
                            // 1111 1111 + SLEB128 : vsp = vsp + SLEB128 ;
                            DecodeSLEB128(value, instructions, amount, j, i)
                            cfa_ += (uint32_t)value;
                            break;
                        default:
                            return false;
                    }
                    break;
            }
            break;
        }

        j--;
        if (j < 0) {
            j = 3;
            i++;
        }
    }

#ifndef __arm__
    #pragma clang diagnostic pop
#endif

    return true;
}

bool QuickenTable::Eval(uint32_t entry_offset) {

    uint32_t command = fut_sections_->quidx[entry_offset + 1];

    if (command >> 31) {
        return Decode(&command, 1, 2); // compact
    } else {
        uint32_t row_count = command >> 24 & 0x7f;
        uint32_t row_offset = command & 0xffffff;
        return Decode(&fut_sections_->qutbl[row_offset], row_count);
    }
}

}  // namespace wechat_backtrace
