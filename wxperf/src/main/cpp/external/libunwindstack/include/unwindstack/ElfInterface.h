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

#ifndef _LIBUNWINDSTACK_ELF_INTERFACE_H
#define _LIBUNWINDSTACK_ELF_INTERFACE_H

#include <elf.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <unwindstack/DwarfSection.h>
#include <unwindstack/Error.h>

namespace unwindstack {

// Forward declarations.
class Memory;
class Regs;
class Symbols;

struct LoadInfo {
  uint64_t offset;
  uint64_t table_offset;
  size_t table_size;
};

enum : uint8_t {
  SONAME_UNKNOWN = 0,
  SONAME_VALID,
  SONAME_INVALID,
};

class ElfInterface {
 public:
  ElfInterface(Memory* memory) : memory_(memory) {}
  virtual ~ElfInterface();

  virtual bool Init(int64_t* load_bias) = 0;

  virtual void InitHeaders() = 0;

  virtual std::string GetSoname() = 0;

  virtual bool GetFunctionName(uint64_t addr, std::string* name, uint64_t* offset) = 0;

  virtual bool GetGlobalVariable(const std::string& name, uint64_t* memory_address) = 0;

  virtual std::string GetBuildID() = 0;

  virtual bool Step(uint64_t rel_pc, Regs* regs, Memory* process_memory, bool* finished);

  virtual bool IsValidPc(uint64_t pc);

  Memory* CreateGnuDebugdataMemory();

  Memory* memory() { return memory_; }

  const std::unordered_map<uint64_t, LoadInfo>& pt_loads() { return pt_loads_; }

  void SetGnuDebugdataInterface(ElfInterface* interface) { gnu_debugdata_interface_ = interface; }

  uint64_t dynamic_offset() { return dynamic_offset_; }
  uint64_t dynamic_vaddr_start() { return dynamic_vaddr_start_; }
  uint64_t dynamic_vaddr_end() { return dynamic_vaddr_end_; }
  uint64_t data_offset() { return data_offset_; }
  uint64_t data_vaddr_start() { return data_vaddr_start_; }
  uint64_t data_vaddr_end() { return data_vaddr_end_; }
  uint64_t eh_frame_hdr_offset() { return eh_frame_hdr_offset_; }
  int64_t eh_frame_hdr_section_bias() { return eh_frame_hdr_section_bias_; }
  uint64_t eh_frame_hdr_size() { return eh_frame_hdr_size_; }
  uint64_t eh_frame_offset() { return eh_frame_offset_; }
  int64_t eh_frame_section_bias() { return eh_frame_section_bias_; }
  uint64_t eh_frame_size() { return eh_frame_size_; }
  uint64_t debug_frame_offset() { return debug_frame_offset_; }
  int64_t debug_frame_section_bias() { return debug_frame_section_bias_; }
  uint64_t debug_frame_size() { return debug_frame_size_; }
  uint64_t gnu_debugdata_offset() { return gnu_debugdata_offset_; }
  uint64_t gnu_debugdata_size() { return gnu_debugdata_size_; }
  uint64_t gnu_build_id_offset() { return gnu_build_id_offset_; }
  uint64_t gnu_build_id_size() { return gnu_build_id_size_; }

  DwarfSection* eh_frame() { return eh_frame_.get(); }
  DwarfSection* debug_frame() { return debug_frame_.get(); }

  const ErrorData& last_error() { return last_error_; }
  ErrorCode LastErrorCode() { return last_error_.code; }
  uint64_t LastErrorAddress() { return last_error_.address; }

  template <typename EhdrType, typename PhdrType>
  static int64_t GetLoadBias(Memory* memory);

  template <typename EhdrType, typename ShdrType, typename NhdrType>
  static std::string ReadBuildIDFromMemory(Memory* memory);

 protected:
  template <typename AddressType>
  void InitHeadersWithTemplate();

  template <typename EhdrType, typename PhdrType, typename ShdrType>
  bool ReadAllHeaders(int64_t* load_bias);

  template <typename EhdrType, typename PhdrType>
  void ReadProgramHeaders(const EhdrType& ehdr, int64_t* load_bias);

  template <typename EhdrType, typename ShdrType>
  void ReadSectionHeaders(const EhdrType& ehdr);

  template <typename DynType>
  std::string GetSonameWithTemplate();

  template <typename SymType>
  bool GetFunctionNameWithTemplate(uint64_t addr, std::string* name, uint64_t* func_offset);

  template <typename SymType>
  bool GetGlobalVariableWithTemplate(const std::string& name, uint64_t* memory_address);

