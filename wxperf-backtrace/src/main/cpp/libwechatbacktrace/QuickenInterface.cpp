//
// Created by Carl on 2020-09-21.
//

#include <android-base/macros.h>
#include <MinimalRegs.h>
#include <QuickenTableManager.h>
#include "../../common/Log.h"
#include "QuickenInterface.h"
#include "QuickenMaps.h"
#include "DwarfEhFrameWithHdrDecoder.h"
#include "DwarfEhFrameDecoder.h"
#include "DwarfDebugFrameDecoder.h"
#include "../../common/PthreadExt.h"

namespace wechat_backtrace {

    using namespace std;
    using namespace unwindstack;

    uint64_t QuickenInterface::GetLoadBias() {
        return load_bias_;
    }

    template<typename AddressType>
    bool QuickenInterface::GenerateQuickenTable(unwindstack::Memory *process_memory,
                                                QutSectionsPtr qut_sections) {

        QUT_DEBUG_LOG("QuickenInterface::GenerateQuickenTableUltra");

        QuickenTableGenerator<AddressType> generator(memory_, process_memory);

        bool ret = generator.GenerateUltraQUTSections(eh_frame_hdr_info_, eh_frame_info_,
                                                      debug_frame_info_, gnu_eh_frame_hdr_info_,
                                                      gnu_eh_frame_info_, gnu_debug_frame_info_,
                                                      arm_exidx_info_, qut_sections);
        return ret;
    }

    bool QuickenInterface::TryInitQuickenTable() {

        lock_guard<mutex> lock(lock_);

        if (qut_sections_) {
            return true;
        }

        QutSectionsPtr qut_sections_tmp = nullptr;
        QutFileError error = QuickenTableManager::getInstance().RequestQutSections(
                soname_, sopath_, hash_, build_id_, build_id_hex_, qut_sections_tmp);

        QUT_LOG("Try init quicken table %s result %d", sopath_.c_str(), error);
        if (error != NoneError) {
            return false;
        }

        qut_sections_ = qut_sections_tmp;

        QUT_DEBUG_LOG("QuickenInterface::TryInitQuickenTable");

        if (qut_sections_) {
            return true;
        }

        return false;
    }

    bool QuickenInterface::FindEntry(uptr pc, size_t *entry_offset) {
        size_t first = 0;
        size_t last = qut_sections_->idx_size;
        QUT_DEBUG_LOG("QuickenInterface::FindEntry first:%u last:%u pc:%x", first, last, pc);
        while (first < last) {
            size_t current = ((first + last) / 2) & 0xfffffffe;
            uptr addr = qut_sections_->quidx[current];
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
            QUT_DEBUG_LOG("QuickenInterface::FindEntry found entry_offset: %u, addr: %x", last - 2,
                          qut_sections_->quidx[last - 2]);
            *entry_offset = last - 2;
            return true;
        }
        last_error_code_ = QUT_ERROR_UNWIND_INFO;
        return false;
    }

    bool
    QuickenInterface::Step(uptr pc, uptr *regs, unwindstack::Memory *process_memory, uptr stack_top,
                           uptr stack_bottom, uptr frame_size, uint64_t *dex_pc, bool *finished) {

        // Adjust the load bias to get the real relative pc.
        if (UNLIKELY(pc < load_bias_)) {
            last_error_code_ = QUT_ERROR_UNWIND_INFO;
            return false;
        }

        if (!qut_sections_) {
            if (!TryInitQuickenTable()) {
                last_error_code_ = QUT_ERROR_REQUEST_QUT_FILE_FAILED;
                return false;
            }
        }

        QuickenTable quicken(qut_sections_, regs, memory_, process_memory, stack_top, stack_bottom, frame_size);
        size_t entry_offset;

        QUT_DEBUG_LOG("QuickenInterface::Step pc:%llx, load_bias_:%llu", (uint64_t) pc, load_bias_);

        pc -= load_bias_;

        if (UNLIKELY(!FindEntry(pc, &entry_offset))) {
            return false;
        }

        quicken.cfa_ = SP(regs);
        bool return_value = false;
        last_error_code_ = quicken.Eval(entry_offset);
        if (last_error_code_ == QUT_ERROR_NONE) {
            QUT_DEBUG_LOG(
                    "QuickenInterface::Step quicken.Eval PC(regs) %llx LR(regs) %llx ken.pc_set_ %d",
                    (uint64_t) PC(regs), (uint64_t) LR(regs), quicken.pc_set_);
            if (!quicken.pc_set_) {
                PC(regs) = LR(regs);
            }
            SP(regs) = quicken.cfa_;
            return_value = true;

            if (quicken.dex_pc_ != 0) {
                *dex_pc = quicken.dex_pc_;
            }
        }

        // If the pc was set to zero, consider this the final frame.
        *finished = (PC(regs) == 0) ? true : false;

        QUT_DEBUG_LOG("QuickenInterface::Step finished: %d, PC(regs) %llx", *finished,
                      (uint64_t) PC(regs));

        return return_value;
    }

//    bool QuickenInterface::StepBack(uptr pc, uptr sp, uptr fp, uint8_t &regs_bits, uptr *regs,
//                                    unwindstack::Memory *process_memory, bool *finish) {
//
//        // Adjust the load bias to get the real relative pc.
//        if (UNLIKELY(pc < load_bias_)) {
//            last_error_code_ = QUT_ERROR_UNWIND_INFO;
//            return false;
//        }
//
//        if (!qut_sections_) {
//            if (!GenerateQuickenTable<addr_t>(process_memory)) {
//                last_error_code_ = QUT_ERROR_QUT_SECTION_INVALID;
//                return false;
//            }
//        }
//
//        QuickenTableReverse quicken(qut_sections_, regs, memory_, process_memory);
//        size_t entry_offset;
//
//        pc -= load_bias_;
//
//        if (UNLIKELY(!FindEntry(pc, &entry_offset))) {
//            return false;
//        }
//
//        quicken.cfa_ = sp - sizeof(uptr) * 2;   // TODO check this
//        quicken.cfa_next_ = fp - sizeof(uptr) * 2;
//        quicken.regs_bits_ = regs_bits;
//        bool return_value = false;
//
//        if (quicken.EvalReverse(entry_offset)) {
//            return_value = true;
//            regs_bits = quicken.regs_bits_ | quicken.regs_bits_tmp_;
//            QUT_TMP_LOG("regs_bits "
//                                BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(regs_bits));
//            QUT_TMP_LOG("quicken.regs_bits_ "
//                                BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(quicken.regs_bits_));
//            QUT_TMP_LOG("quicken.regs_bits_tmp_ "
//                                BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(quicken.regs_bits_tmp_));
//            if (regs_bits == OP(0100, 1111)) {
//                *finish = true;
//            }
//        } else {
//            last_error_code_ = QUT_ERROR_INVALID_QUT_INSTR;
//        }
//
//        return return_value;
//    }

    template bool
    QuickenInterface::GenerateQuickenTable<addr_t>(unwindstack::Memory *process_memory,
                                                   QutSectionsPtr qut_sections);

}  // namespace unwindstack