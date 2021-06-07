/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
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
#include <android-base/logging.h>

#include <memory>
#include <ElfWrapper.h>
#include "Log.h"
#include "DwarfEhFrameWithHdrDecoder.h"
#include "DwarfDebugFrameDecoder.h"
#include "DwarfEhFrameDecoder.h"
#include "ExidxDecoder.h"
#include "QuickenInMemory.h"


namespace wechat_backtrace {

    using namespace std;
    using namespace unwindstack;

    void ExidxDecoderHelper::Init(FrameInfo arm_exidx_info) {
        start_offset_ = arm_exidx_info.offset_;
        total_entries_ = arm_exidx_info.size_;
    }

    inline bool ExidxDecoderHelper::FindEntry(uint32_t pc,
            /* out */ uint32_t *entry_offset,
            /* out */ uint32_t *start_addr,
            /* out */ uint32_t *end_addr) {
        if (start_offset_ == 0 || total_entries_ == 0) {
            return false;
        }

        size_t first = 0;
        size_t last = total_entries_;
        uint32_t addr;
        while (first < last) {
            size_t current = (first + last) / 2;
            if (GetAddr(current, &addr)) {
                return false;
            }
            if (pc == addr) {
                *entry_offset = start_offset_ + current * 8;
                *start_addr = addr;
                if (last >= total_entries_ || !GetAddr(current + 1, end_addr)) {
                    *end_addr = INT32_MAX;
                }
                return true;
            }
            if (pc < addr) {
                last = current;
            } else {
                first = current + 1;
            }
        }
        if (last != 0) {
            *entry_offset = start_offset_ + (last - 1) * 8;
            *start_addr = addr;
            if (last >= total_entries_ || !GetAddr(last, end_addr)) {
                *end_addr = INT32_MAX;
            }
            return true;
        }
        return false;
    }

    inline bool ExidxDecoderHelper::GetAddr(size_t idx, /* out */ uint32_t *addr) {
        *addr = addrs_[idx];
        if (*addr == 0) {
            if (!GetPrel31Addr(start_offset_ + idx * 8, addr)) {
                *addr = 0;
                return false;
            }
            addrs_[idx] = *addr;
        }
        return true;
    }

    inline bool ExidxDecoderHelper::GetPrel31Addr(uint32_t offset, uint32_t *addr) {
        uint32_t data;
        if (!memory_->Read32(offset, &data)) {
            return false;
        }

        // Sign extend the value if necessary.
        int32_t value = static_cast<int32_t>(data) << 1 >> 1;
        *addr = offset + value;
        return true;
    }

    bool ExidxDecoderHelper::DecodeEntry(uint32_t pc,
                                         const shared_ptr<QutInstructionsOfEntries> &instructions,
                                         uint64_t *pc_start, uint64_t *pc_end) {

        uint32_t entry_offset;
        uint32_t start_addr;
        uint32_t end_addr;

        FindEntry(pc, &entry_offset, &start_addr, &end_addr);

        *pc_start = start_addr;
        *pc_end = end_addr;

        ExidxDecoder decoder(memory_, process_memory_);

        // Extract data, evaluate instructions and re-encode it.
        if (decoder.ExtractEntryData(entry_offset) && decoder.Eval()) {
            (*instructions)[start_addr] = std::make_pair(start_addr, move(decoder.instructions_));
            return true;
        }
        return false;
    }

    // --------------------------

    template<typename AddressType>
    void InitDebugFrame(
            Memory *memory,
            FrameInfo debug_frame_info,
            std::unique_ptr<DwarfSectionDecoder<AddressType>> &debug_frame_) {
        {   // debug_frame
            QUT_LOG("QuickenInMemory::InitDebugFrame memory %llx, debug_frame_info.offset_ %llx",
                    (ullint_t) memory, (ullint_t) debug_frame_info.offset_);
            if (memory != nullptr && debug_frame_info.offset_ != 0) {
                auto debug_frame = make_unique<DwarfDebugFrameDecoder<AddressType>>(memory);
                if (debug_frame->Init(debug_frame_info.offset_, debug_frame_info.size_,
                                      debug_frame_info.section_bias_)) {
                    debug_frame_ = std::move(debug_frame);
                }
            }
            QUT_LOG("QuickenInMemory::InitDebugFrame memory %llx, debug_frame_info.offset_ %llx, "
                    "result: %llx",
                    (ullint_t) memory, (ullint_t) debug_frame_info.offset_,
                    (ullint_t) debug_frame_.get());
        }
    }

