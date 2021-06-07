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

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "MemoryFake.h"

namespace unwindstack {

void MemoryFake::SetMemoryBlock(uint64_t addr, size_t length, uint8_t value) {
  for (size_t i = 0; i < length; i++, addr++) {
    auto entry = data_.find(addr);
    if (entry != data_.end()) {
      entry->second = value;
    } else {
      data_.insert({addr, value});
    }
  }
}

void MemoryFake::SetMemory(uint64_t addr, const void* memory, size_t length) {
  const uint8_t* src = reinterpret_cast<const uint8_t*>(memory);
  for (size_t i = 0; i < length; i++, addr++) {
    auto value = data_.find(addr);
    if (value != data_.end()) {
      value->second = src[i];
    } else {
      data_.insert({ addr, src[i] });
    }
  }
}

size_t MemoryFake::Read(uint64_t addr, void* memory, size_t size) {
  uint8_t* dst = reinterpret_cast<uint8_t*>(memory);
  for (size_t i = 0; i < size; i++, addr++) {
    auto value = data_.find(addr);
    if (value == data_.end()) {
      return i;
    }
    dst[i] = value->second;
  }
  return size;
}

}  // namespace unwindstack
