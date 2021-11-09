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

#ifndef _LIBUNWINDSTACK_DEX_FILES_H
#define _LIBUNWINDSTACK_DEX_FILES_H

#include <stdint.h>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <unwindstack/Global.h>
#include <unwindstack/Memory.h>

namespace unwindstack {

// Forward declarations.
class DexFile;
class Maps;
struct MapInfo;
enum ArchEnum : uint8_t;

class DexFiles : public Global {
 public:
  explicit DexFiles(std::shared_ptr<Memory>& memory);
  DexFiles(std::shared_ptr<Memory>& memory, std::vector<std::string>& search_libs);
  virtual ~DexFiles();

  DexFile* GetDexFile(uint64_t dex_file_offset, MapInfo* info);

  void GetMethodInformation(Maps* maps, MapInfo* info, uint64_t dex_pc, std::string* method_name,
                            uint64_t* method_offset);

 private:
  void Init(Maps* maps);

  bool GetAddr(size_t index, uint64_t* addr);

  uint64_t ReadEntryPtr32(uint64_t addr);

  uint64_t ReadEntryPtr64(uint64_t addr);

  bool ReadEntry32();

  bool ReadEntry64();

  bool ReadVariableData(uint64_t ptr_offset) override;

  void ProcessArch() override;

  std::mutex lock_;
  bool initialized_ = false;
  std::unordered_map<uint64_t, std::unique_ptr<DexFile>> files_;

  uint64_t entry_addr_ = 0;
  uint64_t (DexFiles::*read_entry_ptr_func_)(uint64_t) = nullptr;
  bool (DexFiles::*read_entry_func_)() = nullptr;
  std::vector<uint64_t> addrs_;
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_DEX_FILES_H
