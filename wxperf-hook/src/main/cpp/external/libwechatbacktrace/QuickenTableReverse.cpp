//
// Created by Carl on 2020-09-21.
//

#include <deps/android-base/include/android-base/logging.h>
#include <include/unwindstack/MachineArm.h>
#include <include/unwindstack/Memory.h>
#include <vector>
#include <utility>
#include <memory>
#include <QuickenInstructions.h>
#include <MinimalRegs.h>
#include "QuickenTableReverse.h"
#include "../../common/Log.h"

namespace wechat_backtrace {

using namespace std;
using namespace unwindstack;

#define ReturnIndexOverflow(_i, _amount)  \
        if (_i >= _amount) { \
            return false; \
        }

#define NoReturnIndexOverflow(_i, _amount)

#define IterateNextByteIdx(_j, _i, _amount, _ReturnIndexOverflow)  \
    _j--; \
    if (_j < 0) { \
        _j = QUT_TBL_ROW_SIZE; \
        _i++; \
        _ReturnIndexOverflow(_i, _amount) \
    }

#define ReadByte(_byte, _instructions, _j, _i, _amount) \
    _byte = _instructions[_i] >> (8 * _j) & 0xff; \
    IterateNextByteIdx(_j, _i, _amount, NoReturnIndexOverflow)

#define DecodeSLEB128(_value, _instructions, _amount, _j, _i) \
    { \
        unsigned Shift = 0; \
        uint8_t Byte; \
        do { \
            IterateNextByteIdx(_j, _i, _amount, ReturnIndexOverflow) \
            ReadByte(Byte, _instructions, _j, _i, _amount) \
            _value |= (uint64_t(Byte & 0x7f) << Shift); \
            Shift += 7; \
        } while (Byte >= 0x80); \
        if (Shift < 64 && (Byte & 0x40)) _value |= (-1ULL) << Shift; \
    }

// ------------------------------------------------------------------------------------------------

//    inline bool
//    Decode32ReverseInstr(const uint32_t *instructions, const size_t amount, const size_t start_pos,
//                         uint32_t *decoded, uint8_t &index) {
//
//        uint32_t j = start_pos;
//        size_t i = 0;
//
//        for (size_t m = 0; m < amount; m++) {
//            QUT_DEBUG_LOG("QuickenTable::Decode32Reverse instruction %x", instructions[m]);
////        QUT_TMP_LOG("QuickenTable::Decode instruction %x", instructions[m]);
//        }
//
//        while (i < amount) {
//            uint8_t byte;
//            ReadByte(byte, instructions, j, i, amount);
//            switch (byte >> 6) {
//                case 0:
////                cfa_ += (byte << 2);
//                    decoded[index++] = byte;
//                    index += 2;
//                    break;
//                case 1:
////                cfa_ -= (byte << 2);
//                    decoded[index++] = byte;
//                    index += 2;
//                    break;
//                case 2:
//                    switch (byte) {
//                        case QUT_INSTRUCTION_VSP_SET_BY_R7_OP:
////                        cfa_ = R7(regs_);
//                            decoded[index] = QUT_INSTRUCTION_VSP_SET_BY_R7_OP;
//                            index += 2;
//                            break;
//                        case QUT_INSTRUCTION_VSP_SET_BY_R7_PROLOGUE_OP:
////                        cfa_ = R7(regs_) + 8;
////                        LR(regs_) = *((uint32_t *)(cfa_ - 4));
////                        R7(regs_) = *((uint32_t *)(cfa_ - 8));
//                            decoded[index] = QUT_INSTRUCTION_VSP_SET_BY_R7_PROLOGUE_OP;
//                            index += 2;
//                            break;
//                        case QUT_INSTRUCTION_VSP_SET_BY_R11_OP:
////                        cfa_ = R11(regs_);
//                            decoded[index] = QUT_INSTRUCTION_VSP_SET_BY_R11_OP;
//                            index += 2;
//                            break;
//                        case QUT_INSTRUCTION_VSP_SET_BY_R11_PROLOGUE_OP:
////                        cfa_ = R11(regs_) + 8;
////                        LR(regs_) = *((uint32_t *)(cfa_ - 4));
////                        R11(regs_) = *((uint32_t *)(cfa_ - 8));
//                            decoded[index] = QUT_INSTRUCTION_VSP_SET_BY_R11_PROLOGUE_OP;
//                            index += 2;
//                            break;
//                        case QUT_INSTRUCTION_VSP_SET_BY_SP_OP:
////                        cfa_ = SP(regs_);
//                            decoded[index] = QUT_INSTRUCTION_VSP_SET_BY_SP_OP;
//                            index += 2;
//                            break;
//                        case QUT_INSTRUCTION_VSP_SET_BY_JNI_SP_OP: {
////                        uint8_t byte;
////                        ReadByte(byte, instructions, j, i, amount);
////                        cfa_ = R10(regs_) + ((byte & 0x7f) << 2);
////
////                        QUT_DEBUG_LOG("Decode QUT_INSTRUCTION_VSP_SET_BY_JNI_SP_OP %x r10 %x, cfa_ %x, offset %x, " BYTE_TO_BINARY_PATTERN,
////                                      (uint32_t)byte, R10(regs_), cfa_, (uint32_t)((byte & 0x7f) << 2), BYTE_TO_BINARY(byte));
//                            uint8_t byte;
//                            ReadByte(byte, instructions, j, i, amount);
//                            decoded[index++] = QUT_INSTRUCTION_VSP_SET_BY_JNI_SP_OP;
//                            decoded[index++] = byte;
//
//                            break;
//                        }
//                        case QUT_INSTRUCTION_VSP_SET_IMM_OP: {
////                        int64_t value = 0;// TODO overflow
////                        DecodeSLEB128(value, instructions, amount, j, i)
////                        cfa_ = value;
//                            int32_t value = 0;// TODO overflow
//                            DecodeSLEB128(value, instructions, amount, j, i)
//                            decoded[index++] = QUT_INSTRUCTION_VSP_SET_IMM_OP;
//                            decoded[index++] = (int32_t) value;
//                            break;
//                        }
//                        case QUT_INSTRUCTION_VSP_SET_BY_R7_IMM_OP: {
////                        uint8_t byte;
////                        ReadByte(byte, instructions, j, i, amount);
////                        cfa_ = R7(regs_) + ((byte & 0x7f) << 2);
//                            uint8_t byte;
//                            ReadByte(byte, instructions, j, i, amount);
//                            decoded[index++] = QUT_INSTRUCTION_VSP_SET_BY_R7_IMM_OP;
//                            decoded[index++] = byte;
//                            break;
//                        }
//                        case QUT_INSTRUCTION_VSP_SET_BY_R11_IMM_OP: {
////                        uint8_t byte;
////                        ReadByte(byte, instructions, j, i, amount);
////                        cfa_ = R11(regs_) + ((byte & 0x7f) << 2);
//                            uint8_t byte;
//                            ReadByte(byte, instructions, j, i, amount);
//                            decoded[index++] = QUT_INSTRUCTION_VSP_SET_BY_R11_IMM_OP;
//                            decoded[index++] = byte;
//                            break;
//                        }
//                        case QUT_INSTRUCTION_DEX_PC_SET_OP: {
////                        QUT_DEBUG_LOG("Decode byte QUT_INSTRUCTION_DEX_PC_SET_OP");
////                        dex_pc_ = R4(regs_);
//                            // skip this instr
//                            decoded[index] = QUT_INSTRUCTION_DEX_PC_SET_OP;
//                            index += 2;
//                            break;
//                        }
//                        case QUT_END_OF_INS_OP: {
////                        QUT_DEBUG_LOG("Decode byte QUT_END_OF_INS_OP");
//                            decoded[index] = QUT_END_OF_INS_OP;
//                            index += 2;
//                            return true;
//                        }
//                        case QUT_FINISH_OP: {
////                        QUT_DEBUG_LOG("Decode byte QUT_FINISH_OP");
////                        PC(regs_) = 0;
////                        LR(regs_) = 0;
//                            decoded[index] = QUT_FINISH_OP;
//                            index += 2;
//                            return true;
//                        }
//                        default: {
//                            switch (byte & 0xf0) {
//                                case QUT_INSTRUCTION_R4_OFFSET_OP_PREFIX: {
////                                R4(regs_) = *((uint32_t *)(cfa_ - ((byte & 0xf) << 2)));
//                                    decoded[index] = byte;
//                                    index += 2;
//                                    break;
//                                }
//                                case QUT_INSTRUCTION_R7_OFFSET_OP_PREFIX: {
////                                R7(regs_) = *((uint32_t *)(cfa_ - ((byte & 0xf) << 2)));
//                                    decoded[index] = byte;
//                                    index += 2;
//                                    break;
//                                }
//                            }
//                        }
//                    }
//                    break;
//                default:
//                    switch (byte & 0xf0) {
//                        case QUT_INSTRUCTION_R10_OFFSET_OP_PREFIX:
////                        R10(regs_) = *((uint32_t *)(cfa_ - ((byte & 0xf) << 2)));
//                            decoded[index] = byte;
//                            index += 2;
//                            break;
//                        case QUT_INSTRUCTION_R11_OFFSET_OP_PREFIX:
////                        R11(regs_) = *((uint32_t *)(cfa_ - ((byte & 0xf) << 2)));
//                            decoded[index] = byte;
//                            index += 2;
//                            break;
//                        case QUT_INSTRUCTION_LR_OFFSET_OP_PREFIX:
////                        LR(regs_) = *((uint32_t *)(cfa_ - ((byte & 0xf) << 2)));
//                            decoded[index] = byte;
//                            index += 2;
//                            break;
//                        default:
//                            uint32_t value = 0; // TODO overflow
//                            switch (byte) {
//                                case QUT_INSTRUCTION_R7_OFFSET_SLEB128_OP:
////                                DecodeSLEB128(value, instructions, amount, j, i)
////                                R7(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
//                                DecodeSLEB128(value, instructions, amount, j, i)
//                                    decoded[index++] = QUT_INSTRUCTION_R7_OFFSET_SLEB128_OP;
//                                    decoded[index++] = value;
//                                    break;
//                                case QUT_INSTRUCTION_R10_OFFSET_SLEB128_OP:
////                                DecodeSLEB128(value, instructions, amount, j, i)
////                                R10(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
//                                DecodeSLEB128(value, instructions, amount, j, i)
//                                    decoded[index++] = QUT_INSTRUCTION_R10_OFFSET_SLEB128_OP;
//                                    decoded[index++] = value;
//                                    break;
//                                case QUT_INSTRUCTION_R11_OFFSET_SLEB128_OP:
////                                DecodeSLEB128(value, instructions, amount, j, i)
////                                R11(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
//                                DecodeSLEB128(value, instructions, amount, j, i)
//                                    decoded[index++] = QUT_INSTRUCTION_R11_OFFSET_SLEB128_OP;
//                                    decoded[index++] = value;
//                                    break;
//                                case QUT_INSTRUCTION_SP_OFFSET_SLEB128_OP:
////                                DecodeSLEB128(value, instructions, amount, j, i)
////                                SP(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
//                                DecodeSLEB128(value, instructions, amount, j, i)
//                                    decoded[index++] = QUT_INSTRUCTION_SP_OFFSET_SLEB128_OP;
//                                    decoded[index++] = value;
//                                    break;
//                                case QUT_INSTRUCTION_LR_OFFSET_SLEB128_OP:
////                                DecodeSLEB128(value, instructions, amount, j, i)
////                                LR(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
//                                DecodeSLEB128(value, instructions, amount, j, i)
//                                    decoded[index++] = QUT_INSTRUCTION_LR_OFFSET_SLEB128_OP;
//                                    decoded[index++] = value;
//                                    break;
//                                case QUT_INSTRUCTION_PC_OFFSET_SLEB128_OP:
////                                DecodeSLEB128(value, instructions, amount, j, i)
////                                PC(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
////                                pc_set_ = true;
//                                DecodeSLEB128(value, instructions, amount, j, i)
//                                    decoded[index++] = QUT_INSTRUCTION_PC_OFFSET_SLEB128_OP;
//                                    decoded[index++] = value;
//                                    break;
//                                case QUT_INSTRUCTION_VSP_OFFSET_SLEB128_OP:
////                                DecodeSLEB128(value, instructions, amount, j, i)
////                                cfa_ += (uint32_t)value;
//                                DecodeSLEB128(value, instructions, amount, j, i)
//                                    decoded[index++] = QUT_INSTRUCTION_VSP_OFFSET_SLEB128_OP;
//                                    decoded[index++] = value;
//                                    break;
//                                default:
//                                    QUT_DEBUG_LOG("Decode byte UNKNOWN");
//                                    return false;
//                            }
//                            break;
//                    }
//                    break;
//            }
//        }
//
//        return true;
//    }

//// TODO this is 32 bit
//inline bool QuickenTableReverse::Decode32Reverse(const uint32_t* instructions, const size_t amount, const size_t start_pos) {
//
//    QUT_DEBUG_LOG("QuickenTable::Decode32Reverse instructions %x, amount %u, start_pos %u", instructions, (uint32_t)amount, (uint32_t)start_pos);
//
//#pragma clang diagnostic push
//#pragma clang diagnostic ignored "-Wint-to-pointer-cast"
//
//    for (size_t m = 0; m < amount; m++) {
//        QUT_DEBUG_LOG("QuickenTable::Decode32Reverse instruction %x", instructions[m]);
////        QUT_TMP_LOG("QuickenTable::Decode instruction %x", instructions[m]);
//    }
//
//    // TODO overflow check
//
//    if (amount * 8 > 24) {
//        // TODO Error. Too many commands.
//        return false;
//    }
//
//    uint32_t decoded[24];
//    uint8_t index = 0;
//
//    if (UNLIKELY(!Decode32ReverseInstr(instructions, amount, start_pos, decoded, index))) {
//        return false;
//    }
//
//    uint8_t i = index - 2;
//
//    while(i >= 0) {
//        uint8_t byte = (uint8_t)decoded[i];
//        switch (byte >> 6) {
//            case 0:
////                cfa_ += (byte << 2);
//                cfa_ -= (byte << 2);
//                break;
//            case 1:
////                cfa_ -= (byte << 2);
//                cfa_ += (byte << 2);
//                break;
//            case 2:
//                switch (byte) {
//                    case QUT_INSTRUCTION_VSP_SET_BY_R7_OP:
////                        cfa_ = R7(regs_);
//                        if (UNLIKELY(cfa_set_)) {
//                            return false;
//                        }
//                        cfa_set_ = true;
//
//                        if (!CHECK_REG_BIT(regs_bits_, R7_REG_BIT)) {
//                            R7(regs_) = cfa_;
//                            SET_REG_BIT(regs_bits_, R7_REG_BIT);
//                        }
//
//                        break;
//                    case QUT_INSTRUCTION_VSP_SET_BY_R7_PROLOGUE_OP:
////                        cfa_ = R7(regs_) + 8;
////                        LR(regs_) = *((uint32_t *)(cfa_ - 4));
////                        R7(regs_) = *((uint32_t *)(cfa_ - 8));
//                        if (UNLIKELY(cfa_set_)) {
//                            return false;
//                        }
//                        cfa_set_ = true;
//                        if (!CHECK_REG_BIT(regs_bits_, LR_REG_BIT)) {
//                            LR(regs_) = *((uint32_t *)(cfa_ - 4));
//                            SET_REG_BIT(regs_bits_, LR_REG_BIT);
//                        }
//                        if (!CHECK_REG_BIT(regs_bits_, R7_REG_BIT)) {
//                            R7(regs_) = *((uint32_t *)(cfa_ - 8));
//                            SET_REG_BIT(regs_bits_, R7_REG_BIT);
//                        }
//                        break;
//                    case QUT_INSTRUCTION_VSP_SET_BY_R11_OP:
////                        cfa_ = R11(regs_);
//                        if (UNLIKELY(cfa_set_)) {
//                            return false;
//                        }
//                        cfa_set_ = true;
//                        if (!CHECK_REG_BIT(regs_bits_, R11_REG_BIT)) {
//                            R11(regs_) = cfa_;
//                            SET_REG_BIT(regs_bits_, R11_REG_BIT);
//                        }
//                        break;
//                    case QUT_INSTRUCTION_VSP_SET_BY_R11_PROLOGUE_OP:
////                        cfa_ = R11(regs_) + 8;
////                        LR(regs_) = *((uint32_t *)(cfa_ - 4));
////                        R11(regs_) = *((uint32_t *)(cfa_ - 8));
//                        if (UNLIKELY(cfa_set_)) {
//                            return false;
//                        }
//                        cfa_set_ = true;
//
//                        if (!CHECK_REG_BIT(regs_bits_, LR_REG_BIT)) {
//                            LR(regs_) = *((uint32_t *)(cfa_ - 4));
//                            SET_REG_BIT(regs_bits_, LR_REG_BIT);
//                        }
//                        if (!CHECK_REG_BIT(regs_bits_, R11_REG_BIT)) {
//                            R11(regs_) = *((uint32_t *)(cfa_ - 8));
//                            SET_REG_BIT(regs_bits_, R11_REG_BIT);
//                        }
//                        break;
//                    case QUT_INSTRUCTION_VSP_SET_BY_SP_OP:
////                        cfa_ = SP(regs_);
//                        if (UNLIKELY(cfa_set_)) {
//                            return false;
//                        }
//                        cfa_set_ = true;
//                        break;
//                    case QUT_INSTRUCTION_VSP_SET_BY_JNI_SP_OP: {
////                        uint8_t byte;
////                        ReadByte(byte, instructions, j, i, amount);
////                        cfa_ = R10(regs_) + ((byte & 0x7f) << 2);
////
////                        QUT_DEBUG_LOG("Decode QUT_INSTRUCTION_VSP_SET_BY_JNI_SP_OP %x r10 %x, cfa_ %x, offset %x, " BYTE_TO_BINARY_PATTERN,
////                                      (uint32_t)byte, R10(regs_), cfa_, (uint32_t)((byte & 0x7f) << 2), BYTE_TO_BINARY(byte));
//                        if (UNLIKELY(cfa_set_)) {
//                            return false;
//                        }
//                        cfa_set_ = true;
//
//                        if (!CHECK_REG_BIT(regs_bits_, R10_REG_BIT)) {
//                            uint8_t byte = decoded[i + 1];
//                            R10(regs_) = cfa_ - ((byte & 0x7f) << 2);
//                            SET_REG_BIT(regs_bits_, R10_REG_BIT);
//                        }
//                        break;
//                    }
//                    case QUT_INSTRUCTION_VSP_SET_IMM_OP: {
////                        int64_t value = 0;// TODO overflow
////                        DecodeSLEB128(value, instructions, amount, j, i)
////                        cfa_ = value;
//                        if (UNLIKELY(cfa_set_)) {
//                            return false;
//                        }
//                        cfa_set_ = true;
//                        break;
//                    }
//                    case QUT_INSTRUCTION_VSP_SET_BY_R7_IMM_OP: {
////                        uint8_t byte;
////                        ReadByte(byte, instructions, j, i, amount);
////                        cfa_ = R7(regs_) + ((byte & 0x7f) << 2);
//                        if (UNLIKELY(cfa_set_)) {
//                            return false;
//                        }
//                        cfa_set_ = true;
//
//                        if (!CHECK_REG_BIT(regs_bits_, R7_REG_BIT)) {
//                            uint8_t byte = decoded[i + 1];
//                            R7(regs_) = cfa_ - ((byte & 0x7f) << 2);
//                            SET_REG_BIT(regs_bits_, R7_REG_BIT);
//                        }
//
//                        break;
//                    }
//                    case QUT_INSTRUCTION_VSP_SET_BY_R11_IMM_OP: {
////                        uint8_t byte;
////                        ReadByte(byte, instructions, j, i, amount);
////                        cfa_ = R11(regs_) + ((byte & 0x7f) << 2);
//
//                        if (UNLIKELY(cfa_set_)) {
//                            return false;
//                        }
//                        cfa_set_ = true;
//
//                        if (!CHECK_REG_BIT(regs_bits_, R11_REG_BIT)) {
//                            uint8_t byte = decoded[i + 1];
//                            R11(regs_) = cfa_ - ((byte & 0x7f) << 2);
//                            SET_REG_BIT(regs_bits_, R11_REG_BIT);
//                        }
//                        break;
//                    }
//                    case QUT_INSTRUCTION_DEX_PC_SET_OP: {
////                        QUT_DEBUG_LOG("Decode byte QUT_INSTRUCTION_DEX_PC_SET_OP");
////                        dex_pc_ = R4(regs_);
//                        break;
//                    }
//                    case QUT_END_OF_INS_OP: {
////                        QUT_DEBUG_LOG("Decode byte QUT_END_OF_INS_OP");
//                        return true;
//                    }
//                    case QUT_FINISH_OP: {
////                        QUT_DEBUG_LOG("Decode byte QUT_FINISH_OP");
////                        PC(regs_) = 0;
////                        LR(regs_) = 0;
//                        return true;
//                    }
//                    default: {
//                        switch (byte & 0xf0) {
//                            case QUT_INSTRUCTION_R4_OFFSET_OP_PREFIX: {
//                                if (!CHECK_REG_BIT(regs_bits_, R4_REG_BIT)) {
//                                    R4(regs_) = *((uint32_t *)(cfa_ - ((byte & 0xf) << 2)));
//                                    SET_REG_BIT(regs_bits_, R4_REG_BIT);
//                                }
//                                break;
//                            }
//                            case QUT_INSTRUCTION_R7_OFFSET_OP_PREFIX: {
//                                if (!CHECK_REG_BIT(regs_bits_, R7_REG_BIT)) {
//                                    R7(regs_) = *((uint32_t *)(cfa_ - ((byte & 0xf) << 2)));
//                                    SET_REG_BIT(regs_bits_, R7_REG_BIT);
//                                }
//                                break;
//                            }
//                        }
//                    }
//                }
//                break;
//            default:
//                switch (byte & 0xf0) {
//                    case QUT_INSTRUCTION_R10_OFFSET_OP_PREFIX:
//                        if (!CHECK_REG_BIT(regs_bits_, R10_REG_BIT)) {
//                            R10(regs_) = *((uint32_t *)(cfa_ - ((byte & 0xf) << 2)));
//                            SET_REG_BIT(regs_bits_, R10_REG_BIT);
//                        }
//                        break;
//                    case QUT_INSTRUCTION_R11_OFFSET_OP_PREFIX:
////                        R11(regs_) = *((uint32_t *)(cfa_ - ((byte & 0xf) << 2)));
//
//                        if (!CHECK_REG_BIT(regs_bits_, R11_REG_BIT)) {
//                            R11(regs_) = *((uint32_t *)(cfa_ - ((byte & 0xf) << 2)));
//                            SET_REG_BIT(regs_bits_, R11_REG_BIT);
//                        }
//                        break;
//                    case QUT_INSTRUCTION_LR_OFFSET_OP_PREFIX:
////                        LR(regs_) = *((uint32_t *)(cfa_ - ((byte & 0xf) << 2)));
//
//                        if (!CHECK_REG_BIT(regs_bits_, LR_REG_BIT)) {
//                            LR(regs_) = *((uint32_t *)(cfa_ - ((byte & 0xf) << 2)));
//                            SET_REG_BIT(regs_bits_, LR_REG_BIT);
//                        }
//                        break;
//                    default:
//                        uint64_t value = 0;
//                        switch (byte) {
//                            case QUT_INSTRUCTION_R7_OFFSET_SLEB128_OP:
////                                DecodeSLEB128(value, instructions, amount, j, i)
////                                R7(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
//                                if (!CHECK_REG_BIT(regs_bits_, R7_REG_BIT)) {
//                                    value = decoded[i + 1];
//                                    R7(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
//                                    SET_REG_BIT(regs_bits_, R7_REG_BIT);
//                                }
//                                break;
//                            case QUT_INSTRUCTION_R10_OFFSET_SLEB128_OP:
////                            DecodeSLEB128(value, instructions, amount, j, i)
////                                R10(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
//                                if (!CHECK_REG_BIT(regs_bits_, R10_REG_BIT)) {
//                                    value = decoded[i + 1];
//                                    R10(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
//                                    SET_REG_BIT(regs_bits_, R10_REG_BIT);
//                                }
//                                break;
//                            case QUT_INSTRUCTION_R11_OFFSET_SLEB128_OP:
////                            DecodeSLEB128(value, instructions, amount, j, i)
////                                R11(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
//                                if (!CHECK_REG_BIT(regs_bits_, R11_REG_BIT)) {
//                                    value = decoded[i + 1];
//                                    R11(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
//                                    SET_REG_BIT(regs_bits_, R11_REG_BIT);
//                                }
//                                break;
//                            case QUT_INSTRUCTION_SP_OFFSET_SLEB128_OP:
////                            DecodeSLEB128(value, instructions, amount, j, i)
////                                SP(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
//                                break;
//                            case QUT_INSTRUCTION_LR_OFFSET_SLEB128_OP:
////                            DecodeSLEB128(value, instructions, amount, j, i)
////                                LR(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
//                                if (!CHECK_REG_BIT(regs_bits_, LR_REG_BIT)) {
//                                    value = decoded[i + 1];
//                                    LR(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
//                                    SET_REG_BIT(regs_bits_, LR_REG_BIT);
//                                }
//                                break;
//                            case QUT_INSTRUCTION_PC_OFFSET_SLEB128_OP:
////                            DecodeSLEB128(value, instructions, amount, j, i)
////                                PC(regs_) = *((uint32_t *)(cfa_ - (uint32_t)value));
////                                pc_set_ = true;
//                                break;
//                            case QUT_INSTRUCTION_VSP_OFFSET_SLEB128_OP:
////                                DecodeSLEB128(value, instructions, amount, j, i)
//                                value = decoded[i + 1];
//                                cfa_ += (uint32_t)value;
//                                break;
//                            default:
//                                QUT_DEBUG_LOG("Decode byte UNKNOWN");
//                                return false;
//                        }
//                        break;
//                }
//                break;
//        }
//
//        i = i - 2;
//    }
//
//#pragma clang diagnostic pop
//
//    return true;
//}

#define VSP_SET_BY_MACRO(Reg, Offset) \
    if (UNLIKELY(cfa_set_)) { \
        return false; \
    } else { \
        if (!CHECK_REG_BIT(regs_bits_, Reg##_REG_BIT)) { \
            resolving_regs[Reg##_REG_BIT] = cfa_next_ - Offset; \
            resolving = true; \
            SET_REG_BIT(regs_bits_tmp_, Reg##_REG_BIT); \
        } \
        cfa_set_ = true; \
    } \

#define UNRESOLVED_VSP_SET_BY_MACRO(Reg, Offset) \
    if (UNLIKELY(cfa_set_)) { \
        return false; \
    } else { \
        if (!CHECK_REG_BIT(regs_bits_, Reg##_REG_BIT)) { \
            resolving_regs[Reg##_REG_BIT] = cfa_next_ - Offset; \
            resolving = true; \
            unresolved_regs_bits |= (1 << Reg##_REG_BIT); \
            SET_REG_BIT(regs_bits_tmp_, Reg##_REG_BIT); \
        } \
        cfa_set_ = true; \
    } \

#define REG_OFFSET_MACRO(Reg, Offset) \
    if (CHECK_REG_BIT(regs_bits_, Reg##_REG_BIT)) { \
        continue; \
    } \
    if (LIKELY(cfa_set_)) { \
        resolving_regs[Reg##_REG_BIT] = cfa_next_ - Offset; \
        unresolved_regs_bits |= (1 << Reg##_REG_BIT); \
        resolving = true; \
    } else { \
        Reg(regs_) = *((uint32_t *)(cfa_ - Offset)); \
    } \
    SET_REG_BIT(regs_bits_tmp_, Reg##_REG_BIT);  \

inline bool QuickenTableReverse::Decode32Reverse(const uint32_t* instructions, const size_t amount, const size_t start_pos) {

    QUT_DEBUG_LOG("QuickenTable::Decode32 instructions %x, amount %u, start_pos %u", instructions, (uint32_t)amount, (uint32_t)start_pos);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wint-to-pointer-cast"

    int32_t j = start_pos;
    size_t i = 0;

#ifdef EnableLOG
    for (size_t m = 0; m < amount; m++) {
//        QUT_DEBUG_LOG("QuickenTable::Decode instruction %x", instructions[m]);
        QUT_TMP_LOG("QuickenTable::Decode instruction %x", instructions[m]);
    }
#endif

    int32_t resolving_regs[QUT_MINIMAL_REG_SIZE] = {0};
    uint32_t unresolved_regs_bits = 0;
    bool resolving = false;

    while(i < amount) {
        uint8_t byte;
        ReadByte(byte, instructions, j, i, amount);
        QUT_DEBUG_LOG("Decode byte " BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(byte));
        switch (byte >> 6) {
            case 0:
                if (LIKELY(cfa_set_)) {
                    if (UNLIKELY(resolving)) {
                        uint8_t i = 0;
                        while(i < QUT_MINIMAL_REG_SIZE) {
                            if (resolving_regs[i] > 0) {
                                resolving_regs[i] -= (byte << 2);
                            }
                            i++;
                        }
                    }
                } else {
                    cfa_ += (byte << 2);
                }
                break;
            case 1:
                if (LIKELY(cfa_set_)) {
                    if (UNLIKELY(resolving)) {
                        uint8_t i = 0;
                        while(i < QUT_MINIMAL_REG_SIZE) {
                            if (resolving_regs[i] > 0) {
                                resolving_regs[i] += (byte << 2);
                            }
                            i++;
                        }
                    }
                } else {
                    cfa_ -= (byte << 2);
                }
                break;
            case 2:
                switch (byte) {
                    case QUT_INSTRUCTION_VSP_SET_BY_R7_OP:
                        if (UNLIKELY(cfa_set_)) {
                            return false;
                        } else {
                            if (!CHECK_REG_BIT(regs_bits_, R7_REG_BIT)) {
                                resolving_regs[R7_REG_BIT] = cfa_next_;
                                resolving = true;
                                SET_REG_BIT(regs_bits_tmp_, R7_REG_BIT);
                            }
                            cfa_set_ = true;
                        }
                        VSP_SET_BY_MACRO(R7, 0);
                        break;
                    case QUT_INSTRUCTION_VSP_SET_BY_R7_PROLOGUE_OP:
                        if (UNLIKELY(cfa_set_)) {
                            return false;
                        } else {
                            if (!CHECK_REG_BIT(regs_bits_, R7_REG_BIT)) {
                                resolving_regs[R7_REG_BIT] = cfa_next_ - 8;
                                unresolved_regs_bits |= (1 << R7_REG_BIT);
                                resolving = true;
                                SET_REG_BIT(regs_bits_tmp_, R7_REG_BIT);
                            }
                            if (!CHECK_REG_BIT(regs_bits_, LR_REG_BIT)) {
                                resolving_regs[LR_REG_BIT] = cfa_next_ - 4;
                                unresolved_regs_bits |= (1 << LR_REG_BIT);
                                resolving = true;
                                SET_REG_BIT(regs_bits_tmp_, LR_REG_BIT);
                            }
                            cfa_set_ = true;
                        }
                        break;
                    case QUT_INSTRUCTION_VSP_SET_BY_R11_OP:
                        if (UNLIKELY(cfa_set_)) {
                            return false;
                        } else {
                            if (!CHECK_REG_BIT(regs_bits_, R11_REG_BIT)) {
                                resolving_regs[R11_REG_BIT] = cfa_next_;
                                resolving = true;
                                SET_REG_BIT(regs_bits_tmp_, R11_REG_BIT);
                            }
                            cfa_set_ = true;
                        }
                        break;
                    case QUT_INSTRUCTION_VSP_SET_BY_R11_PROLOGUE_OP:
                        if (UNLIKELY(cfa_set_)) {
                            return false;
                        } else {
                            if (!CHECK_REG_BIT(regs_bits_, R11_REG_BIT)) {
                                resolving_regs[R11_REG_BIT] = cfa_next_ - 8;
                                resolving = true;
                                unresolved_regs_bits |= (1 << R11_REG_BIT);
                                SET_REG_BIT(regs_bits_tmp_, R11_REG_BIT);
                            }
                            if (!CHECK_REG_BIT(regs_bits_, LR_REG_BIT)) {
                                resolving_regs[LR_REG_BIT] = cfa_next_ - 4;
                                resolving = true;
                                unresolved_regs_bits |= (1 << LR_REG_BIT);
                                SET_REG_BIT(regs_bits_tmp_, LR_REG_BIT);
                            }
                            cfa_set_ = true;
                        }
                        break;
                    case QUT_INSTRUCTION_VSP_SET_BY_SP_OP:
//                        cfa_ = SP(regs_);
                        cfa_set_ = true;
                        break;
                    case QUT_INSTRUCTION_VSP_SET_BY_JNI_SP_OP: {
//                        uint8_t byte;
//                        ReadByte(byte, instructions, j, i, amount);
//                        cfa_ = R10(regs_) + ((byte & 0x7f) << 2);
//
//                        QUT_DEBUG_LOG("Decode QUT_INSTRUCTION_VSP_SET_BY_JNI_SP_OP %x r10 %x, cfa_ %x, offset %x, " BYTE_TO_BINARY_PATTERN,
//                                      (uint32_t)byte, R10(regs_), cfa_, (uint32_t)((byte & 0x7f) << 2), BYTE_TO_BINARY(byte));
                        uint8_t byte;
                        ReadByte(byte, instructions, j, i, amount);
                        if (UNLIKELY(cfa_set_)) {
                            return false;
                        } else {
                            if (!CHECK_REG_BIT(regs_bits_, R10_REG_BIT)) {
                                resolving_regs[R10_REG_BIT] = R10(regs_) - ((byte & 0x7f) << 2);
                                resolving = true;
                                SET_REG_BIT(regs_bits_tmp_, R10_REG_BIT);
                            }
                            cfa_set_ = true;
                        }
                        break;
                    }
                    case QUT_INSTRUCTION_VSP_SET_IMM_OP: {
                        int64_t value = 0;// TODO overflow
                        DecodeSLEB128(value, instructions, amount, j, i)
//                        cfa_ = value;
                        cfa_set_ = true;
                        break;
                    }
                    case QUT_INSTRUCTION_VSP_SET_BY_R7_IMM_OP: {
                        uint8_t byte;
                        ReadByte(byte, instructions, j, i, amount);
//                        cfa_ = R7(regs_) + ((byte & 0x7f) << 2);
                        if (UNLIKELY(cfa_set_)) {
                            return false;
                        } else {
                            if (!CHECK_REG_BIT(regs_bits_, R7_REG_BIT)) {
                                resolving_regs[R7_REG_BIT] = R7(regs_) - ((byte & 0x7f) << 2);
                                resolving = true;
                                SET_REG_BIT(regs_bits_tmp_, R7_REG_BIT);
                            }
                            cfa_set_ = true;
                        }
                        break;
                    }
                    case QUT_INSTRUCTION_VSP_SET_BY_R11_IMM_OP: {
                        uint8_t byte;
                        ReadByte(byte, instructions, j, i, amount);
//                        cfa_ = R11(regs_) + ((byte & 0x7f) << 2);
                        if (UNLIKELY(cfa_set_)) {
                            return false;
                        } else {
                            if (!CHECK_REG_BIT(regs_bits_, R11_REG_BIT)) {
                                resolving_regs[R11_REG_BIT] = R11(regs_) - ((byte & 0x7f) << 2);
                                resolving = true;
                                SET_REG_BIT(regs_bits_tmp_, R11_REG_BIT);
                            }
                            cfa_set_ = true;
                        }
                        break;
                    }
                    case QUT_INSTRUCTION_DEX_PC_SET_OP: {
//                        QUT_DEBUG_LOG("Decode byte QUT_INSTRUCTION_DEX_PC_SET_OP");
//                        dex_pc_ = R4(regs_);
                        break;
                    }
                    case QUT_END_OF_INS_OP: {
//                        QUT_DEBUG_LOG("Decode byte QUT_END_OF_INS_OP");
                        return true;
                    }
                    case QUT_FINISH_OP: {
//                        QUT_DEBUG_LOG("Decode byte QUT_FINISH_OP");
//                        PC(regs_) = 0;
//                        LR(regs_) = 0;
                        return true;
                    }
                    default: {
                        switch (byte & 0xf0) {
                            case QUT_INSTRUCTION_R4_OFFSET_OP_PREFIX: {
                                REG_OFFSET_MACRO(R4, ((byte & 0xf) << 2));
                                break;
                            }
                            case QUT_INSTRUCTION_R7_OFFSET_OP_PREFIX: {
                                REG_OFFSET_MACRO(R7, ((byte & 0xf) << 2));
                                break;
                            }
                        }
                    }
                }
                break;
            default:
                switch (byte & 0xf0) {
                    case QUT_INSTRUCTION_R10_OFFSET_OP_PREFIX:
                        REG_OFFSET_MACRO(R10, ((byte & 0xf) << 2));
                        break;
                    case QUT_INSTRUCTION_R11_OFFSET_OP_PREFIX:
                        REG_OFFSET_MACRO(R11, ((byte & 0xf) << 2));
                        break;
                    case QUT_INSTRUCTION_LR_OFFSET_OP_PREFIX:
                        break;
                    default:
                        uint64_t value = 0;
                        switch (byte) {
                            case QUT_INSTRUCTION_R7_OFFSET_SLEB128_OP:
                                DecodeSLEB128(value, instructions, amount, j, i)
                                UNRESOLVED_VSP_SET_BY_MACRO(R7, (uint32_t)value);
                                break;
                            case QUT_INSTRUCTION_R10_OFFSET_SLEB128_OP:
                                DecodeSLEB128(value, instructions, amount, j, i)
                                UNRESOLVED_VSP_SET_BY_MACRO(R10, (uint32_t)value);
                                break;
                            case QUT_INSTRUCTION_R11_OFFSET_SLEB128_OP:
                                DecodeSLEB128(value, instructions, amount, j, i)
                                UNRESOLVED_VSP_SET_BY_MACRO(R11, (uint32_t)value);
                                break;
                            case QUT_INSTRUCTION_SP_OFFSET_SLEB128_OP:
                                DecodeSLEB128(value, instructions, amount, j, i)
                                break;
                            case QUT_INSTRUCTION_LR_OFFSET_SLEB128_OP:
                                DecodeSLEB128(value, instructions, amount, j, i)
                                UNRESOLVED_VSP_SET_BY_MACRO(LR, (uint32_t)value);
                                break;
                            case QUT_INSTRUCTION_PC_OFFSET_SLEB128_OP:
                                DecodeSLEB128(value, instructions, amount, j, i)
                                break;
                            case QUT_INSTRUCTION_VSP_OFFSET_SLEB128_OP:
                                DecodeSLEB128(value, instructions, amount, j, i)
                                if (LIKELY(cfa_set_)) {
                                    if (resolving) {
                                        uint8_t i = 0;
                                        while(i < QUT_MINIMAL_REG_SIZE) {
                                            if (resolving_regs[i] > 0) {
                                                resolving_regs[i] -= (uint32_t)value;
                                            }
                                            i++;
                                        }
                                    }
                                } else {
                                    cfa_ += (uint32_t)value;
                                }
                                break;
                            default:
                                QUT_DEBUG_LOG("Decode byte UNKNOWN");
                                return false;
                        }
                        break;
                }
                break;
        }
    }

    for (uint8_t i = 0; i < QUT_MINIMAL_REG_SIZE; i++) {
        if (resolving_regs[i] > 0) {
            if (unresolved_regs_bits & (1 >> i)) {
                regs_[i] = resolving_regs[i];
            } else {
                regs_[i] = *((uint32_t *)(resolving_regs[i]));
            }
        }
    }

#pragma clang diagnostic pop

    return true;
}

inline bool QuickenTableReverse::DecodeReverse(const uptr* instructions, const size_t amount, const size_t start_pos) {
#ifdef __arm__
    return Decode32Reverse(instructions, amount, start_pos);
#else
    return Decode64Reverse(instructions, amount, start_pos);
#endif
}



bool QuickenTableReverse::EvalReverse(size_t entry_offset) {

    uptr command = fut_sections_->quidx[entry_offset + 1];

    QUT_DEBUG_LOG("QuickenTable::EvalReverse entry_offset %u, add %llx, command %llx",
                  (uint32_t)entry_offset, (uint64_t)fut_sections_->quidx[entry_offset],
                  (uint64_t)fut_sections_->quidx[entry_offset + 1]);

    if (command >> (sizeof(uptr) * 8 - 1)) {
        return DecodeReverse(&command, 1, QUT_TBL_ROW_SIZE - 1); // compact
    } else {
        size_t row_count = (command >> ((sizeof(uptr) - 1) * 8)) & 0x7f;
        size_t row_offset = command & 0xffffff;
        QUT_DEBUG_LOG("QuickenTable::EvalReverse row_count %u, row_offset %u", (uint32_t)row_count, (uint32_t)row_offset);
        return DecodeReverse(&fut_sections_->qutbl[row_offset], row_count, QUT_TBL_ROW_SIZE);
    }
}

}  // namespace wechat_backtrace
