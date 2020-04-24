/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _LIBUNWINDSTACK_DWARF_EH_FRAME_WITH_HDR_H
#define _LIBUNWINDSTACK_DWARF_EH_FRAME_WITH_HDR_H

#include <stdint.h>

#include <unordered_map>

#include <unwindstack/DwarfSection.h>

namespace unwindstack {

// Forward declarations.
class Memory;

template <typename AddressType>
class DwarfEhFrameWithHdr : public DwarfSectionImpl<AddressType> {
 public:
  // Add these so that the protected members of DwarfSectionImpl
  // can be accessed without needing a this->.
  using DwarfSectionImpl<AddressType>::memory_;
  using DwarfSectionImpl<AddressType>::pc_offset_;
  using DwarfSectionImpl<AddressType>::entries_offset_;
  using DwarfSectionImpl<AddressType>::entries_end_;
  using DwarfSectionImpl<AddressType>::last_error_;
  using DwarfSectionImpl<AddressType>::load_bias_;

  struct FdeInfo {
    AddressType pc;
    uint64_t offset;
  };

  DwarfEhFrameWithHdr(Memory* memory) : DwarfSectionImpl<AddressType>(memory) {}
  virtual ~DwarfEhFrameWithHdr() = default;

  uint64_t GetCieOffsetFromFde32(uint32_t pointer) override {
    return this->memory_.cur_offset() - pointer - 4;
  }

  uint64_t GetCieOffsetFromFde64(uint64_t pointer) override {
    return this->memory_.cur_offset() - pointer - 8;
  }

  uint64_t AdjustPcFromFde(uint64_t pc) override {
    // The eh_frame uses relative pcs.
    return pc + this->memory_.cur_offset() - 4;
  }

  bool Init(uint64_t offset, uint64_t size, uint64_t load_bias) override;

  const DwarfFde* GetFdeFromPc(uint64_t pc) override;

  bool GetFdeOffsetFromPc(uint64_t pc, uint64_t* fde_offset);

  const FdeInfo* GetFdeInfoFromIndex(size_t index);

  void GetFdes(std::vector<const DwarfFde*>* fdes) override;

 protected:
  uint8_t version_;
  uint8_t ptr_encoding_;
  uint8_t table_encoding_;
  size_t table_entry_size_;

  uint64_t ptr_offset_;

  uint64_t entries_data_offset_;
  uint64_t cur_entries_offset_ = 0;

  uint64_t fde_count_;
  std::unordered_map<uint64_t, FdeInfo> fde_info_;
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_DWARF_EH_FRAME_WITH_HDR_H
