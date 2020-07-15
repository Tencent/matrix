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

#ifndef _LIBUNWINDSTACK_MEMORY_H
#define _LIBUNWINDSTACK_MEMORY_H

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>

namespace unwindstack {

class Memory {
 public:
  Memory() = default;
  virtual ~Memory() = default;

  static std::shared_ptr<Memory> CreateProcessMemory(pid_t pid);
  static std::shared_ptr<Memory> CreateProcessMemoryCached(pid_t pid);
  static std::shared_ptr<Memory> CreateOfflineMemory(const uint8_t* data, uint64_t start,
                                                     uint64_t end);
  static std::unique_ptr<Memory> CreateFileMemory(const std::string& path, uint64_t offset);

  virtual bool ReadString(uint64_t addr, std::string* dst, size_t max_read);

  virtual void Clear() {}

  virtual bool IsLocal() const { return false; }

  virtual size_t Read(uint64_t addr, void* dst, size_t size) = 0;
  virtual long ReadTag(uint64_t) { return -1; }

  bool ReadFully(uint64_t addr, void* dst, size_t size);

  inline bool Read32(uint64_t addr, uint32_t* dst) {
    return ReadFully(addr, dst, sizeof(uint32_t));
  }

  inline bool Read64(uint64_t addr, uint64_t* dst) {
    return ReadFully(addr, dst, sizeof(uint64_t));
  }
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_MEMORY_H
