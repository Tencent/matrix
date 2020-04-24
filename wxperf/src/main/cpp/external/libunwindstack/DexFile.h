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

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <art_api/dex_file_support.h>

namespace unwindstack {

class DexFile : protected art_api::dex::DexFile {
 public:
  virtual ~DexFile() = default;

  bool GetMethodInformation(uint64_t dex_offset, std::string* method_name, uint64_t* method_offset);

  static std::unique_ptr<DexFile> Create(uint64_t dex_file_offset_in_memory, Memory* memory,
                                         MapInfo* info);

 protected:
  DexFile(art_api::dex::DexFile&& art_dex_file) : art_api::dex::DexFile(std::move(art_dex_file)) {}
};

class DexFileFromFile : public DexFile {
 public:
  static std::unique_ptr<DexFileFromFile> Create(uint64_t dex_file_offset_in_file,
                                                 const std::string& file);

 private:
  DexFileFromFile(art_api::dex::DexFile&& art_dex_file) : DexFile(std::move(art_dex_file)) {}
};

class DexFileFromMemory : public DexFile {
 public:
  static std::unique_ptr<DexFileFromMemory> Create(uint64_t dex_file_offset_in_memory,
                                                   Memory* memory, const std::string& name);

 private:
  DexFileFromMemory(art_api::dex::DexFile&& art_dex_file, std::vector<uint8_t>&& memory)
      : DexFile(std::move(art_dex_file)), memory_(std::move(memory)) {}

  std::vector<uint8_t> memory_;
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_DEX_FILE_H
