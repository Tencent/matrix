//
// Created by Carl on 2020-10-29.
//

#ifndef _LIBWECHATBACKTRACE_QUICKEN_INSTRUCTIONS_H
#define _LIBWECHATBACKTRACE_QUICKEN_INSTRUCTIONS_H

#include <cstdint>
#include <vector>
#include <utility>
#include <memory>
#include <deque>

#include "Log.h"

#include "QuickenInstructions.h"
#include "QutStatistics.h"

#define _0000 0x0
#define _0001 0x1
#define _0010 0x2
#define _0011 0x3
#define _0100 0x4
#define _0101 0x5
#define _0110 0x6
#define _0111 0x7
#define _1000 0x8
#define _1001 0x9
#define _1010 0xa
#define _1011 0xb
#define _1100 0xc
#define _1101 0xd
#define _1110 0xe
#define _1111 0xf

#define OP_COMBINE(X1, X2) ((X1 << 4) + X2)
#define BIT(X) (_##X)
#define OP(X1, X2) OP_COMBINE(_##X1, _##X2)
#define IMM(N) (((1 << (N)) - 1) << 2)
#define IMM_ALIGNED(N) ((N) >> 2)
#define FILL_WITH_IMM(Prefix, Imm, Mask) ((Prefix) | (((1 << (Mask)) - 1) & IMM_ALIGNED((Imm))))

// 32-bit vsp set by fp
#define QUT_INSTRUCTION_VSP_SET_BY_R7_OP            OP(1000, 0000)
#define QUT_INSTRUCTION_VSP_SET_BY_R7_PROLOGUE_OP   OP(1000, 0001)
#define QUT_INSTRUCTION_VSP_SET_BY_R11_OP           OP(1000, 0010)
#define QUT_INSTRUCTION_VSP_SET_BY_R11_PROLOGUE_OP  OP(1000, 0011)
#define QUT_INSTRUCTION_VSP_SET_BY_R7_IMM_OP        OP(1000, 0101)
#define QUT_INSTRUCTION_VSP_SET_BY_R11_IMM_OP       OP(1000, 0110)

// 64-bit vsp set by fp
#define QUT_INSTRUCTION_VSP_SET_BY_X29_OP           OP(1000, 0000)
#define QUT_INSTRUCTION_VSP_SET_BY_X29_PROLOGUE_OP  OP(1000, 0001)
#define QUT_INSTRUCTION_VSP_SET_BY_X29_IMM_OP       OP(1000, 0101)

#define QUT_INSTRUCTION_VSP_SET_BY_SP_OP            OP(1000, 0100)

#define QUT_INSTRUCTION_VSP_SET_BY_JNI_SP_OP        OP(1001, 0101)
#define QUT_INSTRUCTION_VSP_SET_IMM_OP              OP(1001, 0110)
#define QUT_INSTRUCTION_DEX_PC_SET_OP               OP(1001, 0111)
#define QUT_END_OF_INS_OP                           OP(1001, 1001)
#define QUT_FINISH_OP                               OP(1001, 1111)

#define QUT_INSTRUCTION_R7_OFFSET_SLEB128_OP        OP(1111, 1001)
#define QUT_INSTRUCTION_R10_OFFSET_SLEB128_OP       OP(1111, 1010)
#define QUT_INSTRUCTION_R11_OFFSET_SLEB128_OP       OP(1111, 1011)

#define QUT_INSTRUCTION_X20_OFFSET_SLEB128_OP       OP(1111, 1001)
#define QUT_INSTRUCTION_X28_OFFSET_SLEB128_OP       OP(1111, 1010)
#define QUT_INSTRUCTION_X29_OFFSET_SLEB128_OP       OP(1111, 1011)

#define QUT_INSTRUCTION_SP_OFFSET_SLEB128_OP        OP(1111, 1100)
#define QUT_INSTRUCTION_LR_OFFSET_SLEB128_OP        OP(1111, 1101)
#define QUT_INSTRUCTION_PC_OFFSET_SLEB128_OP        OP(1111, 1110)
#define QUT_INSTRUCTION_VSP_OFFSET_SLEB128_OP       OP(1111, 1111)

#define QUT_INSTRUCTION_R4_OFFSET_OP_PREFIX         (BIT(1010) << 4)
#define QUT_INSTRUCTION_R7_OFFSET_OP_PREFIX         (BIT(1011) << 4)
#define QUT_INSTRUCTION_R10_OFFSET_OP_PREFIX        (BIT(1100) << 4)
#define QUT_INSTRUCTION_R11_OFFSET_OP_PREFIX        (BIT(1101) << 4)

