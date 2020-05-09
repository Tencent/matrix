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

#ifndef _LIBUNWINDSTACK_SYMBOLS_H
#define _LIBUNWINDSTACK_SYMBOLS_H

#include <stdint.h>

#include <optional>
#include <string>
#include <unordered_map>

namespace unwindstack {

// Forward declaration.
class Memory;

class Symbols {
  struct Info {
    uint64_t addr;  // Symbol address.
    uint32_t size;  // Symbol size in bytes. Zero if not a function.
    uint32_t name;  // Offset in .strtab.
  };

 public:
  Symbols(uint64_t offset, uint64_t size, uint64_t entry_size, uint64_t str_offset,
          uint64_t str_size);
  virtual ~Symbols() = default;

  template <typename SymType>
  bool GetName(uint64_t addr, Memory* elf_memory, std::string* name, uint64_t* func_offset);

  template <typename SymType>
  bool GetGlobal(Memory* elf_memory, const std::string& name, uint64_t* memory_address);

  void ClearCache() {
    symbols_.clear();
    remap_.reset();
  }

 private:
  template <typename SymType>
  const Info* ReadFuncInfo(uint32_t symbol_index, Memory* elf_memory);

  template <typename SymType, bool RemapIndices>
  const Info* BinarySearch(uint64_t addr, Memory* elf_memory);

  template <typename SymType>
  void BuildRemapTable(Memory* elf_memory);

  const uint64_t offset_;
  const uint64_t count_;
  const uint64_t entry_size_;
  const uint64_t str_offset_;
  const uint64_t str_end_;

  std::unordered_map<uint32_t, Info> symbols_;  // Cache of read symbols (keyed by symbol index).
  std::optional<std::vector<uint32_t>> remap_;  // Indices of function symbols sorted by address.
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_SYMBOLS_H
