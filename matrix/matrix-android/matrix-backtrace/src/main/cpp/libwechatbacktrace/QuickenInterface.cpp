/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

    uint64_t QuickenInterface::GetLoadBias() const {
        return load_bias_;
    }

    uint64_t QuickenInterface::GetElfOffset() const {
        return elf_offset_;
    }

    uint64_t QuickenInterface::GetElfStartOffset() const {
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

    QutFileError QuickenInterface::TryInitQuickenTable() {

        QutFileError ret;

        {
            lock_guard<mutex> lock(lock_);

            if (qut_sections_) {
                return NoneError;
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
                    return NoneError;
                } else {
                    ret = LoadFailed;
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
        }

        return ret;
    }

    bool QuickenInterface::FindEntry(QutSections *qut_sections, uptr pc, size_t *entry_offset) {
        size_t first = 0;
        size_t last = qut_sections->idx_size;
        while (first < last) {
            size_t current = ((first + last) / 2) & 0xfffffffe;
            uptr addr = qut_sections->quidx[current];
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

    inline bool
    QuickenInterface::StepInternal(StepContext *step_context, QutSections *sections) {

        QuickenTable quicken(sections, step_context);

        size_t entry_offset;

        if (UNLIKELY(!FindEntry(sections, step_context->pc, &entry_offset))) {
            return false;
        }

        uptr *regs = step_context->regs;

        quicken.cfa_ = SP(regs);
        bool return_value = false;
//        quicken.log = log && log_pc == pc;
        last_error_code_ = quicken.Eval(entry_offset);
        if (LIKELY(last_error_code_ == QUT_ERROR_NONE)) {
            if (!quicken.pc_set_) {
                PC(regs) = LR(regs);
            }
            SP(regs) = quicken.cfa_;
            return_value = true;
            step_context->dex_pc = quicken.dex_pc_;
        }

        // If the pc was set to zero, consider this the final frame.
        step_context->finished = (PC(regs) == 0);
        return return_value;
    }

    bool
    QuickenInterface::StepJIT(StepContext *step_context, wechat_backtrace::Maps *maps) {

        std::shared_ptr<QutSectionsInMemory> qut_sections_for_jit;
        if (LIKELY(debug_jit_)) {
            bool ret = debug_jit_->GetFutSectionsInMemory(
                    maps, step_context->pc,
                    qut_sections_for_jit);
            if (!ret || qut_sections_for_jit == nullptr) {
                last_error_code_ = QUT_ERROR_REQUEST_QUT_INMEM_FAILED;
                return false;
            }
        } else {
            last_error_code_ = QUT_ERROR_REQUEST_QUT_INMEM_FAILED;
            return false;
        }

        return StepInternal(step_context, qut_sections_for_jit.get());
    }

    bool
    QuickenInterface::Step(StepContext *step_context) {

        if (UNLIKELY(step_context->pc < load_bias_)) {
            last_error_code_ = QUT_ERROR_UNWIND_INFO;
            return false;
        }
        if (UNLIKELY(!qut_sections_)) {
            std::shared_ptr<QuickenInMemory<addr_t>> quicken_in_memory;
            {
                lock_guard<mutex> lock(lock_quicken_in_memory_);
                quicken_in_memory = quicken_in_memory_;
            }
            if (quicken_in_memory) {
                QUT_LOG("QuickenInterface::Step using quicken_in_memory_, elf: %s, frame: %llu",
                        soname_.c_str(), (ullint_t) frame_size);
                std::shared_ptr<QutSectionsInMemory> qut_section_in_memory;
                bool ret = quicken_in_memory->GetFutSectionsInMemory(
                        step_context->pc, /* out */ qut_section_in_memory);
                if (UNLIKELY(!ret)) {
                    QUT_LOG("QuickenInterface::Step quicken_in_memory_ failed");
                    last_error_code_ = QUT_ERROR_REQUEST_QUT_FILE_FAILED;
                    return false;
                }
                return StepInternal(step_context, qut_section_in_memory.get());
            } else {
                last_error_code_ = QUT_ERROR_REQUEST_QUT_FILE_FAILED;
                return false;
            }
        }
        return StepInternal(step_context, const_cast<QutSections *>(qut_sections_));
    }

    void QuickenInterface::ResetQuickenInMemory() {
        {
            lock_guard<mutex> lock(lock_quicken_in_memory_);
            quicken_in_memory_ = nullptr;
        }
    }

    template bool
    QuickenInterface::GenerateQuickenTable<addr_t>(
            unwindstack::Memory *memory,
            unwindstack::Memory *gnu_debug_data_memory,
            unwindstack::Memory *process_memory,
            QutSectionsPtr qut_sections);

}  // namespace wechat_backtrace