  virtual void HandleUnknownType(uint32_t, uint64_t, uint64_t) {}

  template <typename EhdrType>
  static void GetMaxSizeWithTemplate(Memory* memory, uint64_t* size);

  template <typename NhdrType>
  std::string ReadBuildID();

  Memory* memory_;
  std::unordered_map<uint64_t, LoadInfo> pt_loads_;

  // Stored elf data.
  uint64_t dynamic_offset_ = 0;
  uint64_t dynamic_vaddr_start_ = 0;
  uint64_t dynamic_vaddr_end_ = 0;

  uint64_t data_offset_ = 0;
  uint64_t data_vaddr_start_ = 0;
  uint64_t data_vaddr_end_ = 0;

  uint64_t eh_frame_hdr_offset_ = 0;
  int64_t eh_frame_hdr_section_bias_ = 0;
  uint64_t eh_frame_hdr_size_ = 0;

  uint64_t eh_frame_offset_ = 0;
  int64_t eh_frame_section_bias_ = 0;
  uint64_t eh_frame_size_ = 0;

  uint64_t debug_frame_offset_ = 0;
  int64_t debug_frame_section_bias_ = 0;
  uint64_t debug_frame_size_ = 0;

  uint64_t gnu_debugdata_offset_ = 0;
  uint64_t gnu_debugdata_size_ = 0;

  uint64_t gnu_build_id_offset_ = 0;
  uint64_t gnu_build_id_size_ = 0;

  uint8_t soname_type_ = SONAME_UNKNOWN;
  std::string soname_;

  ErrorData last_error_{ERROR_NONE, 0};

  std::unique_ptr<DwarfSection> eh_frame_;
  std::unique_ptr<DwarfSection> debug_frame_;
  // The Elf object owns the gnu_debugdata interface object.
  ElfInterface* gnu_debugdata_interface_ = nullptr;

  std::vector<Symbols*> symbols_;
  std::vector<std::pair<uint64_t, uint64_t>> strtabs_;
};

class ElfInterface32 : public ElfInterface {
 public:
  ElfInterface32(Memory* memory) : ElfInterface(memory) {}
  virtual ~ElfInterface32() = default;

  bool Init(int64_t* load_bias) override {
    return ElfInterface::ReadAllHeaders<Elf32_Ehdr, Elf32_Phdr, Elf32_Shdr>(load_bias);
  }

  void InitHeaders() override { ElfInterface::InitHeadersWithTemplate<uint32_t>(); }

  std::string GetSoname() override { return ElfInterface::GetSonameWithTemplate<Elf32_Dyn>(); }

  bool GetFunctionName(uint64_t addr, std::string* name, uint64_t* func_offset) override {
    return ElfInterface::GetFunctionNameWithTemplate<Elf32_Sym>(addr, name, func_offset);
  }

  bool GetGlobalVariable(const std::string& name, uint64_t* memory_address) override {
    return ElfInterface::GetGlobalVariableWithTemplate<Elf32_Sym>(name, memory_address);
  }

  std::string GetBuildID() override { return ElfInterface::ReadBuildID<Elf32_Nhdr>(); }

  static void GetMaxSize(Memory* memory, uint64_t* size) {
    GetMaxSizeWithTemplate<Elf32_Ehdr>(memory, size);
  }
};

class ElfInterface64 : public ElfInterface {
 public:
  ElfInterface64(Memory* memory) : ElfInterface(memory) {}
  virtual ~ElfInterface64() = default;

  bool Init(int64_t* load_bias) override {
    return ElfInterface::ReadAllHeaders<Elf64_Ehdr, Elf64_Phdr, Elf64_Shdr>(load_bias);
  }

  void InitHeaders() override { ElfInterface::InitHeadersWithTemplate<uint64_t>(); }

  std::string GetSoname() override { return ElfInterface::GetSonameWithTemplate<Elf64_Dyn>(); }

  bool GetFunctionName(uint64_t addr, std::string* name, uint64_t* func_offset) override {
    return ElfInterface::GetFunctionNameWithTemplate<Elf64_Sym>(addr, name, func_offset);
  }

  bool GetGlobalVariable(const std::string& name, uint64_t* memory_address) override {
    return ElfInterface::GetGlobalVariableWithTemplate<Elf64_Sym>(name, memory_address);
  }

  std::string GetBuildID() override { return ElfInterface::ReadBuildID<Elf64_Nhdr>(); }

  static void GetMaxSize(Memory* memory, uint64_t* size) {
    GetMaxSizeWithTemplate<Elf64_Ehdr>(memory, size);
  }
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_ELF_INTERFACE_H
