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

#ifndef _LIBWECHATBACKTRACE_DWARF_EH_FRAME_WITH_HDR_H
#define _LIBWECHATBACKTRACE_DWARF_EH_FRAME_WITH_HDR_H

#include <stdint.h>

#include <unordered_map>

#include <unwindstack/DwarfSection.h>
#include "DwarfSectionDecoder.h"

namespace unwindstack {
    // Forward declarations.
    class Memory;
}

namespace wechat_backtrace {

    template<typename AddressType>
    class DwarfEhFrameWithHdrDecoder : public DwarfSectionDecoder<AddressType> {
    public:
        // Add these so that the protected members of DwarfSectionImpl
        // can be accessed without needing a this->.
        using DwarfSectionDecoder<AddressType>::memory_;
        using DwarfSectionDecoder<AddressType>::last_error_;

        struct FdeInfo {
            AddressType pc;
            uint64_t offset;
        };

        DwarfEhFrameWithHdrDecoder(unwindstack::Memory *memory) : DwarfSectionDecoder<AddressType>(
                memory) {}

        virtual ~DwarfEhFrameWithHdrDecoder() = default;

        uint64_t GetCieOffsetFromFde32(uint32_t pointer) override {
            return memory_.cur_offset() - pointer - 4;
        }

        uint64_t GetCieOffsetFromFde64(uint64_t pointer) override {
            return memory_.cur_offset() - pointer - 8;
        }

        uint64_t AdjustPcFromFde(uint64_t pc) override {
            // The eh_frame uses relative pcs.
            return pc + memory_.cur_offset() - 4;
        }

        bool EhFrameInit(uint64_t offset, uint64_t size, int64_t section_bias);

        bool Init(uint64_t offset, uint64_t size, int64_t section_bias) override;

        const unwindstack::DwarfFde *GetFdeFromPc(uint64_t pc) override;

        bool GetFdeOffsetFromPc(uint64_t pc, uint64_t *fde_offset);

        const FdeInfo *GetFdeInfoFromIndex(size_t index);

    protected:
        uint8_t version_ = 0;
        uint8_t table_encoding_ = 0;
        size_t table_entry_size_ = 0;

        uint64_t hdr_entries_offset_ = 0;
        uint64_t hdr_entries_data_offset_ = 0;
        uint64_t hdr_section_bias_ = 0;

        uint64_t fde_count_ = 0;
        std::unordered_map<uint64_t, FdeInfo> fde_info_;
    };

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_DWARF_EH_FRAME_WITH_HDR_H