#define QUT_INSTRUCTION_X20_OFFSET_OP_PREFIX        (BIT(1010) << 4)
#define QUT_INSTRUCTION_X28_OFFSET_OP_PREFIX        (BIT(1100) << 4)
#define QUT_INSTRUCTION_X29_OFFSET_OP_PREFIX        (BIT(1101) << 4)

#define QUT_INSTRUCTION_LR_OFFSET_OP_PREFIX         (BIT(1110) << 4)


#define EncodeSLEB128_PUSHBACK(EncodedQueue, IMM) { \
        uint8_t encoded_bytes[5]; \
        size_t size = EncodeSLEB128(IMM, encoded_bytes); \
        for (size_t i = 0; i < size; i++) { \
            EncodedQueue.push_back(encoded_bytes[i]); \
        } \
    }

namespace wechat_backtrace {

    QUT_EXTERN_C_BLOCK

    enum QutInstruction : uint64_t {
        // Do not change this order!!!
                QUT_INSTRUCTION_R4_OFFSET = 0,  // r4_offset
        QUT_INSTRUCTION_R7_OFFSET = 1,      // r7_offset
        QUT_INSTRUCTION_R10_OFFSET = 2,     // r10_offset
        QUT_INSTRUCTION_R11_OFFSET = 3,     // r11_offset
        QUT_INSTRUCTION_SP_OFFSET = 4,      // sp_offset
        QUT_INSTRUCTION_LR_OFFSET = 5,      // lr_offset
        QUT_INSTRUCTION_PC_OFFSET = 6,      // pc_offset

        QUT_INSTRUCTION_X20_OFFSET = 7,     // x20_offset
        QUT_INSTRUCTION_X28_OFFSET = 8,     // x28_offset
        QUT_INSTRUCTION_X29_OFFSET = 9,     // x29_offset

        QUT_INSTRUCTION_VSP_OFFSET = 10,     // vsp_offset
        QUT_INSTRUCTION_VSP_SET_IMM = 11,    // vsp_set_imm

        QUT_INSTRUCTION_VSP_SET_BY_R7 = 12,  // vsp_set_by_r7
        QUT_INSTRUCTION_VSP_SET_BY_R11 = 13, // vsp_set_by_r11

        QUT_INSTRUCTION_VSP_SET_BY_X29 = 14, // vsp_set_by_x29

        QUT_INSTRUCTION_VSP_SET_BY_SP = 15,  // vsp_set_by_sp
        QUT_INSTRUCTION_VSP_SET_BY_JNI_SP = 16, // vsp_set_by_jni_sp
        QUT_INSTRUCTION_DEX_PC_SET = 17,  // dex_pc_set
        QUT_END_OF_INS = 18,
        QUT_FIN = 19,
        QUT_NOP = 20, // No meaning.
    };


