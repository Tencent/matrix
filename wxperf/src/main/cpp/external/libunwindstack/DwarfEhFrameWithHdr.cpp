/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <stdint.h>

#include <unwindstack/DwarfError.h>
#include <unwindstack/DwarfStructs.h>
#include <unwindstack/Memory.h>

#include "Check.h"
#include "DwarfEhFrameWithHdr.h"

namespace unwindstack {

template <typename AddressType>
bool DwarfEhFrameWithHdr<AddressType>::Init(uint64_t offset, uint64_t size) {
  uint8_t data[4];

  memory_.clear_func_offset();
  memory_.clear_text_offset();
  memory_.set_data_offset(offset);
  memory_.set_cur_offset(offset);

  // Read the first four bytes all at once.
  if (!memory_.ReadBytes(data, 4)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }

  version_ = data[0];
  if (version_ != 1) {
    // Unknown version.
    last_error_.code = DWARF_ERROR_UNSUPPORTED_VERSION;
    return false;
  }

  ptr_encoding_ = data[1];
  uint8_t fde_count_encoding = data[2];
  table_encoding_ = data[3];
  table_entry_size_ = memory_.template GetEncodedSize<AddressType>(table_encoding_);

  memory_.set_pc_offset(memory_.cur_offset());
  if (!memory_.template ReadEncodedValue<AddressType>(ptr_encoding_, &ptr_offset_)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }

  memory_.set_pc_offset(memory_.cur_offset());
  if (!memory_.template ReadEncodedValue<AddressType>(fde_count_encoding, &fde_count_)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }

  if (fde_count_ == 0) {
    last_error_.code = DWARF_ERROR_NO_FDES;
    return false;
  }

  entries_offset_ = memory_.cur_offset();
  entries_end_ = offset + size;
  entries_data_offset_ = offset;
  cur_entries_offset_ = entries_offset_;

  return true;
}

template <typename AddressType>
const DwarfFde* DwarfEhFrameWithHdr<AddressType>::GetFdeFromIndex(size_t index) {
  const FdeInfo* info = GetFdeInfoFromIndex(index);
  if (info == nullptr) {
    return nullptr;
  }
  return this->GetFdeFromOffset(info->offset);
}

template <typename AddressType>
const typename DwarfEhFrameWithHdr<AddressType>::FdeInfo*
DwarfEhFrameWithHdr<AddressType>::GetFdeInfoFromIndex(size_t index) {
  auto entry = fde_info_.find(index);
  if (entry != fde_info_.end()) {
    return &fde_info_[index];
  }
  FdeInfo* info = &fde_info_[index];

  memory_.set_data_offset(entries_data_offset_);
  memory_.set_cur_offset(entries_offset_ + 2 * index * table_entry_size_);
  memory_.set_pc_offset(memory_.cur_offset());
  uint64_t value;
  if (!memory_.template ReadEncodedValue<AddressType>(table_encoding_, &value) ||
      !memory_.template ReadEncodedValue<AddressType>(table_encoding_, &info->offset)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    fde_info_.erase(index);
    return nullptr;
  }
  info->pc = value;
  return info;
}

template <typename AddressType>
bool DwarfEhFrameWithHdr<AddressType>::GetFdeOffsetBinary(uint64_t pc, uint64_t* fde_offset,
                                                          uint64_t total_entries) {
  CHECK(fde_count_ > 0);
  CHECK(total_entries <= fde_count_);

  size_t first = 0;
  size_t last = total_entries;
  while (first < last) {
    size_t current = (first + last) / 2;
    const FdeInfo* info = GetFdeInfoFromIndex(current);
    if (info == nullptr) {
      return false;
    }
    if (pc == info->pc) {
      *fde_offset = info->offset;
      return true;
    }
    if (pc < info->pc) {
      last = current;
    } else {
      first = current + 1;
    }
  }
  if (last != 0) {
    const FdeInfo* info = GetFdeInfoFromIndex(last - 1);
    if (info == nullptr) {
      return false;
    }
    *fde_offset = info->offset;
    return true;
  }
  return false;
}

template <typename AddressType>
bool DwarfEhFrameWithHdr<AddressType>::GetFdeOffsetSequential(uint64_t pc, uint64_t* fde_offset) {
  CHECK(fde_count_ != 0);
  last_error_.code = DWARF_ERROR_NONE;
  last_error_.address = 0;

  // We can do a binary search if the pc is in the range of the elements
  // that have already been cached.
  if (!fde_info_.empty()) {
    const FdeInfo* info = &fde_info_[fde_info_.size() - 1];
    if (pc >= info->pc) {
      *fde_offset = info->offset;
      return true;
    }
    if (pc < info->pc) {
      return GetFdeOffsetBinary(pc, fde_offset, fde_info_.size());
    }
  }

  if (cur_entries_offset_ == 0) {
    // All entries read, or error encountered.
    return false;
  }

  memory_.set_data_offset(entries_data_offset_);
  memory_.set_cur_offset(cur_entries_offset_);
  cur_entries_offset_ = 0;

  FdeInfo* prev_info = nullptr;
  for (size_t current = fde_info_.size();
       current < fde_count_ && memory_.cur_offset() < entries_end_; current++) {
    memory_.set_pc_offset(memory_.cur_offset());
    uint64_t value;
    if (!memory_.template ReadEncodedValue<AddressType>(table_encoding_, &value)) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }

    FdeInfo* info = &fde_info_[current];
    if (!memory_.template ReadEncodedValue<AddressType>(table_encoding_, &info->offset)) {
      fde_info_.erase(current);
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }
    info->pc = value + 4;

    if (pc < info->pc) {
      if (prev_info == nullptr) {
        return false;
      }
      cur_entries_offset_ = memory_.cur_offset();
      *fde_offset = prev_info->offset;
      return true;
    }
    prev_info = info;
  }

  if (fde_count_ == fde_info_.size() && pc >= prev_info->pc) {
    *fde_offset = prev_info->offset;
    return true;
  }
  return false;
}

template <typename AddressType>
bool DwarfEhFrameWithHdr<AddressType>::GetFdeOffsetFromPc(uint64_t pc, uint64_t* fde_offset) {
  if (fde_count_ == 0) {
    return false;
  }

  if (table_entry_size_ > 0) {
    // Do a binary search since the size of each table entry is fixed.
    return GetFdeOffsetBinary(pc, fde_offset, fde_count_);
  } else {
    // Do a sequential search since each table entry size is variable.
    return GetFdeOffsetSequential(pc, fde_offset);
  }
}

// Explicitly instantiate DwarfEhFrameWithHdr
template class DwarfEhFrameWithHdr<uint32_t>;
template class DwarfEhFrameWithHdr<uint64_t>;

}  // namespace unwindstack
