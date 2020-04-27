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

#include <algorithm>
#include <string>
#include <vector>

#include <unwindstack/Memory.h>

#include "Check.h"
#include "Symbols.h"

namespace unwindstack {

Symbols::Symbols(uint64_t offset, uint64_t size, uint64_t entry_size, uint64_t str_offset,
                 uint64_t str_size)
    : offset_(offset),
      count_(entry_size != 0 ? size / entry_size : 0),
      entry_size_(entry_size),
      str_offset_(str_offset),
      str_end_(str_offset_ + str_size) {}

template <typename SymType>
static bool IsFunc(const SymType* entry) {
  return entry->st_shndx != SHN_UNDEF && ELF32_ST_TYPE(entry->st_info) == STT_FUNC;
}

// Read symbol entry from memory and cache it so we don't have to read it again.
template <typename SymType>
inline __attribute__((__always_inline__)) const Symbols::Info* Symbols::ReadFuncInfo(
    uint32_t symbol_index, Memory* elf_memory) {
  auto it = symbols_.find(symbol_index);
  if (it != symbols_.end()) {
    return &it->second;
  }
  SymType sym;
  if (!elf_memory->ReadFully(offset_ + symbol_index * entry_size_, &sym, sizeof(sym))) {
    return nullptr;
  }
  if (!IsFunc(&sym)) {
    // We need the address for binary search, but we don't want it to be matched.
    sym.st_size = 0;
  }
  Info info{.addr = sym.st_value, .size = static_cast<uint32_t>(sym.st_size), .name = sym.st_name};
  return &symbols_.emplace(symbol_index, info).first->second;
}

// Binary search the symbol table to find function containing the given address.
// Without remap, the symbol table is assumed to be sorted and accessed directly.
// If the symbol table is not sorted this method might fail but should not crash.
// When the indices are remapped, they are guaranteed to be sorted by address.
template <typename SymType, bool RemapIndices>
const Symbols::Info* Symbols::BinarySearch(uint64_t addr, Memory* elf_memory) {
  size_t first = 0;
  size_t last = RemapIndices ? remap_->size() : count_;
  while (first < last) {
    size_t current = first + (last - first) / 2;
    size_t symbol_index = RemapIndices ? remap_.value()[current] : current;
    const Info* info = ReadFuncInfo<SymType>(symbol_index, elf_memory);
    if (info == nullptr) {
      return nullptr;
    }
    if (addr < info->addr) {
      last = current;
    } else if (addr < info->addr + info->size) {
      return info;
    } else {
      first = current + 1;
    }
  }
  return nullptr;
}

// Create remapping table which allows us to access symbols as if they were sorted by address.
template <typename SymType>
void Symbols::BuildRemapTable(Memory* elf_memory) {
  std::vector<uint64_t> addrs;  // Addresses of all symbols (addrs[i] == symbols[i].st_value).
  addrs.reserve(count_);
  remap_.emplace();  // Construct the optional remap table.
  remap_->reserve(count_);
  for (size_t symbol_idx = 0; symbol_idx < count_;) {
    // Read symbols from memory.  We intentionally bypass the cache to save memory.
    // Do the reads in batches so that we minimize the number of memory read calls.
    uint8_t buffer[1024];
    size_t read = std::min<size_t>(sizeof(buffer), (count_ - symbol_idx) * entry_size_);
    size_t size = elf_memory->Read(offset_ + symbol_idx * entry_size_, buffer, read);
    if (size < sizeof(SymType)) {
      break;  // Stop processing, something looks like it is corrupted.
    }
    for (size_t offset = 0; offset + sizeof(SymType) <= size; offset += entry_size_, symbol_idx++) {
      SymType sym;
      memcpy(&sym, &buffer[offset], sizeof(SymType));  // Copy to ensure alignment.
      addrs.push_back(sym.st_value);  // Always insert so it is indexable by symbol index.
      if (IsFunc(&sym)) {
        remap_->push_back(symbol_idx);  // Indices of function symbols only.
      }
    }
  }
  // Sort by address to make the remap list binary searchable (stable due to the a<b tie break).
  auto comp = [&addrs](auto a, auto b) { return std::tie(addrs[a], a) < std::tie(addrs[b], b); };
  std::sort(remap_->begin(), remap_->end(), comp);
  // Remove duplicate entries (methods de-duplicated by the linker).
  auto pred = [&addrs](auto a, auto b) { return addrs[a] == addrs[b]; };
  remap_->erase(std::unique(remap_->begin(), remap_->end(), pred), remap_->end());
  remap_->shrink_to_fit();
}

template <typename SymType>
bool Symbols::GetName(uint64_t addr, Memory* elf_memory, std::string* name, uint64_t* func_offset) {
  const Info* info;
  if (!remap_.has_value()) {
    // Assume the symbol table is sorted. If it is not, this will gracefully fail.
    info = BinarySearch<SymType, false>(addr, elf_memory);
    if (info == nullptr) {
      // Create the remapping table and retry the search.
      BuildRemapTable<SymType>(elf_memory);
      symbols_.clear();  // Remove cached symbols since the access pattern will be different.
      info = BinarySearch<SymType, true>(addr, elf_memory);
    }
  } else {
    // Fast search using the previously created remap table.
    info = BinarySearch<SymType, true>(addr, elf_memory);
  }
  if (info == nullptr) {
    return false;
  }
  // Read the function name from the string table.
  *func_offset = addr - info->addr;
  uint64_t str = str_offset_ + info->name;
  return str < str_end_ && elf_memory->ReadString(str, name, str_end_ - str);
}

template <typename SymType>
bool Symbols::GetGlobal(Memory* elf_memory, const std::string& name, uint64_t* memory_address) {
  for (uint32_t i = 0; i < count_; i++) {
    SymType entry;
    if (!elf_memory->ReadFully(offset_ + i * entry_size_, &entry, sizeof(entry))) {
      return false;
    }

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
template bool Symbols::GetName<Elf32_Sym>(uint64_t, Memory*, std::string*, uint64_t*);
template bool Symbols::GetName<Elf64_Sym>(uint64_t, Memory*, std::string*, uint64_t*);

template bool Symbols::GetGlobal<Elf32_Sym>(Memory*, const std::string&, uint64_t*);
template bool Symbols::GetGlobal<Elf64_Sym>(Memory*, const std::string&, uint64_t*);
}  // namespace unwindstack