    template<typename AddressType>
    void InitEhFrame(
            Memory *memory,
            FrameInfo eh_frame_hdr_info,
            FrameInfo eh_frame_info,
            std::unique_ptr<DwarfSectionDecoder<AddressType>> &eh_frame_) {
        {   // eh_frame & eh_frame_hdr
            QUT_LOG("QuickenInMemory::InitEhFrame memory %llx, eh_frame_hdr_info.size_ %llx "
                    "eh_frame_info.size_ %llx",
                    (ullint_t) memory, (ullint_t) eh_frame_hdr_info.size_,
                    (ullint_t) eh_frame_info.size_);
            if (memory != nullptr && eh_frame_hdr_info.offset_ != 0) {

                auto *eh_frame_hdr = new DwarfEhFrameWithHdrDecoder<AddressType>(
                        memory);
                eh_frame_.reset(eh_frame_hdr);
                if (!eh_frame_hdr->EhFrameInit(eh_frame_info.offset_, eh_frame_info.size_,
                                               eh_frame_info.section_bias_) ||
                    !eh_frame_hdr->Init(eh_frame_hdr_info.offset_, eh_frame_hdr_info.size_,
                                        eh_frame_hdr_info.section_bias_)) {
                    eh_frame_ = nullptr;
                }
            }

            if (memory != nullptr && eh_frame_ == nullptr && eh_frame_info.offset_ != 0) {
                // If there is an eh_frame section without an eh_frame_hdr section,
                // or using the frame hdr object failed to init.
                eh_frame_.reset(new DwarfEhFrameDecoder<AddressType>(memory));
                if (!eh_frame_->Init(eh_frame_info.offset_, eh_frame_info.size_,
                                     eh_frame_info.section_bias_)) {
                    eh_frame_ = nullptr;
                }
            }

            QUT_LOG("QuickenInMemory::InitEhFrame memory %llx, eh_frame_hdr_info.size_ %llx "
                    "eh_frame_info.size_ %llx, result %llx",
                    (ullint_t) memory, (ullint_t) eh_frame_hdr_info.size_,
                    (ullint_t) eh_frame_info.size_, (ullint_t) eh_frame_.get());
        }
    }

    template<typename AddressType>
    void
    QuickenInMemory<AddressType>::Init(ElfWrapper *elf_wrapper,
                                       const std::shared_ptr<unwindstack::Memory> &process_memory,
                                       FrameInfo &eh_frame_hdr_info,
                                       FrameInfo &eh_frame_info,
                                       FrameInfo &debug_frame_info,
                                       FrameInfo &gnu_eh_frame_hdr_info,
                                       FrameInfo &gnu_eh_frame_info,
                                       FrameInfo &gnu_debug_frame_info,
                                       FrameInfo &arm_exidx_info) {

        CHECK(process_memory);
        CHECK(elf_wrapper);

        elf_wrapper_ = elf_wrapper;

        Memory *memory = elf_wrapper->GetMemoryBackedElf()->memory();

        InitDebugFrame<AddressType>(
                memory, debug_frame_info, debug_frame_);
        InitEhFrame<AddressType>(
                memory, eh_frame_hdr_info, eh_frame_info, eh_frame_);

        if (elf_wrapper->GetGnuDebugDataInterface()) {
            QUT_LOG("QuickenInMemory::Init elf %s has gnu_debugdata_interface",
                    elf_wrapper->GetSoname().c_str());
            Memory *gnu_debug_data_memory = elf_wrapper->GetGnuDebugDataInterface()->memory();
            InitDebugFrame<AddressType>(
                    gnu_debug_data_memory, gnu_debug_frame_info, debug_frame_from_gnu_debug_data_);
            InitEhFrame<AddressType>(
                    gnu_debug_data_memory, gnu_eh_frame_hdr_info, gnu_eh_frame_info,
                    eh_frame_from_gnu_debug_data_);
        }

        QUT_LOG("QuickenInMemory::Init elf %s, %llx, %llx, %llx, %llx", elf_wrapper->GetSoname().c_str(),
                (ullint_t) debug_frame_.get(), (ullint_t) eh_frame_.get(),
                (ullint_t) debug_frame_from_gnu_debug_data_.get(),
                (ullint_t) eh_frame_from_gnu_debug_data_.get());

        if (arm_exidx_info.size_ > 0) {
            exidx_decoder_ = std::make_unique<ExidxDecoderHelper>(memory, process_memory.get());
            exidx_decoder_->Init(arm_exidx_info);
        }

        process_memory_ = process_memory;
    }

