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

#include <elf.h>
#include <stdint.h>

#include <unwindstack/MachineArm.h>
#include <unwindstack/Memory.h>
#include <unwindstack/RegsArm.h>

#include "ArmExidx.h"
#include "ElfInterfaceArm.h"

namespace unwindstack {

bool ElfInterfaceArm::FindEntry(uint32_t pc, uint64_t* entry_offset) {
  if (start_offset_ == 0 || total_entries_ == 0) {
    last_error_.code = ERROR_UNWIND_INFO;
    return false;
  }

  size_t first = 0;
  size_t last = total_entries_;
  while (first < last) {
    size_t current = (first + last) / 2;
    uint32_t addr = addrs_[current];
    if (addr == 0) {
      if (!GetPrel31Addr(start_offset_ + current * 8, &addr)) {
        return false;
      }
      addrs_[current] = addr;
    }
    if (pc == addr) {
      *entry_offset = start_offset_ + current * 8;
      return true;
    }
    if (pc < addr) {
      last = current;
    } else {
      first = current + 1;
    }
  }
  if (last != 0) {
    *entry_offset = start_offset_ + (last - 1) * 8;
    return true;
  }
  last_error_.code = ERROR_UNWIND_INFO;
  return false;
}

bool ElfInterfaceArm::GetPrel31Addr(uint32_t offset, uint32_t* addr) {
  uint32_t data;
  if (!memory_->Read32(offset, &data)) {
    last_error_.code = ERROR_MEMORY_INVALID;
    last_error_.address = offset;
    return false;
  }

  // Sign extend the value if necessary.
  int32_t value = (static_cast<int32_t>(data) << 1) >> 1;
  *addr = offset + value;
  return true;
}

#if !defined(PT_ARM_EXIDX)
#define PT_ARM_EXIDX 0x70000001
#endif

bool ElfInterfaceArm::HandleType(uint64_t offset, uint32_t type, uint64_t load_bias) {
  if (type != PT_ARM_EXIDX) {
    return false;
  }

  Elf32_Phdr phdr;
  if (!memory_->ReadField(offset, &phdr, &phdr.p_vaddr, sizeof(phdr.p_vaddr))) {
    return true;
  }
  if (!memory_->ReadField(offset, &phdr, &phdr.p_memsz, sizeof(phdr.p_memsz))) {
    return true;
  }
  start_offset_ = phdr.p_vaddr - load_bias;
  total_entries_ = phdr.p_memsz / 8;
  return true;
}

bool ElfInterfaceArm::Step(uint64_t pc, uint64_t load_bias, Regs* regs, Memory* process_memory,
                           bool* finished) {
  // Dwarf unwind information is precise about whether a pc is covered or not,
  // but arm unwind information only has ranges of pc. In order to avoid
  // incorrectly doing a bad unwind using arm unwind information for a
  // different function, always try and unwind with the dwarf information first.
  return ElfInterface32::Step(pc, load_bias, regs, process_memory, finished) ||
         StepExidx(pc, load_bias, regs, process_memory, finished);
}

bool ElfInterfaceArm::StepExidx(uint64_t pc, uint64_t load_bias, Regs* regs, Memory* process_memory,
                                bool* finished) {
  // Adjust the load bias to get the real relative pc.
  if (pc < load_bias) {
    last_error_.code = ERROR_UNWIND_INFO;
    return false;
  }
  pc -= load_bias;

  RegsArm* regs_arm = reinterpret_cast<RegsArm*>(regs);
  uint64_t entry_offset;
  if (!FindEntry(pc, &entry_offset)) {
    return false;
  }

  ArmExidx arm(regs_arm, memory_, process_memory);
  arm.set_cfa(regs_arm->sp());
  bool return_value = false;
  if (arm.ExtractEntryData(entry_offset) && arm.Eval()) {
    // If the pc was not set, then use the LR registers for the PC.
    if (!arm.pc_set()) {
      (*regs_arm)[ARM_REG_PC] = (*regs_arm)[ARM_REG_LR];
    }
    (*regs_arm)[ARM_REG_SP] = arm.cfa();
    return_value = true;

    // If the pc was set to zero, consider this the final frame.
    *finished = (regs_arm->pc() == 0) ? true : false;
  }

  if (arm.status() == ARM_STATUS_NO_UNWIND) {
    *finished = true;
    return true;
  }

  if (!return_value) {
    switch (arm.status()) {
      case ARM_STATUS_NONE:
      case ARM_STATUS_NO_UNWIND:
      case ARM_STATUS_FINISH:
        last_error_.code = ERROR_NONE;
        break;

      case ARM_STATUS_RESERVED:
      case ARM_STATUS_SPARE:
      case ARM_STATUS_TRUNCATED:
      case ARM_STATUS_MALFORMED:
      case ARM_STATUS_INVALID_ALIGNMENT:
      case ARM_STATUS_INVALID_PERSONALITY:
        last_error_.code = ERROR_UNWIND_INFO;
        break;

      case ARM_STATUS_READ_FAILED:
        last_error_.code = ERROR_MEMORY_INVALID;
        last_error_.address = arm.status_address();
        break;
    }
  }
  return return_value;
}

bool ElfInterfaceArm::GetFunctionName(uint64_t addr, uint64_t load_bias, std::string* name,
                                      uint64_t* offset) {
  // For ARM, thumb function symbols have bit 0 set, but the address passed
  // in here might not have this bit set and result in a failure to find
  // the thumb function names. Adjust the address and offset to account
  // for this possible case.
  if (ElfInterface32::GetFunctionName(addr | 1, load_bias, name, offset)) {
    *offset &= ~1;
    return true;
  }
  return false;
}

}  // namespace unwindstack
