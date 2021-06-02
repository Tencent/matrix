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

#ifndef _LIBWECHATBACKTRACE_QUICKEN_IN_MEMORY_H
#define _LIBWECHATBACKTRACE_QUICKEN_IN_MEMORY_H

#include <stdint.h>
#include <memory>
#include <mutex>
#include "Errors.h"
#include "DwarfSectionDecoder.h"
#include "QuickenTable.h"
#include "QuickenTableGenerator.h"

namespace wechat_backtrace {

    class QuickenInterface;

    class ExidxDecoderHelper {

    public:
        ExidxDecoderHelper(unwindstack::Memory *memory, unwindstack::Memory *process_memory)
                : memory_(memory), process_memory_(process_memory) {};

        ~ExidxDecoderHelper() {};

        void Init(FrameInfo frame_info);

        bool
        GetAddr(size_t idx,
                /* out */ uint32_t *addr
        );

        bool
        FindEntry(uint32_t pc,
                /* out */ uint32_t *entry_offset,
                /* out */ uint32_t *start_addr,
                /* out */ uint32_t *end_addr
        );

        bool
        GetPrel31Addr(uint32_t offset,
                /* out */ uint32_t *addr
        );

        bool
        DecodeEntry(uint32_t pc,
                /* out */ const std::shared_ptr<QutInstructionsOfEntries>& instructions,
                /* out */ uint64_t *pc_start,
                /* out */ uint64_t *pc_end
        );

    protected:
        unwindstack::Memory *memory_;
        unwindstack::Memory *process_memory_;

        uint64_t start_offset_ = 0;
        size_t total_entries_ = 0;

        std::unordered_map<size_t, uint32_t> addrs_;
    };

    template<typename AddressType>
    class QuickenInMemory {
    public:
        QuickenInMemory() {};

        ~QuickenInMemory() {};

        void
        Init(unwindstack::Elf *elf, const std::shared_ptr<unwindstack::Memory>& process_memory,
             FrameInfo &eh_frame_hdr_info, FrameInfo &eh_frame_info,
             FrameInfo &debug_frame_info,
             FrameInfo &gnu_eh_frame_hdr_info, FrameInfo &gnu_eh_frame_info,
             FrameInfo &gnu_debug_frame_info, FrameInfo &arm_exidx_info); // No Init for DebugJIT.


        bool FindInCache(
                uint64_t pc,
                /* out */ std::shared_ptr<QutSectionsInMemory> &fut_sections);

        void UpdateCache(
                uint64_t pc_start, uint64_t pc_end,
                /* out */ std::shared_ptr<QutSectionsInMemory> &fut_sections);

        bool GetFutSectionsInMemory(
                uint64_t pc,
                /* out */ std::shared_ptr<QutSectionsInMemory> &fut_sections);


        bool GenerateFutSectionsInMemoryForJIT(
                unwindstack::Elf *elf,
                unwindstack::Memory *memory,
                uint64_t pc,
                /* out */ std::shared_ptr<QutSectionsInMemory> &fut_sections);

        const bool log = false;
    protected:
        std::unique_ptr<DwarfSectionDecoder<AddressType>> eh_frame_;
        std::unique_ptr<DwarfSectionDecoder<AddressType>> debug_frame_;
        std::unique_ptr<DwarfSectionDecoder<AddressType>> eh_frame_from_gnu_debug_data_;
        std::unique_ptr<DwarfSectionDecoder<AddressType>> debug_frame_from_gnu_debug_data_;
        std::unique_ptr<ExidxDecoderHelper> exidx_decoder_;

        std::unique_ptr<unwindstack::Elf> elf_;

        std::shared_ptr<unwindstack::Memory> process_memory_;

        std::mutex lock_decode_;
        std::mutex lock_cache_;

        std::map<uint64_t, std::shared_ptr<QutSectionsInMemory>> qut_in_memory_;

    };

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_IN_MEMORY_H