    template<typename AddressType>
    inline bool QuickenInMemory<AddressType>::FindInCache(
            uint64_t pc,
            /* out */ std::shared_ptr<QutSectionsInMemory> &fut_sections) {

        if (log) {
            for (const auto &it : qut_in_memory_) {
                (void) it;
                QUT_LOG("GetFutSectionsInMemory dump cache qut [%llx, %llx]",
                        it.second->pc_start, it.second->pc_end);
            }
        }

        std::lock_guard<std::mutex> guard(lock_cache_);
        if (!qut_in_memory_.empty()) {
            auto it = qut_in_memory_.upper_bound(pc);
            if (it != qut_in_memory_.begin()) {
                it--;
            }

            QUT_LOG("GetFutSectionsInMemory found cache qut pc %llx in range of fde[%llx, %llx]",
                    pc, it->second->pc_start, it->second->pc_end);
            if (pc >= it->second->pc_start && pc <= it->second->pc_end) {
                fut_sections = it->second;
                return true;
            }
        }

        return false;
    }

    template<typename AddressType>
    inline void QuickenInMemory<AddressType>::UpdateCache(
            uint64_t pc_start, uint64_t pc_end,
            /* out */ shared_ptr<QutSectionsInMemory> &fut_sections) {

        if (log) {
            for (size_t i = 0; i < fut_sections->idx_size; i += 1) {
                QUT_LOG("GetFutSectionsInMemory dump fut sections -> %llx",
                        fut_sections->quidx[i]);
            }
        }

        fut_sections->pc_start = pc_start;
        fut_sections->pc_end = pc_end;

        std::lock_guard<std::mutex> guard(lock_cache_);
        qut_in_memory_[pc_start] = fut_sections;
    }

    template<typename AddressType>
    bool QuickenInMemory<AddressType>::GenerateFutSectionsInMemoryForJIT(
            Elf *elf,
            Memory *process_memory,
            uint64_t pc,
            /* out */ std::shared_ptr<QutSectionsInMemory> &fut_sections) {

        CHECK(elf);

        const DwarfFde *fde;

        wechat_backtrace::FrameInfo debug_frame_info;
        Memory *gnu_debug_data_memory = nullptr;

        {
            std::lock_guard<std::mutex> guard(lock_decode_);
            if (elf->gnu_debugdata_interface()) {
                gnu_debug_data_memory = elf->gnu_debugdata_interface()->memory();
                debug_frame_info = {
                        .offset_ = elf->gnu_debugdata_interface()->debug_frame_offset(),
                        .section_bias_ = elf->gnu_debugdata_interface()->debug_frame_section_bias(),
                        .size_ = elf->gnu_debugdata_interface()->debug_frame_size(),
                };
                fde = elf->gnu_debugdata_interface()->debug_frame()->GetFdeFromPc(pc);
            } else {
                debug_frame_info = {
                        .offset_ = elf->interface()->debug_frame_offset(),
                        .section_bias_ = elf->interface()->debug_frame_section_bias(),
                        .size_ = elf->interface()->debug_frame_size(),
                };
                fde = elf->interface()->debug_frame()->GetFdeFromPc(pc);
            }
        }

        if (fde == nullptr) {
            return false;
        }

        QuickenTableGenerator<addr_t>
                generator(elf->memory(), gnu_debug_data_memory, process_memory);

        auto fut_sections_sp = make_shared<QutSectionsInMemory>();
        bool ret = generator.GenerateSingleQUTSections(
                debug_frame_info, fde, fut_sections_sp.get(),
                gnu_debug_data_memory != nullptr);

        if (ret) {
            QUT_LOG("GetFutSectionsInMemory found pc %llx in range of fde[%llx, %llx]", pc,
                    fde->pc_start, fde->pc_end);
            fut_sections = fut_sections_sp;
            UpdateCache(fde->pc_start, fde->pc_end, fut_sections);
            return true;
        }
        return false;
    }

