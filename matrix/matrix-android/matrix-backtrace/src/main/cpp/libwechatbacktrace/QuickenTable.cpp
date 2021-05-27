//
// Created by Carl on 2020-09-21.
//
#include <vector>
#include <utility>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <memory>

#include <android-base/logging.h>
#include <include/unwindstack/MachineArm.h>
#include <include/unwindstack/Memory.h>
#include <QuickenInstructions.h>
#include <MinimalRegs.h>
#include <sys/mman.h>
#include "QuickenTable.h"
#include "Log.h"

namespace wechat_backtrace {

    using namespace std;
    using namespace unwindstack;

#define ReturnIndexOverflow(_i, _amount)  \
        if (_i >= _amount) { \
            return QUT_ERROR_TABLE_INDEX_OVERFLOW; \
        }

#define NoReturnIndexOverflow(_i, _amount)

#define IterateNextByteIdx(_j, _i, _amount, _ReturnIndexOverflow)  \
    { \
        _j--; \
        if (_j < 0) { \
            _j = QUT_TBL_ROW_SIZE; \
            _i++; \
            _ReturnIndexOverflow(_i, _amount) \
        } \
    }
#define ReadByte(_byte, _instructions, _j, _i, _amount) \
    { \
    _byte = (uint8_t)(_instructions[_i] >> (8 * _j) & 0xff); \
    IterateNextByteIdx(_j, _i, _amount, NoReturnIndexOverflow) \
    }

#define DecodeSLEB128(_value, _instructions, _amount, _j, _i) \
    { \
        unsigned Shift = 0; \
        uint8_t Byte; \
        do { \
            ReadByte(Byte, _instructions, _j, _i, _amount) \
            _value |= (uint64_t(Byte & 0x7f) << Shift); \
            Shift += 7; \
        } while (Byte >= 0x80 && (_i < _amount)); \
        if (Shift < 64 && (Byte & 0x40)) _value |= (-1ULL) << Shift; \
        if (UNLIKELY(Byte >= 0x80)) { \
            return QUT_ERROR_TABLE_INDEX_OVERFLOW; \
        } \
    }

    inline bool QuickenTable::ReadStack(const uptr addr, uptr *value) {
        if (UNLIKELY(addr > stack_top_ || addr < stack_bottom_)) {
            return false;
        }

        *value = *((uptr *) addr);

        return true;
    }