    inline unsigned EncodeSLEB128(int64_t Value, uint8_t *p, unsigned PadTo = 0) {
        uint8_t *orig_p = p;
        unsigned Count = 0;
        bool More;
        do {
            uint8_t Byte = Value & 0x7f;
            // NOTE: this assumes that this signed shift is an arithmetic right shift.
            Value >>= 7;
            More = !((((Value == 0) && ((Byte & 0x40) == 0)) ||
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
        return (unsigned) (p - orig_p);
    }

    inline void CheckInstruction(uint64_t instruction) {

        uint32_t op = instruction >> 32;
        int32_t imm = instruction & 0xFFFFFFFF;

        if (op == QUT_INSTRUCTION_VSP_OFFSET) {
            if (imm > IMM(6) || imm < -IMM(6)) {
                QUT_STATISTIC_TIPS(InstructionOp, op, imm);
            }
        } else if (op == QUT_INSTRUCTION_VSP_SET_BY_R7 || op == QUT_INSTRUCTION_VSP_SET_BY_R11 ||
                   op == QUT_INSTRUCTION_VSP_SET_BY_JNI_SP ||
                   op == QUT_INSTRUCTION_VSP_SET_BY_X29) {
            if (imm < 0 || imm > IMM(7)) {
                QutStatisticType type;
                if (op == QUT_INSTRUCTION_VSP_SET_BY_R7) type = InstructionOpOverflowR7;
                else if (op == QUT_INSTRUCTION_VSP_SET_BY_R11) type = InstructionOpOverflowR11;
                else if (op == QUT_INSTRUCTION_VSP_SET_BY_JNI_SP) type = InstructionOpOverflowJNISP;
                else if (op == QUT_INSTRUCTION_VSP_SET_BY_X29) type = InstructionOpOverflowX29;
                else return;
                QUT_STATISTIC(type, op, imm);
            }
        } else if (op == QUT_INSTRUCTION_R4_OFFSET || op == QUT_INSTRUCTION_R7_OFFSET ||
                   op == QUT_INSTRUCTION_R10_OFFSET || op == QUT_INSTRUCTION_R11_OFFSET ||
                   op == QUT_INSTRUCTION_X20_OFFSET ||
                   op == QUT_INSTRUCTION_X28_OFFSET || op == QUT_INSTRUCTION_X29_OFFSET ||
                   op == QUT_INSTRUCTION_LR_OFFSET ||
                   op == QUT_INSTRUCTION_SP_OFFSET || op == QUT_INSTRUCTION_PC_OFFSET) {
            if (imm < 0 || imm > IMM(4)) {
                if (op == QUT_INSTRUCTION_R4_OFFSET || op == QUT_INSTRUCTION_X20_OFFSET) {
                    if (op == QUT_INSTRUCTION_R4_OFFSET) QUT_STATISTIC(InstructionOpOverflowR4, op,
                                                                       imm);
                    else if (op == QUT_INSTRUCTION_X20_OFFSET) QUT_STATISTIC(
                            InstructionOpOverflowX20, op, imm);
                } else {
                    QUT_STATISTIC_TIPS(InstructionOp, op, imm);
                }
            }
        }

    }

    inline bool
    _QuickenInstructionsEncode32(std::deque<uint64_t> &instructions, std::deque<uint8_t> &encoded,
                                 bool *prologue_conformed, bool log) {

// QUT encode for 32-bit:
//		00nn nnnn           : vsp = vsp + (nnnnnn << 2)             			; # (nnnnnnn << 2) in [0, 0xfc]
//      01nn nnnn           : vsp = vsp - (nnnnnn << 2)             			; # (nnnnnnn << 2) in [0, 0xfc]

//      1000 0000           : vsp = r7		              						; # r7 is fp reg in thumb mode
//      1000 0001           : vsp = r7 + 8, lr = [vsp - 4], sp = [vsp - 8]      ; # Have prologue
//      1000 0010           : vsp = r11		             						; # r11 is fp reg in arm mode
//      1000 0011           : vsp = r11 + 8, lr = [vsp - 4], sp = [vsp - 8]     ; # Have prologue
//      1000 0100           : vsp = sp                                    		; # XXX

//      1000 0101 0nnn nnnn : vsp = r7 + (nnnnnnn << 2)							;
//      1000 0110 0nnn nnnn : vsp = r11 + (nnnnnnn << 2)						;

//		1001 0101 0nnn nnnn : vsp = r10 + (nnnnnnn << 2)						; # (nnnnnnn << 2) in [0, 0x1fc],  0nnnnnnn is an one bit ULEB128
//		1001 0110 + SLEB128 : vsp = SLEB128							    		; # vsp set by IMM

//		1001 0111 			: dex_pc = r4										; # Dex pc is saved in r4

//		1001 1001			: End of instructions                				;
//		1001 1111			: Finish                							;

//		1010 nnnn 			: r4 = [vsp - (nnnn << 2)]     						; # (nnnn << 2) in [0, 0x3c]
//      1011 nnnn           : r7 = [vsp - (nnnn << 2)]     						; # Same as above
//		1100 nnnn           : r10 = [vsp - (nnnn << 2)]    						; # Same as above. r10 will be used while unwinding through JNI function
//      1101 nnnn           : r11 = [vsp - (nnnn << 2)]    						; # Same as above
//      1110 nnnn           : lr = [vsp - (nnnn << 2)]     						; # Same as above

//      1111 0xxx 			: Reserved											;
//      1111 1000			: Reserved											;

//      1111 1001 + SLEB128 : r7 = [vsp - SLEB128]  							; # [addr] means get value from pointed by addr
//      1111 1010 + SLEB128 : r10 = [vsp - SLEB128] 							; # Same as above
//      1111 1011 + SLEB128 : r11 = [vsp - SLEB128] 							; # Same as above
//      1111 1100 + SLEB128 : sp = [vsp - SLEB128]  							; # Same as above
//      1111 1101 + SLEB128 : lr = [vsp - SLEB128]  							; # Same as above
//      1111 1110 + SLEB128 : pc = [vsp - SLEB128]  							; # Same as above

//      1111 1111 + SLEB128 : vsp = vsp + SLEB128   							;

        while (!instructions.empty()) {

            uint64_t instruction = instructions.front();

            instructions.pop_front();

#ifdef QUT_STATISTIC_ENABLE
            CheckInstruction(instruction);
#endif

            int64_t op = instruction >> 32;
            int32_t imm = instruction & 0xFFFFFFFF;

            if (log) {
                QUT_DEBUG_LOG("Encoding instruction: %llx", (ullint_t) instruction);
                QUT_DEBUG_LOG("Encoding op: %llx, imm: %lld", (ullint_t) op, (llint_t) imm);
            }

            if (imm & 0x3) {
                QUT_STATISTIC(InstructionOpImmNotAligned, op, imm);
                if (log) {
                    QUT_DEBUG_LOG("InstructionOpImmNotAligned");
                }
                return false; // bad entry
            }

            uint8_t byte;
            switch (op) {
                case QUT_INSTRUCTION_VSP_OFFSET:
                    // imm ~ [-((1 << 6) - 1) << 2, ((1 << 6) - 1) << 2]
                    if (imm >= -IMM(6) & imm <= IMM(6)) {
                        // 00nn nnnn : vsp = vsp + (nnnnnn << 2)
                        // 01nn nnnn : vsp = vsp - (nnnnnn << 2)
                        byte = FILL_WITH_IMM(imm > 0 ? 0 : 0x40, imm, 6);
                        encoded.push_back(byte);
                    } else {
                        // 1111 1111 + SLEB128 : vsp = vsp + SLEB128
                        byte = QUT_INSTRUCTION_VSP_OFFSET_SLEB128_OP;
                        encoded.push_back(byte);
                        EncodeSLEB128_PUSHBACK(encoded, imm);
                    }
                    break;
                case QUT_INSTRUCTION_VSP_SET_BY_R7:
                case QUT_INSTRUCTION_VSP_SET_BY_R11: {

                    if (imm != 0) {
                        uint8_t byte;
                        byte = (op == QUT_INSTRUCTION_VSP_SET_BY_R7) ?
                               QUT_INSTRUCTION_VSP_SET_BY_R7_IMM_OP
                                                                     : QUT_INSTRUCTION_VSP_SET_BY_R11_IMM_OP;
                        encoded.push_back(byte);
                        if (imm > 0 && imm <= IMM(7)) {
                            byte = FILL_WITH_IMM(0, imm, 7);
                            encoded.push_back(byte);
                        } else {
                            if (log) {
                                QUT_DEBUG_LOG("QUT_INSTRUCTION_VSP_SET_BY_R11 Overflow");
                            }
                            return false; // overflow, bad entry
                        }
                        break;
                    } else {
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
                                if (!(next_op == QUT_INSTRUCTION_LR_OFFSET &&
                                      next_imm_lr == (next_imm - 4))) {
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
                                    instructions.pop_front(); // pop lr = vsp - 4
                                    instructions.pop_front(); // pop r7/r11 = vsp - 8
                                }
                            }
                        } while (false);
                        // 1000 0000 : vsp = r7                                      ;
                        // 1000 0001 : vsp = r7 + 8, ls = [vsp - 4], sp = [vsp - 8]  ; # have prologue
                        // 1000 0010 : vsp = r11                                     ;
                        // 1000 0011 : vsp = r11 + 8, ls = [vsp - 4], sp = [vsp - 8] ; # have prologue
                        uint8_t byte;
                        if (have_prologue) {
                            byte = (op == QUT_INSTRUCTION_VSP_SET_BY_R7) ?
                                   QUT_INSTRUCTION_VSP_SET_BY_R7_PROLOGUE_OP
                                                                         : QUT_INSTRUCTION_VSP_SET_BY_R11_PROLOGUE_OP;

                            *prologue_conformed = true;
                        } else {
                            byte = (op == QUT_INSTRUCTION_VSP_SET_BY_R7) ?
                                   QUT_INSTRUCTION_VSP_SET_BY_R7_OP
                                                                         : QUT_INSTRUCTION_VSP_SET_BY_R11_OP;
                        }

                        encoded.push_back(byte);

                        break;
                    }
                }
                case QUT_INSTRUCTION_VSP_SET_BY_SP:
                    // 1000 0100 : vsp = sp
                    byte = QUT_INSTRUCTION_VSP_SET_BY_SP_OP;
                    encoded.push_back(byte);
                    break;
                case QUT_INSTRUCTION_VSP_SET_BY_JNI_SP:
                    byte = QUT_INSTRUCTION_VSP_SET_BY_JNI_SP_OP;
                    encoded.push_back(byte);
                    byte = FILL_WITH_IMM(0, imm, 7);
                    encoded.push_back(byte);
                    if (log) {
                        QUT_DEBUG_LOG("QUT_INSTRUCTION_VSP_SET_BY_JNI_SP %x %x %x",
                                      QUT_INSTRUCTION_VSP_SET_BY_JNI_SP_OP, (uint32_t) byte,
                                      (uint32_t) imm);
                    }
                    break;
                case QUT_INSTRUCTION_VSP_SET_IMM:
                    byte = QUT_INSTRUCTION_VSP_SET_IMM_OP;
                    encoded.push_back(byte);
                    EncodeSLEB128_PUSHBACK(encoded, imm);
                    break;
                case QUT_INSTRUCTION_DEX_PC_SET:
                    byte = QUT_INSTRUCTION_DEX_PC_SET_OP;
                    encoded.push_back(byte);
                    if (log) {
                        QUT_DEBUG_LOG("QUT_INSTRUCTION_DEX_PC_SET_OP %x", (uint32_t) byte);
                    }
                    break;
                case QUT_END_OF_INS:
                    byte = QUT_END_OF_INS_OP;
                    encoded.push_back(byte);
                    break;
                case QUT_FIN:
                    byte = QUT_FINISH_OP;
                    encoded.push_back(byte);
                    break;
                case QUT_INSTRUCTION_R4_OFFSET:
                    if (imm >= 0 & imm <= IMM(4)) {
                        byte = FILL_WITH_IMM(QUT_INSTRUCTION_R4_OFFSET_OP_PREFIX, imm, 4);
                        encoded.push_back(byte);
                    } else {
                        if (log) {
                            QUT_DEBUG_LOG("QUT_INSTRUCTION_R4_OFFSET Overflow");
                        }
                        return false; // overflow, bad entry
                    }
                    break;
                case QUT_INSTRUCTION_R7_OFFSET:
                    if (imm >= 0 & imm <= IMM(4)) {
                        byte = FILL_WITH_IMM(QUT_INSTRUCTION_R7_OFFSET_OP_PREFIX, imm, 4);
                        encoded.push_back(byte);
                    } else {
                        byte = QUT_INSTRUCTION_R7_OFFSET_SLEB128_OP;
                        encoded.push_back(byte);
                        EncodeSLEB128_PUSHBACK(encoded, imm);
                    }
                    break;
                case QUT_INSTRUCTION_R10_OFFSET:
                    if (imm >= 0 & imm <= IMM(4)) {
                        byte = FILL_WITH_IMM(QUT_INSTRUCTION_R10_OFFSET_OP_PREFIX, imm, 4);
                        encoded.push_back(byte);
                    } else {
                        byte = QUT_INSTRUCTION_R10_OFFSET_SLEB128_OP;
                        encoded.push_back(byte);
                        EncodeSLEB128_PUSHBACK(encoded, imm);
                    }
                    break;
                case QUT_INSTRUCTION_R11_OFFSET:
                    if (imm >= 0 & imm <= IMM(4)) {
                        byte = FILL_WITH_IMM(QUT_INSTRUCTION_R11_OFFSET_OP_PREFIX, imm, 4);
                        encoded.push_back(byte);
                    } else {
                        byte = QUT_INSTRUCTION_R11_OFFSET_SLEB128_OP;
                        encoded.push_back(byte);
                        EncodeSLEB128_PUSHBACK(encoded, imm);
                    }
                    break;
                case QUT_INSTRUCTION_LR_OFFSET:
                    if (imm >= 0 & imm <= IMM(4)) {
                        byte = FILL_WITH_IMM(QUT_INSTRUCTION_LR_OFFSET_OP_PREFIX, imm, 4);
                        encoded.push_back(byte);
                    } else {
                        byte = QUT_INSTRUCTION_LR_OFFSET_SLEB128_OP;
                        encoded.push_back(byte);
                        EncodeSLEB128_PUSHBACK(encoded, imm);
                    }
                    break;
                case QUT_INSTRUCTION_SP_OFFSET: {
                    byte = QUT_INSTRUCTION_SP_OFFSET_SLEB128_OP;
                    encoded.push_back(byte);
                    EncodeSLEB128_PUSHBACK(encoded, imm);
                    break;
                }
                case QUT_INSTRUCTION_PC_OFFSET: {
                    byte = QUT_INSTRUCTION_PC_OFFSET_SLEB128_OP;
                    encoded.push_back(byte);
                    EncodeSLEB128_PUSHBACK(encoded, imm);
                    break;
                }
            }
        }

        return true;
    }

    inline bool
    _QuickenInstructionsEncode64(std::deque<uint64_t> &instructions, std::deque<uint8_t> &encoded,
                                 bool *prologue_conformed, bool log) {

// QUT encode for 64-bit:
//		00nn nnnn           : vsp = vsp + (nnnnnn << 2)             			; # (nnnnnnn << 2) in [0, 0xfc]
//      01nn nnnn           : vsp = vsp - (nnnnnn << 2)             			; # (nnnnnnn << 2) in [0, 0xfc]

//      1000 0000           : vsp = x29	            							; # x29 is fp reg
//      1000 0001           : vsp = x29 + 16, lr = [vsp - 8], sp = [vsp - 16]   ; # Have prologue
//      1000 0100           : vsp = sp                                    		;

//      1000 0101 0nnn nnnn : vsp = x29 + (nnnnnnn << 2)						; # (nnnnnnn << 2) in [0, 0x1fc],  0nnnnnnn is an one bit ULEB128

//		1001 0101 0nnn nnnn : vsp = x28 + (nnnnnnn << 2)						; # (nnnnnnn << 2) in [0, 0x1fc],  0nnnnnnn is an one bit ULEB128
//		1001 0110 + SLEB128 : vsp = SLEB128							    		; # vsp set by IMM

//		1001 0111 			: dex_pc = x20										; # Dex pc is saved in x29

//		1001 1001			: End of instructions                				;
//		1001 1111			: Finish                							;

//		1010 nnnn 			: x20 = [vsp - (nnnn << 2)]     					; # (nnnn << 2) in [0, 0x3c]
//		1100 nnnn           : x28 = [vsp - (nnnn << 2)]    						; # Same as above. x28 will be used while unwinding through JNI function
//      1101 nnnn           : x29 = [vsp - (nnnn << 2)]    						; # Same as above
//      1110 nnnn           : lr = [vsp - (nnnn << 2)]     						; # Same as above

//      1111 0xxx 			: Reserved											;
//      1111 1000			: Reserved											;

//      1111 1010 + SLEB128 : x28 = [vsp - SLEB128] 							; # [addr] means get value from pointed by addr
//      1111 1011 + SLEB128 : x29 = [vsp - SLEB128] 							; # Same as above
//      1111 1100 + SLEB128 : sp = [vsp - SLEB128]  							; # Same as above
//      1111 1101 + SLEB128 : lr = [vsp - SLEB128]  							; # Same as above
//      1111 1110 + SLEB128 : pc = [vsp - SLEB128]  							; # Same as above

//      1111 1111 + SLEB128 : vsp = vsp + SLEB128   							;

        (void) prologue_conformed;

        while (!instructions.empty()) {

            uint64_t instruction = instructions.front();

            instructions.pop_front();

#ifdef QUT_STATISTIC_ENABLE
            CheckInstruction(instruction);
#endif

            uint32_t op = (uint32_t) (instruction >> 32);
            int32_t imm = (int32_t) (instruction & 0xFFFFFFFF);

            if (log) {
                QUT_DEBUG_LOG("Encoding instruction: %llx", (ullint_t) instruction);
                QUT_DEBUG_LOG("Encoding op: %llx, imm: %lld", (ullint_t) op, (llint_t) imm);
            }

            if (imm & 0x3) {
                QUT_STATISTIC(InstructionOpImmNotAligned, op, imm);
                if (log) {
                    QUT_DEBUG_LOG("InstructionOpImmNotAligned");
                }
                return false; // bad entry
            }

            uint8_t byte;
            switch (op) {
                case QUT_INSTRUCTION_VSP_OFFSET:
                    // imm ~ [-((1 << 6) - 1) << 2, ((1 << 6) - 1) << 2]
                    if (imm >= -IMM(6) & imm <= IMM(6)) {
                        // 00nn nnnn : vsp = vsp + (nnnnnn << 2)
                        // 01nn nnnn : vsp = vsp - (nnnnnn << 2)
                        byte = FILL_WITH_IMM(imm > 0 ? 0 : 0x40, imm, 6);
                        encoded.push_back(byte);
                    } else {
                        // 1111 1111 + SLEB128 : vsp = vsp + SLEB128
                        byte = QUT_INSTRUCTION_VSP_OFFSET_SLEB128_OP;
                        encoded.push_back(byte);
                        EncodeSLEB128_PUSHBACK(encoded, imm);
                    }
                    break;
                case QUT_INSTRUCTION_VSP_SET_BY_X29: {

                    if (imm != 0) {
                        uint8_t byte;
                        byte = QUT_INSTRUCTION_VSP_SET_BY_X29_IMM_OP;
                        encoded.push_back(byte);
                        if (imm > 0 && imm <= IMM(7)) {
                            byte = FILL_WITH_IMM(0, imm, 7);
                            encoded.push_back(byte);
                        } else {
                            if (log) {
                                QUT_DEBUG_LOG("QUT_INSTRUCTION_VSP_SET_BY_X29 overflow");
                            }
                            return false; // overflow, bad entry
                        }
                        break;
                    } else {
                        bool have_prologue = false;

                        do {
                            if (instructions.size() >= 3) {
                                uint64_t next = instructions.at(0);
                                uint32_t next_op = (uint32_t) (next >> 32);
                                int32_t next_imm = (int32_t) (next & 0xFFFFFFFF);
                                if (!(next_op == QUT_INSTRUCTION_VSP_OFFSET)) {
                                    break;
                                }

                                next = instructions.at(1);
                                next_op = (uint32_t) (next >> 32);
                                int32_t next_imm_lr = (int32_t) (next & 0xFFFFFFFF);
                                if (!(next_op == QUT_INSTRUCTION_LR_OFFSET &&
                                      next_imm_lr == (next_imm - 8))) {
                                    break;
                                }

                                next = instructions.at(2);
                                next_op = (uint32_t) (next >> 32);
                                int32_t next_imm_fp = (int32_t) (next & 0xFFFFFFFF);

                                if ((next_op == QUT_INSTRUCTION_X29_OFFSET)
                                    && (next_imm_fp == next_imm)) {
                                    have_prologue = true;
                                    instructions.pop_front(); // vsp + 16       // TODO recheck this logic
                                    instructions.pop_front(); // pop lr = vsp - 8
                                    instructions.pop_front(); // pop x29 = vsp - 16
                                }
                            }
                        } while (false);
                        // 1000 0000 : vsp = x29                                         ;
                        // 1000 0001 : vsp = x29 + 16, lr = [vsp - 8], sp = [vsp - 16]   ; # Have prologue
                        uint8_t byte;
                        if (have_prologue) {
                            byte = QUT_INSTRUCTION_VSP_SET_BY_X29_PROLOGUE_OP;
                        } else {
                            byte = QUT_INSTRUCTION_VSP_SET_BY_X29_OP;
                        }

                        encoded.push_back(byte);

                        break;
                    }
                }
                case QUT_INSTRUCTION_VSP_SET_BY_SP:
                    // 1000 0100 : vsp = sp
                    byte = QUT_INSTRUCTION_VSP_SET_BY_SP_OP;
                    encoded.push_back(byte);
                    break;
                case QUT_INSTRUCTION_VSP_SET_BY_JNI_SP:
                    byte = QUT_INSTRUCTION_VSP_SET_BY_JNI_SP_OP;
                    encoded.push_back(byte);
                    if (imm <= IMM(7)) {
                        byte = FILL_WITH_IMM(0, imm, 7);
                    } else {
                        if (log) {
                            QUT_DEBUG_LOG("QUT_INSTRUCTION_VSP_SET_BY_JNI_SP overflow");
                        }
                        return false; // overflow bad entry
                    }
                    encoded.push_back(byte);
                    if (log) {
                        QUT_DEBUG_LOG("QUT_INSTRUCTION_VSP_SET_BY_JNI_SP %x %x %x",
                                      QUT_INSTRUCTION_VSP_SET_BY_JNI_SP_OP, (uint32_t) byte,
                                      (uint32_t) imm);
                    }
                    break;
                case QUT_INSTRUCTION_VSP_SET_IMM:
                    byte = QUT_INSTRUCTION_VSP_SET_IMM_OP;
                    encoded.push_back(byte);
                    EncodeSLEB128_PUSHBACK(encoded, imm);
                    break;
                case QUT_INSTRUCTION_DEX_PC_SET:
                    byte = QUT_INSTRUCTION_DEX_PC_SET_OP;
                    encoded.push_back(byte);
                    if (log) {
                        QUT_DEBUG_LOG("QUT_INSTRUCTION_DEX_PC_SET_OP %x", (uint32_t) byte);
                    }
                    break;
                case QUT_END_OF_INS:
                    byte = QUT_END_OF_INS_OP;
                    encoded.push_back(byte);
                    break;
                case QUT_FIN:
                    byte = QUT_FINISH_OP;
                    encoded.push_back(byte);
                    break;
                case QUT_INSTRUCTION_X20_OFFSET:
                    if (imm >= 0 & imm <= IMM(4)) {
                        byte = FILL_WITH_IMM(QUT_INSTRUCTION_X20_OFFSET_OP_PREFIX, imm, 4);
                        encoded.push_back(byte);
                    } else {
                        byte = QUT_INSTRUCTION_X20_OFFSET_SLEB128_OP;
                        encoded.push_back(byte);
                        EncodeSLEB128_PUSHBACK(encoded, imm);
                    }
                    break;
                case QUT_INSTRUCTION_X29_OFFSET:
                    if (imm >= 0 & imm <= IMM(4)) {
                        byte = FILL_WITH_IMM(QUT_INSTRUCTION_X29_OFFSET_OP_PREFIX, imm, 4);
                        encoded.push_back(byte);
                    } else {
                        byte = QUT_INSTRUCTION_X29_OFFSET_SLEB128_OP;
                        encoded.push_back(byte);
                        EncodeSLEB128_PUSHBACK(encoded, imm);
                    }
                    break;
                case QUT_INSTRUCTION_X28_OFFSET:
                    if (imm >= 0 & imm <= IMM(4)) {
                        byte = FILL_WITH_IMM(QUT_INSTRUCTION_X28_OFFSET_OP_PREFIX, imm, 4);
                        encoded.push_back(byte);
                    } else {
                        byte = QUT_INSTRUCTION_X28_OFFSET_SLEB128_OP;
                        encoded.push_back(byte);
                        EncodeSLEB128_PUSHBACK(encoded, imm);
                    }
                    break;
                case QUT_INSTRUCTION_LR_OFFSET:
                    if (imm >= 0 & imm <= IMM(4)) {
                        byte = FILL_WITH_IMM(QUT_INSTRUCTION_LR_OFFSET_OP_PREFIX, imm, 4);
                        encoded.push_back(byte);
                    } else {
                        byte = QUT_INSTRUCTION_LR_OFFSET_SLEB128_OP;
                        encoded.push_back(byte);
                        EncodeSLEB128_PUSHBACK(encoded, imm);
                    }
                    break;
                case QUT_INSTRUCTION_SP_OFFSET: {
                    byte = QUT_INSTRUCTION_SP_OFFSET_SLEB128_OP;
                    encoded.push_back(byte);
                    EncodeSLEB128_PUSHBACK(encoded, imm);
                    break;
                }
                case QUT_INSTRUCTION_PC_OFFSET: {
                    byte = QUT_INSTRUCTION_PC_OFFSET_SLEB128_OP;
                    encoded.push_back(byte);
                    EncodeSLEB128_PUSHBACK(encoded, imm);
                    break;
                }
            }
        }

        if (log) {
            QUT_DEBUG_LOG("Encoding done.");
        }
        return true;
    }

    inline bool
    QuickenInstructionsEncode(std::deque<uint64_t> &instructions, std::deque<uint8_t> &encoded,
                              bool *prologue_conformed, bool log = false) {
#ifdef __arm__
        return _QuickenInstructionsEncode32(instructions, encoded, prologue_conformed, log);
#else
        return _QuickenInstructionsEncode64(instructions, encoded, prologue_conformed, log);
#endif
    }

    QUT_EXTERN_C_BLOCK_END
}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_INSTRUCTIONS_H
