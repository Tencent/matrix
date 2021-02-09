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

#ifndef _LIBUNWINDSTACK_TESTS_MEMORY_FAKE_H
#define _LIBUNWINDSTACK_TESTS_MEMORY_FAKE_H

#include <stdint.h>

#include <string>
#include <vector>
#include <unordered_map>

#include <unwindstack/Memory.h>

namespace unwindstack {

class MemoryFake : public Memory {
 public:
  MemoryFake() = default;
  virtual ~MemoryFake() = default;

  size_t Read(uint64_t addr, void* buffer, size_t size) override;

  void SetMemory(uint64_t addr, const void* memory, size_t length);

  void SetMemoryBlock(uint64_t addr, size_t length, uint8_t value);

  void SetData8(uint64_t addr, uint8_t value) {
    SetMemory(addr, &value, sizeof(value));
  }

  void SetData16(uint64_t addr, uint16_t value) {
    SetMemory(addr, &value, sizeof(value));
  }

  void SetData32(uint64_t addr, uint32_t value) {
    SetMemory(addr, &value, sizeof(value));
  }

  void SetData64(uint64_t addr, uint64_t value) {
    SetMemory(addr, &value, sizeof(value));
  }

  void SetMemory(uint64_t addr, std::vector<uint8_t> values) {
    SetMemory(addr, values.data(), values.size());
  }

  void SetMemory(uint64_t addr, std::string string) {
    SetMemory(addr, string.c_str(), string.size() + 1);
  }

  void Clear() { data_.clear(); }

 private:
  std::unordered_map<uint64_t, uint8_t> data_;
};

class MemoryFakeAlwaysReadZero : public Memory {
 public:
  MemoryFakeAlwaysReadZero() = default;
  virtual ~MemoryFakeAlwaysReadZero() = default;

  size_t Read(uint64_t, void* buffer, size_t size) override {
    memset(buffer, 0, size);
    return size;
  }
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_TESTS_MEMORY_FAKE_H
