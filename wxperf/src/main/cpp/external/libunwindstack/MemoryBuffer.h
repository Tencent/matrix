/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef _LIBUNWINDSTACK_MEMORY_BUFFER_H
#define _LIBUNWINDSTACK_MEMORY_BUFFER_H

#include <stdint.h>

#include <string>
#include <vector>

#include <unwindstack/Memory.h>

namespace unwindstack {

class MemoryBuffer : public Memory {
 public:
  MemoryBuffer() = default;
  virtual ~MemoryBuffer() { free(raw_); }

  size_t Read(uint64_t addr, void* dst, size_t size) override;

  uint8_t* GetPtr(size_t offset);

  bool Resize(size_t size) {
    raw_ = reinterpret_cast<uint8_t*>(realloc(raw_, size));
    if (raw_ == nullptr) {
      size_ = 0;
      return false;
    }
    size_ = size;
    return true;
  }

  uint64_t Size() { return size_; }

 private:
  uint8_t* raw_ = nullptr;
  size_t size_ = 0;
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_MEMORY_BUFFER_H
