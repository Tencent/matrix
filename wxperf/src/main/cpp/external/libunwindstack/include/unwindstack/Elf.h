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

#ifndef _LIBUNWINDSTACK_ELF_H
#define _LIBUNWINDSTACK_ELF_H

#include <stddef.h>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include <unwindstack/ElfInterface.h>
#include <unwindstack/Memory.h>

#if !defined(EM_AARCH64)
#define EM_AARCH64 183
#endif

namespace unwindstack {

// Forward declaration.
struct MapInfo;
class Regs;

enum ArchEnum : uint8_t {
  ARCH_UNKNOWN = 0,
  ARCH_ARM,
  ARCH_ARM64,
  ARCH_X86,
  ARCH_X86_64,
  ARCH_MIPS,
  ARCH_MIPS64,
};

class Elf {
 public:
  Elf(Memory* memory) : memory_(memory) {}
  virtual ~Elf() = default;

  bool Init();

  void InitGnuDebugdata();

  void Invalidate();

  std::string GetSoname();

  bool GetFunctionName(uint64_t addr, std::string* name, uint64_t* func_offset);

  bool GetGlobalVariableOffset(const std::string& name, uint64_t* memory_offset);

  uint64_t GetRelPc(uint64_t pc, const MapInfo* map_info);

  bool StepIfSignalHandler(uint64_t rel_pc, Regs* regs, Memory* process_memory);

  bool Step(uint64_t rel_pc, Regs* regs, Memory* process_memory, bool* finished);

  ElfInterface* CreateInterfaceFromMemory(Memory* memory);

  std::string GetBuildID();

  int64_t GetLoadBias() { return load_bias_; }

  bool IsValidPc(uint64_t pc);

  void GetLastError(ErrorData* data);
  ErrorCode GetLastErrorCode();
  uint64_t GetLastErrorAddress();

  bool valid() { return valid_; }

  uint32_t machine_type() { return machine_type_; }

  uint8_t class_type() { return class_type_; }

  ArchEnum arch() { return arch_; }

  Memory* memory() { return memory_.get(); }

  ElfInterface* interface() { return interface_.get(); }

  ElfInterface* gnu_debugdata_interface() { return gnu_debugdata_interface_.get(); }

  static bool IsValidElf(Memory* memory);

  static bool GetInfo(Memory* memory, uint64_t* size);

  static int64_t GetLoadBias(Memory* memory);

  static std::string GetBuildID(Memory* memory);

  static void SetCachingEnabled(bool enable);
  static bool CachingEnabled() { return cache_enabled_; }

  static void CacheLock();
  static void CacheUnlock();
  static void CacheAdd(MapInfo* info);
  static bool CacheGet(MapInfo* info);
  static bool CacheAfterCreateMemory(MapInfo* info);

 protected:
  bool valid_ = false;
  int64_t load_bias_ = 0;
  std::unique_ptr<ElfInterface> interface_;
  std::unique_ptr<Memory> memory_;
  uint32_t machine_type_;
  uint8_t class_type_;
  ArchEnum arch_;
  // Protect calls that can modify internal state of the interface object.
  std::mutex lock_;

  std::unique_ptr<Memory> gnu_debugdata_memory_;
  std::unique_ptr<ElfInterface> gnu_debugdata_interface_;

  static bool cache_enabled_;
  static std::unordered_map<std::string, std::pair<std::shared_ptr<Elf>, bool>>* cache_;
  static std::mutex* cache_lock_;
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_ELF_H
