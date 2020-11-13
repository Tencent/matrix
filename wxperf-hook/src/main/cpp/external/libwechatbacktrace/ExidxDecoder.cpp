//
// Created by Carl on 2020-09-21.
//

#include <cstdint>
#include <deps/android-base/include/android-base/logging.h>
#include <include/unwindstack/MachineArm.h>
#include <include/unwindstack/Memory.h>
#include <vector>
#include <utility>
#include <memory>
#include <QutStatistics.h>
#include "QuickenTableGenerator.h"
#include "MinimalRegs.h"
#include "../../common/Log.h"
#include "DwarfEhFrameWithHdrDecoder.h"
#include "DwarfDebugFrameDecoder.h"
#include "ExidxDecoder.h"

namespace wechat_backtrace {

    using namespace std;
    using namespace unwindstack;

    void ExidxDecoder::FlushInstructions() {

        if (context_.vsp_ != 0) {
            CHECK((context_.vsp_ & 0x3) == 0);
            uint64_t instr = (((uint64_t) QUT_INSTRUCTION_VSP_OFFSET) << 32) | context_.vsp_;
            instructions_->push_back(instr);
        }

        vector<pair<size_t, int32_t>> sorted_regs;

        // Sort by vsp offset ascend, convenient for search prologue later.
        for (size_t i = 0; i < QUT_MINIMAL_REG_SIZE; i++) {
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
            uint32_t QUT_INSTR = (sorted_regs.at(j).first);
            if (context_.transformed_bits & (1 << QUT_INSTR)) {
                CHECK((context_.regs_[QUT_INSTR] & 0x3) == 0);
                uint64_t instr = (((uint64_t) QUT_INSTR) << 32) | context_.regs_[QUT_INSTR];
                instructions_->push_back(instr);
            }
        }

        if (context_.transformed_bits & (1 << QUT_INSTRUCTION_SP_OFFSET)) {
            uint64_t instr = (((uint64_t) QUT_INSTRUCTION_VSP_SET_BY_SP) << 32);
            instructions_->push_back(instr);
        }

        // reset
        context_.reset();
    }

    void ExidxDecoder::SaveInstructions(QutInstruction instruction) {

        FlushInstructions();

        if (instruction == QUT_INSTRUCTION_VSP_SET_BY_R7 ||
            instruction == QUT_INSTRUCTION_VSP_SET_BY_R11) {
            instructions_->push_back((((uint64_t) instruction) << 32));
        }
    }

    inline bool ExidxDecoder::GetByte(uint8_t *byte) {
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
        context_.log = log;
        while (Decode());

        SaveInstructions(QUT_NOP);

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

        for (size_t reg = ARM_REG_R4; reg <= ARM_REG_R12; reg++) {
            if (registers & (1 << reg)) {

                if (ARM_REG_R4 == reg) {    // r4
                    context_.Transform(QUT_INSTRUCTION_R4_OFFSET);
                } else if (ARM_REG_R7 == reg) { // r7
                    context_.Transform(QUT_INSTRUCTION_R7_OFFSET);
                } else if (ARM_REG_R10 == reg) {    // r10
                    context_.Transform(QUT_INSTRUCTION_R10_OFFSET);
                } else if (ARM_REG_R11 == reg) {    // r11
                    context_.Transform(QUT_INSTRUCTION_R11_OFFSET);
                }
                context_.AddUpVSP(4);
            }
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
            SaveInstructions(QUT_NOP);
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
            QUT_STATISTIC(UnsupportedArmExdix, bits, byte);
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
            } else if (i == ARM_REG_R4) {  // r4
                context_.Transform(QUT_INSTRUCTION_R4_OFFSET);
            } else if (i == ARM_REG_R10) {  // r10
                context_.Transform(QUT_INSTRUCTION_R10_OFFSET);
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

}  // namespace wechat_backtrace
