//
// Created by Carl on 2020-09-21.
//

#ifndef _LIBWECHATBACKTRACE_EXIDX_DECODER_H
#define _LIBWECHATBACKTRACE_EXIDX_DECODER_H

#include <deque>

#include "Errors.h"
#include "MinimalRegs.h"
#include "QuickenInstructions.h"

namespace wechat_backtrace {

    enum ArmStatus : size_t {
        ARM_STATUS_NONE = 0,
        ARM_STATUS_NO_UNWIND,
        ARM_STATUS_FINISH,
        ARM_STATUS_RESERVED,
        ARM_STATUS_SPARE,
        ARM_STATUS_TRUNCATED,
        ARM_STATUS_READ_FAILED,
        ARM_STATUS_MALFORMED,
        ARM_STATUS_INVALID_ALIGNMENT,
        ARM_STATUS_INVALID_PERSONALITY,
    };

    enum ArmOp : uint8_t {
        ARM_OP_FINISH = 0xb0,
    };

    struct ExidxContext {

    public:
        int32_t vsp_ = 0;
        int32_t transformed_bits = 0;
        int32_t regs_[QUT_MINIMAL_REG_SIZE] = {0};

        const bool log = false;

        void reset() {
            if (log) {
                QUT_DEBUG_LOG("reset");
            }
            vsp_ = 0;
            transformed_bits = 0;
            memset(regs_, 0, sizeof(int32_t) * QUT_MINIMAL_REG_SIZE);
        }

        void AddUpTransformed(uint32_t reg_idx, int32_t imm) {
            if (transformed_bits & (1 << reg_idx)) {
                regs_[reg_idx] += imm;
            }
        }

        void AddUpVSP(int32_t imm) {

            if (log) {
                QUT_DEBUG_LOG("AddUpVSP imm: %d", imm);
            }

            vsp_ += imm;

            AddUpTransformed(QUT_INSTRUCTION_R4_OFFSET, imm);
            AddUpTransformed(QUT_INSTRUCTION_R7_OFFSET, imm);
            AddUpTransformed(QUT_INSTRUCTION_R10_OFFSET, imm);
            AddUpTransformed(QUT_INSTRUCTION_R11_OFFSET, imm);
            AddUpTransformed(QUT_INSTRUCTION_SP_OFFSET, imm);
            AddUpTransformed(QUT_INSTRUCTION_LR_OFFSET, imm);
            AddUpTransformed(QUT_INSTRUCTION_PC_OFFSET, imm);
        }

        void Transform(uint32_t reg_idx) {
            if (log) {
                QUT_DEBUG_LOG("Transform reg_idx: %d", reg_idx);
            }
            transformed_bits = transformed_bits | (1 << reg_idx);
            regs_[reg_idx] = 0;
        }

    };

    class ExidxDecoder {

    public:
        ExidxDecoder(unwindstack::Memory *memory, unwindstack::Memory *process_memory)
                : memory_(memory), process_memory_(process_memory) {
            instructions_.reset(new QutInstrCollection);
        }

        ~ExidxDecoder() {};

        bool ExtractEntryData(uint32_t entry_offset);

        bool Eval();

        std::unique_ptr<QutInstrCollection> instructions_;

        ArmStatus status_ = ARM_STATUS_NONE;
        uint64_t status_address_ = 0;

        const bool log = false;

    protected:
        unwindstack::Memory *memory_;
        unwindstack::Memory *process_memory_;

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

        bool GetByte(uint8_t *byte);

        void FlushInstructions();

        void SaveInstructions(QutInstruction instruction);

        std::deque<uint8_t> data_;

        // context
        ExidxContext context_;

    };

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_EXIDX_DECODER_H