    template<typename AddressType>
    bool QuickenInMemory<AddressType>::GetFutSectionsInMemory(
            uint64_t pc,
            /* out */ std::shared_ptr<QutSectionsInMemory> &fut_sections) {

        if (FindInCache(pc, fut_sections)) {
            return true;
        }

        QUT_LOG("GetFutSectionsInMemory miss cache qut pc %llx", pc);

        uint64_t pc_start = 0;
        uint64_t pc_end = 0;
        bool ret = false;
        std::shared_ptr<wechat_backtrace::QutSectionsInMemory> fut_sections_sp
                = std::make_shared<wechat_backtrace::QutSectionsInMemory>();
        {
            const DwarfFde *fde = nullptr;
            std::lock_guard<std::mutex> guard(lock_decode_);
            Elf *elf = elf_wrapper_->GetMemoryBackedElf().get();

            DwarfSectionDecoder<AddressType> *decoder = nullptr;

            if (elf->interface()->eh_frame()) {
                fde = elf->interface()->eh_frame()->GetFdeFromPc(pc);
                decoder = eh_frame_.get();
                QUT_LOG("GetFutSectionsInMemory decoder eh_frame_");
            }
            // Memory backed elf no debug frame.
            /*
            else if (elf->interface()->debug_frame()) {
                fde = elf->interface()->debug_frame()->GetFdeFromPc(pc);
                decoder = debug_frame_.get();
                QUT_LOG("GetFutSectionsInMemory decoder debug_frame_");
            }
            */
            else if (elf_wrapper_->GetGnuDebugDataInterface()) {
                if (elf_wrapper_->GetGnuDebugDataInterface()->eh_frame()) {
                    fde = elf_wrapper_->GetGnuDebugDataInterface()->eh_frame()->GetFdeFromPc(pc);
                    decoder = eh_frame_from_gnu_debug_data_.get();
                    QUT_LOG("GetFutSectionsInMemory decoder eh_frame_from_gnu_debug_data_");
                } else if (elf_wrapper_->GetGnuDebugDataInterface()->debug_frame()) {
                    fde = elf_wrapper_->GetGnuDebugDataInterface()->debug_frame()->GetFdeFromPc(pc);
                    decoder = debug_frame_from_gnu_debug_data_.get();
                    QUT_LOG("GetFutSectionsInMemory decoder debug_frame_from_gnu_debug_data_");
                }
            }

            QUT_LOG("GetFutSectionsInMemory get fde -> %llx, pc -> %llx.", fde, pc);

            if (fde) {
                if (UNLIKELY(decoder == nullptr)) {
                    QUT_LOG("GetFutSectionsInMemory get null decoder, pc -> %llx.", pc);
                    return false;
                }

                QuickenTableGenerator<AddressType>
                        generator(/* no use */ nullptr, /* no use */ nullptr,
                                               process_memory_.get());

                ret = generator.GenerateSingleQUTSections(decoder, fde, fut_sections_sp.get());

                pc_start = fde->pc_start;
                pc_end = fde->pc_end;
            } else if (exidx_decoder_) {
                // Maybe Arm exidx
                auto instructions = make_shared<QutInstructionsOfEntries>();
                if (exidx_decoder_->DecodeEntry(pc, instructions, &pc_start, &pc_end)) {
                    QuickenTableGenerator<AddressType>
                            generator(/* no use */ nullptr, /* no use */ nullptr, /* no use */
                                                   nullptr);
                    ret = generator.PackEntriesToQutSections(instructions.get(),
                                                             fut_sections_sp.get());
                } else {
                    QUT_LOG("GetFutSectionsInMemory decode exidx failed, pc -> %llx.", pc);
                }
            }
        }

        if (ret) {
            QUT_LOG("GetFutSectionsInMemory found pc %llx in range of fde[%llx, %llx]", pc,
                    pc_start, pc_end);
            fut_sections = fut_sections_sp;
            UpdateCache(pc_start, pc_end, fut_sections);
            return true;
        }

        return false;
    }

    template void QuickenInMemory<uint32_t>::Init(
            ElfWrapper *elf_wrapper,
            const std::shared_ptr<unwindstack::Memory> &process_memory,
            FrameInfo &eh_frame_hdr_info,
            FrameInfo &eh_frame_info,
            FrameInfo &debug_frame_info,
            FrameInfo &gnu_eh_frame_hdr_info,
            FrameInfo &gnu_eh_frame_info,
            FrameInfo &gnu_debug_frame_info,
            FrameInfo &arm_exidx_info);

    template void QuickenInMemory<uint64_t>::Init(
            ElfWrapper *elf_wrapper,
            const std::shared_ptr<unwindstack::Memory> &process_memory,
            FrameInfo &eh_frame_hdr_info,
            FrameInfo &eh_frame_info,
            FrameInfo &debug_frame_info,
            FrameInfo &gnu_eh_frame_hdr_info,
            FrameInfo &gnu_eh_frame_info,
            FrameInfo &gnu_debug_frame_info,
            FrameInfo &arm_exidx_info);

    template bool QuickenInMemory<uint32_t>::GetFutSectionsInMemory(
            uint64_t pc,
            /* out */ std::shared_ptr<wechat_backtrace::QutSectionsInMemory> &fut_sections);

    template bool QuickenInMemory<uint64_t>::GetFutSectionsInMemory(
            uint64_t pc,
            /* out */ std::shared_ptr<wechat_backtrace::QutSectionsInMemory> &fut_sections);

    template bool QuickenInMemory<uint32_t>::GenerateFutSectionsInMemoryForJIT(
            Elf *elf,
            Memory *memory,
            uint64_t pc,
            /* out */ std::shared_ptr<QutSectionsInMemory> &fut_sections);

    template bool QuickenInMemory<uint64_t>::GenerateFutSectionsInMemoryForJIT(
            Elf *elf,
            Memory *memory,
            uint64_t pc,
            /* out */ std::shared_ptr<QutSectionsInMemory> &fut_sections);

}  // namespace wechat_backtrace