    inline QutErrorCode
    QuickenTable::Decode32(const uint32_t *const instructions, const size_t amount,
                           const size_t start_pos) {

        if (log) {
            QUT_LOG("QuickenTable::Decode32 instructions %llx, amount %u, start_pos %u",
                    (ullint_t) instructions, (uint32_t) amount, (uint32_t) start_pos);
            int32_t m = start_pos;
            size_t n = 0;
            while (n < amount) {
                uint8_t byte;
                ReadByte(byte, instructions, m, n, amount);
                QUT_LOG("Decode32 instructions "
                                BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(byte));
            }
        }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wint-to-pointer-cast"

        int32_t j = start_pos;
        size_t i = 0;

        while (i < amount) {
            uint8_t byte;
            ReadByte(byte, instructions, j, i, amount);
            switch (byte >> 6) {
                case 0:
                    // 00nn nnnn : vsp = vsp + (nnnnnn << 2) ; # (nnnnnnn << 2) in [0, 0xfc]
                    cfa_ += (byte << 2);
                    break;
                case 1:
                    // 01nn nnnn : vsp = vsp - (nnnnnn << 2) ; # (nnnnnnn << 2) in [0, 0xfc]
                    cfa_ -= (uint8_t)(byte << 2);
                    break;
                case 2:
                    switch (byte) {
                        case QUT_INSTRUCTION_VSP_SET_BY_R7_OP:
                            cfa_ = R7(regs_);
                            break;
                        case QUT_INSTRUCTION_VSP_SET_BY_R7_PROLOGUE_OP:
                            cfa_ = R7(regs_) + 8;
                            if (UNLIKELY(!ReadStack(cfa_ - 4, &LR(regs_)))) {
                                return QUT_ERROR_READ_STACK_FAILED;
                            }
                            if (UNLIKELY(!ReadStack(cfa_ - 8, &R7(regs_)))) {
                                return QUT_ERROR_READ_STACK_FAILED;
                            }
                            break;
                        case QUT_INSTRUCTION_VSP_SET_BY_R11_OP:
                            cfa_ = R11(regs_);
                            break;
                        case QUT_INSTRUCTION_VSP_SET_BY_R11_PROLOGUE_OP:
                            cfa_ = R11(regs_) + 8;
                            if (UNLIKELY(!ReadStack(cfa_ - 4, &LR(regs_)))) {
                                return QUT_ERROR_READ_STACK_FAILED;
                            }
                            if (UNLIKELY(!ReadStack(cfa_ - 8, &R11(regs_)))) {
                                return QUT_ERROR_READ_STACK_FAILED;
                            }
                            break;
                        case QUT_INSTRUCTION_VSP_SET_BY_SP_OP:
                            cfa_ = SP(regs_);
                            break;
                        case QUT_INSTRUCTION_VSP_SET_BY_JNI_SP_OP: {
                            uint8_t byte;
                            ReadByte(byte, instructions, j, i, amount);
                            cfa_ = R10(regs_) + ((byte & 0x7f) << 2);
                            break;
                        }
                        case QUT_INSTRUCTION_VSP_SET_IMM_OP: {
                            int64_t value = 0;// TODO overflow
                            DecodeSLEB128(value, instructions, amount, j, i)
                            cfa_ = value;
                            break;
                        }
                        case QUT_INSTRUCTION_VSP_SET_BY_R7_IMM_OP: {
                            uint8_t byte;
                            ReadByte(byte, instructions, j, i, amount);
                            cfa_ = R7(regs_) + ((byte & 0x7f) << 2);
                            break;
                        }
                        case QUT_INSTRUCTION_VSP_SET_BY_R11_IMM_OP: {
                            uint8_t byte;
                            ReadByte(byte, instructions, j, i, amount);
                            cfa_ = R11(regs_) + ((byte & 0x7f) << 2);
                            break;
                        }
                        case QUT_INSTRUCTION_DEX_PC_SET_OP: {
                            dex_pc_ = R4(regs_);
                            break;
                        }
                        case QUT_END_OF_INS_OP: {
                            return QUT_ERROR_NONE;
                        }
                        case QUT_FINISH_OP: {
                            PC(regs_) = 0;
                            LR(regs_) = 0;
                            return QUT_ERROR_NONE;
                        }
                        default: {
                            switch (byte & 0xf0) {
                                case QUT_INSTRUCTION_R4_OFFSET_OP_PREFIX: {
                                    if (UNLIKELY(
                                            !ReadStack((cfa_ - ((byte & 0xf) << 2)), &R4(regs_)))) {
                                        return QUT_ERROR_READ_STACK_FAILED;
                                    }
                                    break;
                                }
                                case QUT_INSTRUCTION_R7_OFFSET_OP_PREFIX: {
                                    if (UNLIKELY(
                                            !ReadStack((cfa_ - ((byte & 0xf) << 2)), &R7(regs_)))) {
                                        return QUT_ERROR_READ_STACK_FAILED;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    break;
                default:
                    switch (byte & 0xf0) {
                        case QUT_INSTRUCTION_R10_OFFSET_OP_PREFIX:
                            if (UNLIKELY(!ReadStack((cfa_ - ((byte & 0xf) << 2)), &R10(regs_)))) {
                                return QUT_ERROR_READ_STACK_FAILED;
                            }
                            break;
                        case QUT_INSTRUCTION_R11_OFFSET_OP_PREFIX:
                            if (UNLIKELY(!ReadStack((cfa_ - ((byte & 0xf) << 2)), &R11(regs_)))) {
                                return QUT_ERROR_READ_STACK_FAILED;
                            }
                            break;
                        case QUT_INSTRUCTION_LR_OFFSET_OP_PREFIX:
                            if (UNLIKELY(!ReadStack((cfa_ - ((byte & 0xf) << 2)), &LR(regs_)))) {
                                return QUT_ERROR_READ_STACK_FAILED;
                            }
                            break;
                        default:
                            int64_t value = 0;
                            switch (byte) {
                                case QUT_INSTRUCTION_R7_OFFSET_SLEB128_OP: {
                                    DecodeSLEB128(value, instructions, amount, j, i);
                                    if (UNLIKELY(
                                            !ReadStack((cfa_ - (int32_t) value), &R7(regs_)))) {
                                        return QUT_ERROR_READ_STACK_FAILED;
                                    }
                                    break;
                                }
                                case QUT_INSTRUCTION_R10_OFFSET_SLEB128_OP: {
                                    DecodeSLEB128(value, instructions, amount, j, i);
                                    if (UNLIKELY(
                                            !ReadStack((cfa_ - (int32_t) value), &R10(regs_)))) {
                                        return QUT_ERROR_READ_STACK_FAILED;
                                    }
                                    break;
                                }
                                case QUT_INSTRUCTION_R11_OFFSET_SLEB128_OP: {
                                    DecodeSLEB128(value, instructions, amount, j, i);
                                    if (UNLIKELY(
                                            !ReadStack((cfa_ - (int32_t) value), &R11(regs_)))) {
                                        return QUT_ERROR_READ_STACK_FAILED;
                                    }
                                    break;
                                }
                                case QUT_INSTRUCTION_SP_OFFSET_SLEB128_OP: {
                                    DecodeSLEB128(value, instructions, amount, j, i);
                                    if (UNLIKELY(
                                            !ReadStack((cfa_ - (int32_t) value), &SP(regs_)))) {
                                        return QUT_ERROR_READ_STACK_FAILED;
                                    }
                                    break;
                                }
                                case QUT_INSTRUCTION_LR_OFFSET_SLEB128_OP: {
                                    DecodeSLEB128(value, instructions, amount, j, i);
                                    if (UNLIKELY(
                                            !ReadStack((cfa_ - (int32_t) value), &LR(regs_)))) {
                                        return QUT_ERROR_READ_STACK_FAILED;
                                    }
                                    break;
                                }
                                case QUT_INSTRUCTION_PC_OFFSET_SLEB128_OP: {
                                    DecodeSLEB128(value, instructions, amount, j, i);
                                    if (UNLIKELY(
                                            !ReadStack((cfa_ - (int32_t) value), &PC(regs_)))) {
                                        return QUT_ERROR_READ_STACK_FAILED;
                                    }
                                    pc_set_ = true;
                                    break;
                                }
                                case QUT_INSTRUCTION_VSP_OFFSET_SLEB128_OP: {
                                    DecodeSLEB128(value, instructions, amount, j, i)
                                    cfa_ += (int32_t) value;
                                    break;
                                }
                                default:
                                    return QUT_ERROR_INVALID_QUT_INSTR;
                            }
                            break;
                    }
                    break;
            }
        }

#pragma clang diagnostic pop

        return QUT_ERROR_NONE;
    }

    inline QutErrorCode QuickenTable::Decode64(const uint64_t *instructions, const size_t amount,
                                               const size_t start_pos) {

        if (log) {
            QUT_DEBUG_LOG(
                    "QuickenTable::Decode64 instructions %llx, amount %zu, start_pos %zu",
                    (ullint_t) instructions, amount, start_pos);

            for (size_t m = 0; m < amount; m++) {
                QUT_DEBUG_LOG("QuickenTable::Decode64 instruction %llx",
                              (ullint_t) instructions[m]);
            }

            int32_t m = start_pos;
            size_t n = 0;
            while (n < amount) {
                uint8_t byte;
                ReadByte(byte, instructions, m, n, amount);
                QUT_LOG("Decode64 instructions byte "
                                BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(byte));
            }
        }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wint-to-pointer-cast"

        int32_t j = start_pos;
        size_t i = 0;

        while (i < amount) {
            uint8_t byte;
            ReadByte(byte, instructions, j, i, amount);
            switch (byte >> 6) {
                case 0:
                    // 00nn nnnn : vsp = vsp + (nnnnnn << 2) ; # (nnnnnnn << 2) in [0, 0xfc]
                    cfa_ += (byte << 2);
                    break;
                case 1:
                    // 01nn nnnn : vsp = vsp - (nnnnnn << 2) ; # (nnnnnnn << 2) in [0, 0xfc]
                    cfa_ -= (uint8_t)(byte << 2);
                    break;
                case 2:
                    switch (byte) {
                        case QUT_INSTRUCTION_VSP_SET_BY_X29_OP:
                            cfa_ = R29(regs_);
                            break;
                        case QUT_INSTRUCTION_VSP_SET_BY_X29_PROLOGUE_OP:
                            cfa_ = R29(regs_) + 16;
                            if (UNLIKELY(!ReadStack((cfa_ - 8), &LR(regs_)))) {
                                return QUT_ERROR_READ_STACK_FAILED;
                            }
                            if (UNLIKELY(!ReadStack((cfa_ - 16), &R29(regs_)))) {
                                return QUT_ERROR_READ_STACK_FAILED;
                            }
                            break;
                        case QUT_INSTRUCTION_VSP_SET_BY_SP_OP:
                            cfa_ = SP(regs_);
                            break;
                        case QUT_INSTRUCTION_VSP_SET_BY_JNI_SP_OP: {
                            uint8_t byte;
                            ReadByte(byte, instructions, j, i, amount);
                            cfa_ = R28(regs_) + ((byte & 0x7f) << 2);
                            break;
                        }
                        case QUT_INSTRUCTION_VSP_SET_IMM_OP: {
                            int64_t value = 0; // TODO overflow
                            DecodeSLEB128(value, instructions, amount, j, i)
                            cfa_ = value;
                            break;
                        }
                        case QUT_INSTRUCTION_VSP_SET_BY_X29_IMM_OP: {
                            uint8_t byte;
                            ReadByte(byte, instructions, j, i, amount);
                            cfa_ = R29(regs_) + ((byte & 0x7f) << 2);
                            break;
                        }
                        case QUT_INSTRUCTION_VSP_SET_BY_X29_OFFSET_OP: {
                            int64_t value = 0;
                            DecodeSLEB128(value, instructions, amount, j, i)
                            cfa_ = R29(regs_) + value;
                        }
                        case QUT_INSTRUCTION_DEX_PC_SET_OP: {
                            dex_pc_ = R20(regs_);
                            break;
                        }
                        case QUT_END_OF_INS_OP: {
                            return QUT_ERROR_NONE;
                        }
                        case QUT_FINISH_OP: {
                            PC(regs_) = 0;
                            LR(regs_) = 0;
                            return QUT_ERROR_NONE;
                        }
                        default: {
                            switch (byte & 0xf0) {
                                case QUT_INSTRUCTION_X20_OFFSET_OP_PREFIX: {
                                    if (UNLIKELY(!ReadStack((cfa_ - ((byte & 0xf) << 2)),
                                                            &R20(regs_)))) {
                                        return QUT_ERROR_READ_STACK_FAILED;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    break;
                default:
                    switch (byte & 0xf0) {
                        case QUT_INSTRUCTION_X28_OFFSET_OP_PREFIX:
                            if (UNLIKELY(!ReadStack((cfa_ - ((byte & 0xf) << 2)), &R28(regs_)))) {
                                return QUT_ERROR_READ_STACK_FAILED;
                            }
                            break;
                        case QUT_INSTRUCTION_X29_OFFSET_OP_PREFIX:
                            if (UNLIKELY(!ReadStack((cfa_ - ((byte & 0xf) << 2)), &R29(regs_)))) {
                                return QUT_ERROR_READ_STACK_FAILED;
                            }
                            if (R29(regs_) == 0) {  // reach end
                                return QUT_ERROR_NONE;
                            }
                            break;
                        case QUT_INSTRUCTION_LR_OFFSET_OP_PREFIX:
                            if (UNLIKELY(!ReadStack((cfa_ - ((byte & 0xf) << 2)), &LR(regs_)))) {
                                return QUT_ERROR_READ_STACK_FAILED;
                            }
                            break;
                        default:
                            int64_t value = 0;
                            switch (byte) {
                                case QUT_INSTRUCTION_X20_OFFSET_SLEB128_OP: {
                                    DecodeSLEB128(value, instructions, amount, j, i)
                                    if (UNLIKELY(
                                            !ReadStack((cfa_ - (int64_t) value), &R20(regs_)))) {
                                        return QUT_ERROR_READ_STACK_FAILED;
                                    }
                                    break;
                                }
                                case QUT_INSTRUCTION_X28_OFFSET_SLEB128_OP: {
                                    DecodeSLEB128(value, instructions, amount, j, i)
                                    if (UNLIKELY(
                                            !ReadStack((cfa_ - (int64_t) value), &R28(regs_)))) {
                                        return QUT_ERROR_READ_STACK_FAILED;
                                    }
                                    break;
                                }
                                case QUT_INSTRUCTION_X29_OFFSET_SLEB128_OP: {
                                    DecodeSLEB128(value, instructions, amount, j, i)
                                    if (UNLIKELY(
                                            !ReadStack((cfa_ - (int64_t) value), &R29(regs_)))) {
                                        return QUT_ERROR_READ_STACK_FAILED;
                                    }
                                    if (UNLIKELY(R29(regs_) == 0)) { // reach end
                                        return QUT_ERROR_NONE;
                                    }
                                    break;
                                }
                                case QUT_INSTRUCTION_SP_OFFSET_SLEB128_OP: {
                                    DecodeSLEB128(value, instructions, amount, j, i)
                                    if (UNLIKELY(
                                            !ReadStack((cfa_ - (int64_t) value), &SP(regs_)))) {
                                        return QUT_ERROR_READ_STACK_FAILED;
                                    }
                                    break;
                                }
                                case QUT_INSTRUCTION_LR_OFFSET_SLEB128_OP: {
                                    DecodeSLEB128(value, instructions, amount, j, i)
                                    if (UNLIKELY(
                                            !ReadStack((cfa_ - (int64_t) value), &LR(regs_)))) {
                                        return QUT_ERROR_READ_STACK_FAILED;
                                    }
                                    break;
                                }
                                case QUT_INSTRUCTION_PC_OFFSET_SLEB128_OP: {
                                    DecodeSLEB128(value, instructions, amount, j, i)
                                    if (UNLIKELY(
                                            !ReadStack((cfa_ - (int64_t) value), &PC(regs_)))) {
                                        return QUT_ERROR_READ_STACK_FAILED;
                                    }
                                    pc_set_ = true;
                                    break;
                                }
                                case QUT_INSTRUCTION_VSP_OFFSET_SLEB128_OP: {
                                    DecodeSLEB128(value, instructions, amount, j, i)
                                    cfa_ += (int64_t) value;
                                    break;
                                }
                                default:
                                    return QUT_ERROR_INVALID_QUT_INSTR;
                            }
                            break;
                    }
                    break;
            }
        }

#pragma clang diagnostic pop

        return QUT_ERROR_NONE;
    }

    inline QutErrorCode
    QuickenTable::Decode(const uptr *instructions, const size_t amount, const size_t start_pos) {
#ifdef __arm__
        return Decode32(instructions, amount, start_pos);
#else
        return Decode64(instructions, amount, start_pos);
#endif
    }

    QutErrorCode QuickenTable::Eval(size_t entry_offset) {
        uptr command = qut_sections_->quidx[entry_offset + 1];

//        log = log_entry == entry_offset;

        if (log) {
            QUT_LOG("QuickenTable::Eval entry_offset %u, add %llx, command %llx",
                    (uint32_t) entry_offset, (ullint_t) qut_sections_->quidx[entry_offset],
                    (ullint_t) qut_sections_->quidx[entry_offset + 1]);
        }

        if (command >> ((sizeof(uptr) * 8) - 1)) {
            return Decode(&command, 1, QUT_TBL_ROW_SIZE - 1); // compact
        } else {
            size_t row_count = (command >> ((sizeof(uptr) - 1) * 8)) & 0x7f;
            size_t row_offset = command & 0xffffff;

            CHECK(row_offset + row_count <= qut_sections_->tbl_size);

            return Decode(&(qut_sections_->qutbl[row_offset]), row_count, QUT_TBL_ROW_SIZE);
        }
    }

}  // namespace wechat_backtrace
