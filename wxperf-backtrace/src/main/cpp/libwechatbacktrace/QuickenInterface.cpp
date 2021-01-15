//
// Created by Carl on 2020-09-21.
//

#include <android-base/macros.h>
#include <MinimalRegs.h>
#include <QuickenTableManager.h>
#include <deps/android-base/include/android-base/logging.h>
#include <QuickenJNI.h>
#include "Log.h"
#include "QuickenInterface.h"
#include "QuickenMaps.h"
#include "DwarfEhFrameWithHdrDecoder.h"
#include "DwarfEhFrameDecoder.h"
#include "DwarfDebugFrameDecoder.h"
#include "PthreadExt.h"

namespace wechat_backtrace {

    using namespace std;
    using namespace unwindstack;

    quicken_generate_delegate_func QuickenInterface::quicken_generate_delegate_ = nullptr;

    void QuickenInterface::SetQuickenGenerateDelegate(
            quicken_generate_delegate_func quicken_generate_delegate) {
        quicken_generate_delegate_ = quicken_generate_delegate;
    }

    uint64_t QuickenInterface::GetLoadBias() {
        return load_bias_;
    }

    uint64_t QuickenInterface::GetElfOffset() {
        return elf_offset_;
    }

    uint64_t QuickenInterface::GetElfStartOffset() {
        return elf_start_offset_;
    }

    template<typename AddressType>
    bool QuickenInterface::GenerateQuickenTable(
            unwindstack::Memory *memory,
            unwindstack::Memory *gnu_debug_data_memory,
            unwindstack::Memory *process_memory,
            QutSectionsPtr qut_sections) {

        QUT_DEBUG_LOG("QuickenInterface::GenerateQuickenTableUltra");

        CHECK(memory != nullptr);
        CHECK(process_memory != nullptr);

        QuickenTableGenerator<AddressType> generator(memory, gnu_debug_data_memory, process_memory);

        bool ret = generator.GenerateUltraQUTSections(
                eh_frame_hdr_info_, eh_frame_info_,
                debug_frame_info_, gnu_eh_frame_hdr_info_,
                gnu_eh_frame_info_, gnu_debug_frame_info_,
                arm_exidx_info_, qut_sections);

        return ret;
    }

    bool QuickenInterface::TryInitQuickenTable() {

        QutFileError ret;

        {
            lock_guard<mutex> lock(lock_);

            if (qut_sections_) {
                return true;
            }

            QutSectionsPtr qut_sections_tmp = nullptr;
            ret = QuickenTableManager::getInstance().RequestQutSections(
                    soname_, sopath_, hash_, build_id_, elf_start_offset_, qut_sections_tmp);

            QUT_DEBUG_LOG("Try init quicken table soname %s path %s build_id_ %s result %d",
                          soname_.c_str(), sopath_.c_str(),
                          build_id_.c_str(), ret);
            if (ret == NoneError) {
                qut_sections_ = qut_sections_tmp;

                QUT_DEBUG_LOG("Request %s build id(%s) quicken table success.",
                              sopath_.c_str(), build_id_.c_str());

                if (qut_sections_) {
                    return true;
                }
            }
        }

        if (UNLIKELY(quicken_generate_delegate_)) {
            if (try_load_qut_failed_count_ < 3 &&
                (ret == TryInvokeJavaRequestQutGenerate || ret == NotWarmedUp ||
                 ret == LoadRequesting)) {
                QUT_DEBUG_LOG("Invoke generation immediately for so: %s, elf start offset: %llu",
                              sopath_.c_str(), (ullint_t) elf_start_offset_);
                if (!quicken_generate_delegate_(sopath_, elf_start_offset_, false)) {
                    try_load_qut_failed_count_++;
                } else {
                    try_load_qut_failed_count_ = 0;
                }
            }
        } else {
            if (ret == TryInvokeJavaRequestQutGenerate) {
                InvokeJava_RequestQutGenerate();
            }
        }


        return false;
    }

    bool QuickenInterface::FindEntry(uptr pc, size_t *entry_offset) {
        size_t first = 0;
        size_t last = qut_sections_->idx_size;
        while (first < last) {
            size_t current = ((first + last) / 2) & 0xfffffffe;
            uptr addr = qut_sections_->quidx[current];
            if (log && pc == log_pc) {
                QUT_LOG(">>> QuickenInterface::FindEntry current:%llu addr:%llx pc:%llx",
                        (ullint_t) current, (ullint_t) addr, (ullint_t) pc);
            }
            if (pc == addr) {
                *entry_offset = current;
                if (log && pc == log_pc) {
                    QUT_LOG(">>> QuickenInterface::FindEntry found entry_offset:%llu pc:%llx",
                            (ullint_t) *entry_offset, (ullint_t) pc);
                }
                return true;
            }
            if (pc < addr) {
                last = current;
            } else {
                first = current + 2;
            }
        }
        if (last != 0) {
            if (log && pc == log_pc) {
                QUT_LOG(">>> QuickenInterface::FindEntry found entry_offset:%llu pc:%llx",
                        (ullint_t) *entry_offset, (ullint_t) pc);
            }
            *entry_offset = last - 2;
            return true;
        }
        last_error_code_ = QUT_ERROR_UNWIND_INFO;
        return false;
    }

    bool
    QuickenInterface::Step(uptr pc, uptr *regs, unwindstack::Memory *process_memory, uptr stack_top,
                           uptr stack_bottom, uptr frame_size, uint64_t *dex_pc, bool *finished) {

        if (UNLIKELY(pc < load_bias_)) {
            last_error_code_ = QUT_ERROR_UNWIND_INFO;
            return false;
        }

//        if (HasSuffix(sopath_, ".odex")) {
//            QUT_DEBUG_LOG("Step in %s", sopath_.c_str());
//        }

        if (!qut_sections_) {
            if (!TryInitQuickenTable()) {
                last_error_code_ = QUT_ERROR_REQUEST_QUT_FILE_FAILED;
                return false;
            }
        }
        QuickenTable quicken(const_cast<QutSections *>(qut_sections_), regs,
                             process_memory, stack_top, stack_bottom, frame_size);
        size_t entry_offset;

        // Adjust the load bias to get the real relative pc.
        pc -= load_bias_;

        if (UNLIKELY(!FindEntry(pc, &entry_offset))) {
            return false;
        }

        quicken.cfa_ = SP(regs);
        bool return_value = false;
        last_error_code_ = quicken.Eval(entry_offset);
        if (last_error_code_ == QUT_ERROR_NONE) {
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
    QuickenInterface::GenerateQuickenTable<addr_t>(
            unwindstack::Memory *memory,
            unwindstack::Memory *gnu_debug_data_memory,
            unwindstack::Memory *process_memory,
            QutSectionsPtr qut_sections);

}  // namespace unwindstack