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

#ifndef _LIBUNWINDSTACK_ELF_INTERFACE_ARM_EXIDX_H
#define _LIBUNWINDSTACK_ELF_INTERFACE_ARM_EXIDX_H

#include <elf.h>
#include <stdint.h>

#include <iterator>
#include <unordered_map>

#include <unwindstack/ElfInterface.h>
#include <unwindstack/Memory.h>
#include <memory>
#include "QuickenTableGenerator.h"
#include "ElfInterfaceArm.h"

namespace unwindstack {

class ElfInterfaceArmExidx : public ElfInterfaceArm {
 public:
  ElfInterfaceArmExidx(Memory* memory) : ElfInterfaceArm(memory) {}
  virtual ~ElfInterfaceArmExidx() = default;

  bool Init(int64_t* section_bias) override;

  bool GetPrel31Addr(uint32_t offset, uint32_t* addr);

  bool FindEntry(uint32_t pc, uint64_t* entry_offset);

  void HandleUnknownType(uint32_t type, uint64_t ph_offset, uint64_t ph_filesz) override;

  bool Step(uint64_t pc, Regs* regs, Memory* process_memory, bool* finished) override;

  bool StepExidx(uint64_t pc, uintptr_t* regs, Memory* process_memory, bool* finished);

  bool GetFunctionName(uint64_t addr, std::string* name, uint64_t* offset) override;

//  bool FutFindEntry(uint32_t pc, uint64_t* entry_offset);
//  bool StepFut(uint64_t pc, uintptr_t* regs, Memory* process_memory, bool* finished);
//  void GenerateFastUnwindTable(const std::shared_ptr<Memory>& process_memory);
//  void GenerateFastUnwindTable(Memory *process_memory);

  uint64_t start_offset() { return start_offset_; }

  size_t total_entries() { return total_entries_; }

  void set_load_bias(uint64_t load_bias) { load_bias_ = load_bias; }

 protected:
  uint64_t start_offset_ = 0;
  size_t total_entries_ = 0;
  uint64_t load_bias_ = 0;

  // TODO destruct
  uint32_t * addrs_array_ = nullptr;
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_ELF_INTERFACE_ARM_EXIDX_H
