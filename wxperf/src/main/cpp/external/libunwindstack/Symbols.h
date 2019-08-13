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

#include <string>
#include <vector>

namespace unwindstack {

// Forward declaration.
class Memory;

class Symbols {
  struct Info {
    Info(uint64_t start_offset, uint64_t end_offset, uint64_t str_offset)
        : start_offset(start_offset), end_offset(end_offset), str_offset(str_offset) {}
    uint64_t start_offset;
    uint64_t end_offset;
    uint64_t str_offset;
  };

 public:
  Symbols(uint64_t offset, uint64_t size, uint64_t entry_size, uint64_t str_offset,
          uint64_t str_size);
  virtual ~Symbols() = default;

  const Info* GetInfoFromCache(uint64_t addr);

  template <typename SymType>
  bool GetName(uint64_t addr, uint64_t load_bias, Memory* elf_memory, std::string* name,
               uint64_t* func_offset);

  template <typename SymType>
  bool GetGlobal(Memory* elf_memory, const std::string& name, uint64_t* memory_address);

  void ClearCache() {
    symbols_.clear();
    cur_offset_ = offset_;
  }

 private:
  uint64_t cur_offset_;
  uint64_t offset_;
  uint64_t end_;
  uint64_t entry_size_;
  uint64_t str_offset_;
  uint64_t str_end_;

  std::vector<Info> symbols_;
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_SYMBOLS_H
