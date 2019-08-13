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

#include <elf.h>
#include <stdint.h>

#include <string>

#include <unwindstack/Memory.h>

#include "Check.h"
#include "Symbols.h"

namespace unwindstack {

Symbols::Symbols(uint64_t offset, uint64_t size, uint64_t entry_size, uint64_t str_offset,
                 uint64_t str_size)
    : cur_offset_(offset),
      offset_(offset),
      end_(offset + size),
      entry_size_(entry_size),
      str_offset_(str_offset),
      str_end_(str_offset_ + str_size) {}

const Symbols::Info* Symbols::GetInfoFromCache(uint64_t addr) {
  // Binary search the table.
  size_t first = 0;
  size_t last = symbols_.size();
  while (first < last) {
    size_t current = first + (last - first) / 2;
    const Info* info = &symbols_[current];
    if (addr < info->start_offset) {
      last = current;
    } else if (addr < info->end_offset) {
      return info;
    } else {
      first = current + 1;
    }
  }
  return nullptr;
}

template <typename SymType>
bool Symbols::GetName(uint64_t addr, uint64_t load_bias, Memory* elf_memory, std::string* name,
                      uint64_t* func_offset) {
  addr += load_bias;

  if (symbols_.size() != 0) {
    const Info* info = GetInfoFromCache(addr);
    if (info) {
      CHECK(addr >= info->start_offset && addr <= info->end_offset);
      *func_offset = addr - info->start_offset;
      return elf_memory->ReadString(info->str_offset, name, str_end_ - info->str_offset);
    }
  }

  bool symbol_added = false;
  bool return_value = false;
  while (cur_offset_ + entry_size_ <= end_) {
    SymType entry;
    if (!elf_memory->ReadFully(cur_offset_, &entry, sizeof(entry))) {
      // Stop all processing, something looks like it is corrupted.
      cur_offset_ = UINT64_MAX;
      return false;
    }
    cur_offset_ += entry_size_;

    if (entry.st_shndx != SHN_UNDEF && ELF32_ST_TYPE(entry.st_info) == STT_FUNC) {
      // Treat st_value as virtual address.
      uint64_t start_offset = entry.st_value;
      if (entry.st_shndx != SHN_ABS) {
        start_offset += load_bias;
      }
      uint64_t end_offset = start_offset + entry.st_size;

      // Cache the value.
      symbols_.emplace_back(start_offset, end_offset, str_offset_ + entry.st_name);
      symbol_added = true;

      if (addr >= start_offset && addr < end_offset) {
        *func_offset = addr - start_offset;
        uint64_t offset = str_offset_ + entry.st_name;
        if (offset < str_end_) {
          return_value = elf_memory->ReadString(offset, name, str_end_ - offset);
        }
        break;
      }
    }
  }

  if (symbol_added) {
    std::sort(symbols_.begin(), symbols_.end(),
              [](const Info& a, const Info& b) { return a.start_offset < b.start_offset; });
  }
  return return_value;
}

template <typename SymType>
bool Symbols::GetGlobal(Memory* elf_memory, const std::string& name, uint64_t* memory_address) {
  uint64_t cur_offset = offset_;
  while (cur_offset + entry_size_ <= end_) {
    SymType entry;
    if (!elf_memory->ReadFully(cur_offset, &entry, sizeof(entry))) {
      return false;
    }
    cur_offset += entry_size_;

    if (entry.st_shndx != SHN_UNDEF && ELF32_ST_TYPE(entry.st_info) == STT_OBJECT &&
        ELF32_ST_BIND(entry.st_info) == STB_GLOBAL) {
      uint64_t str_offset = str_offset_ + entry.st_name;
      if (str_offset < str_end_) {
        std::string symbol;
        if (elf_memory->ReadString(str_offset, &symbol, str_end_ - str_offset) && symbol == name) {
          *memory_address = entry.st_value;
          return true;
        }
      }
    }
  }
  return false;
}

// Instantiate all of the needed template functions.
template bool Symbols::GetName<Elf32_Sym>(uint64_t, uint64_t, Memory*, std::string*, uint64_t*);
template bool Symbols::GetName<Elf64_Sym>(uint64_t, uint64_t, Memory*, std::string*, uint64_t*);

template bool Symbols::GetGlobal<Elf32_Sym>(Memory*, const std::string&, uint64_t*);
template bool Symbols::GetGlobal<Elf64_Sym>(Memory*, const std::string&, uint64_t*);
}  // namespace unwindstack
