/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef _LIBUNWINDSTACK_DEX_FILE_H
#define _LIBUNWINDSTACK_DEX_FILE_H

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include <dex/dex_file-inl.h>

namespace unwindstack {

class DexFile {
 public:
  DexFile() = default;
  virtual ~DexFile() = default;

  bool GetMethodInformation(uint64_t dex_offset, std::string* method_name, uint64_t* method_offset);

  static DexFile* Create(uint64_t dex_file_offset_in_memory, Memory* memory, MapInfo* info);

 protected:
  std::unique_ptr<const art::DexFile> dex_file_;
};

class DexFileFromFile : public DexFile {
 public:
  DexFileFromFile() = default;
  virtual ~DexFileFromFile();

  bool Open(uint64_t dex_file_offset_in_file, const std::string& name);

 private:
  void* mapped_memory_ = nullptr;
  size_t size_ = 0;
};

class DexFileFromMemory : public DexFile {
 public:
  DexFileFromMemory() = default;
  virtual ~DexFileFromMemory() = default;

  bool Open(uint64_t dex_file_offset_in_memory, Memory* memory);

 private:
  std::vector<uint8_t> memory_;
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_DEX_FILE_H
