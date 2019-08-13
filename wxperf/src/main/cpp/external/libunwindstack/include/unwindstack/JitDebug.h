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

#ifndef _LIBUNWINDSTACK_JIT_DEBUG_H
#define _LIBUNWINDSTACK_JIT_DEBUG_H

#include <stdint.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace unwindstack {

// Forward declarations.
class Elf;
class Maps;
class Memory;
enum ArchEnum : uint8_t;

class JitDebug {
 public:
  explicit JitDebug(std::shared_ptr<Memory>& memory);
  JitDebug(std::shared_ptr<Memory>& memory, std::vector<std::string>& search_libs);
  ~JitDebug();

  Elf* GetElf(Maps* maps, uint64_t pc);

  void SetArch(ArchEnum arch);

 private:
  void Init(Maps* maps);

  std::shared_ptr<Memory> memory_;
  uint64_t entry_addr_ = 0;
  bool initialized_ = false;
  std::vector<Elf*> elf_list_;
  std::vector<std::string> search_libs_;

  std::mutex lock_;

  uint64_t (JitDebug::*read_descriptor_func_)(uint64_t) = nullptr;
  uint64_t (JitDebug::*read_entry_func_)(uint64_t*, uint64_t*) = nullptr;

  uint64_t ReadDescriptor32(uint64_t);
  uint64_t ReadDescriptor64(uint64_t);

  uint64_t ReadEntry32Pack(uint64_t* start, uint64_t* size);
  uint64_t ReadEntry32Pad(uint64_t* start, uint64_t* size);
  uint64_t ReadEntry64(uint64_t* start, uint64_t* size);
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_JIT_DEBUG_H
