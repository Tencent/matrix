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
#include <deps/android-base/include/android-base/macros.h>

#include "ArmExidx.h"
#include "ElfInterfaceArmExidx.h"
#include "../../common/Log.h"
#include "ArmExidxFast.h"

namespace unwindstack {

bool ElfInterfaceArmExidx::Init(int64_t* load_bias) {
  if (!ElfInterface32::Init(load_bias)) {
    return false;
  }
  load_bias_ = *load_bias;
  return true;
}

bool ElfInterfaceArmExidx::FindEntry(uint32_t pc, uint64_t* entry_offset) {
  if (start_offset_ == 0 || total_entries_ == 0) {
    last_error_.code = ERROR_UNWIND_INFO;
    return false;
  }

  size_t first = 0;
  size_t last = total_entries_;
  while (first < last) {
    size_t current = (first + last) / 2;
    uint32_t addr = addrs_array_[current];
    if (addr == 0) {
      if (!GetPrel31Addr(start_offset_ + current * 8, &addr)) {
        return false;
      }
      addrs_array_[current] = addr;
    }
    if (pc == addr) {
      *entry_offset = start_offset_ + current * 8;
//        LOGE("WTF-FUT", "ElfInterfaceArmExidx::FindEntry pc %x, addr %x, current %u", pc, addr, current);
      return true;
    }
    if (pc < addr) {
      last = current;
    } else {
      first = current + 1;
    }
  }
  if (last != 0) {
//      LOGE("WTF-FUT", "ElfInterfaceArmExidx::FindEntry pc %x, addr %x, last %u", pc, addrs_[last - 1], last);
    *entry_offset = start_offset_ + (last - 1) * 8;
    return true;
  }
  last_error_.code = ERROR_UNWIND_INFO;
  return false;
}

bool ElfInterfaceArmExidx::GetPrel31Addr(uint32_t offset, uint32_t* addr) {
//  LOGE("Unwind-test", "GetPrel31Addr 0x%x", offset);
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

void ElfInterfaceArmExidx::HandleUnknownType(uint32_t type, uint64_t ph_offset, uint64_t ph_filesz) {
  if (type != PT_ARM_EXIDX) {
    return;
  }

  // The offset already takes into account the load bias.
  start_offset_ = ph_offset;

  // Always use filesz instead of memsz. In most cases they are the same,
  // but some shared libraries wind up setting one correctly and not the other.
  total_entries_ = ph_filesz / 8;

  size_t capacity = sizeof(uint32_t) * (total_entries_ + 1);
  addrs_array_ = static_cast<uint32_t *>(malloc(capacity));
  memset(addrs_array_, 0, capacity);
}
//
//void ElfInterfaceArmExidx::GenerateFastUnwindTable(Memory *process_memory) {
//
//  if (fut_sections_.get() == nullptr) {
//    fut_sections_.reset(new FutSections());
//  } else {
//      return;
//  }
//  WeChatQuickenTableGenerator futGen(memory_, process_memory);
//
//  bool ret = futGen.GenerateFutSections(start_offset_, total_entries_, fut_sections_.get());
//
////  fut_addrs_ = new uint32_t[fut_sections_->idx_size / 2];
//  if (!ret) {
//    // TODO error
//  }
//}

bool ElfInterfaceArmExidx::Step(uint64_t pc, Regs* regs, Memory* process_memory, bool* finished) {
  return ElfInterface32::Step(pc, regs, process_memory, finished);
}

bool ElfInterfaceArmExidx::StepExidx(uint64_t pc, uintptr_t* regs, Memory* process_memory, bool* finished) {
  // Adjust the load bias to get the real relative pc.
  if (UNLIKELY(pc < load_bias_)) {
    last_error_.code = ERROR_UNWIND_INFO;
    return false;
  }
  pc -= load_bias_;

  uint64_t entry_offset;
  if (UNLIKELY(!FindEntry(pc, &entry_offset))) {
    return false;
  }

//  LOGE("WTF-FUT", "ElfInterfaceArmExidx::StepExidx pc %x, %llu", pc, entry_offset);

//    ArmExidxExtremeTest arm(regs, memory_, process_memory);
    ArmExidxFast arm(regs, memory_, process_memory);
  arm.set_cfa(SP(regs));
  bool return_value = false;
  if (arm.ExtractEntryData(entry_offset) && arm.Eval()) {
    // If the pc was not set, then use the LR registers for the PC.
    if (!arm.pc_set()) {
      PC(regs) = LR(regs);
    }
    SP(regs) = arm.cfa();
    return_value = true;

    // If the pc was set to zero, consider this the final frame.
    *finished = (PC(regs) == 0) ? true : false;
  }

  if (UNLIKELY(arm.status() == ARM_STATUS_NO_UNWIND)) {
    *finished = true;
    return true;
  }

//  if (!return_value) {
//    switch (arm.status()) {
//      case ARM_STATUS_NONE:
//      case ARM_STATUS_NO_UNWIND:
//      case ARM_STATUS_FINISH:
//        last_error_.code = ERROR_NONE;
//        break;
//
//      case ARM_STATUS_RESERVED:
//      case ARM_STATUS_SPARE:
//      case ARM_STATUS_TRUNCATED:
//      case ARM_STATUS_MALFORMED:
//      case ARM_STATUS_INVALID_ALIGNMENT:
//      case ARM_STATUS_INVALID_PERSONALITY:
//        last_error_.code = ERROR_UNWIND_INFO;
//        break;
//
//      case ARM_STATUS_READ_FAILED:
//        last_error_.code = ERROR_MEMORY_INVALID;
//        last_error_.address = arm.status_address();
//        break;
//    }
//  }
  return return_value;
}

//bool ElfInterfaceArmExidx::FutFindEntry(uint32_t pc, uint64_t* entry_offset) {
//
//    size_t first = 0;
//    size_t last = fut_sections_->idx_size;
////    LOGE("WTF-FUT", "ElfInterfaceArmExidx::FutFindEntry fut_sections_->idx_size %u", fut_sections_->idx_size);
//    while (first < last) {
//        size_t current = ((first + last) / 2) & 0xfffffffe;
//        uint32_t addr = fut_sections_->fuidx[current];
////        LOGE("WTF-FUT", "ElfInterfaceArmExidx::FutFindEntry first %u, last %u, current %u, pc %x, addr %x, pc == addr %d", first, last, current, pc, addr, pc == addr);
//        if (pc == addr) {
////            LOGE("WTF-FUT", "ElfInterfaceArmExidx::FutFindEntry pc %x, addr %x, pc == addr", pc, addr);
////            *entry_offset = fut_sections_->start_offset_ + current * 4;
//            *entry_offset = current;
////            LOGE("WTF-FUT", "ElfInterfaceArmExidx::FutFindEntry pc %x, addr %x, current %u", pc, addr, current);
//            return true;
//        }
//        if (pc < addr) {
//            last = current;
//        } else {
//            first = current + 2;
//        }
//    }
//    if (last != 0) {
////        LOGE("WTF-FUT", "ElfInterfaceArmExidx::FutFindEntry pc %x, addr %x, %x, last %u", pc, fut_sections_->fuidx[last - 2], fut_sections_->fuidx[last], last);
////        *entry_offset = fut_sections_->start_offset_ + (last - 2) * 4;
//        *entry_offset = last - 2;
//        return true;
//    }
//    last_error_.code = ERROR_UNWIND_INFO;
//    return false;
//}
//
//bool ElfInterfaceArmExidx::StepFut(uint64_t pc, uintptr_t* regs, Memory* process_memory, bool* finished) {
//
//    // Adjust the load bias to get the real relative pc.
//    if (UNLIKELY(pc < load_bias_)) {
//        last_error_.code = ERROR_UNWIND_INFO;
//        return false;
//    }
//    pc -= load_bias_;
//
//    if (!fut_sections_) {
//        GenerateFastUnwindTable(process_memory);
//    }
//
//    QuickenTable arm(fut_sections_.get(), regs, memory_, process_memory);
//    uint64_t entry_offset;
//    if (UNLIKELY(!FutFindEntry(pc, &entry_offset))) {
//        return false;
//    }
//
//    arm.cfa_ = SP(regs);
//    bool return_value = false;
//
//    if (arm.Eval(entry_offset)) {
//        if (!arm.pc_set) {
//            PC(regs) = LR(regs);
//        }
//        SP(regs) = arm.cfa_;
//        return_value = true;
//    }
//
//    // If the pc was set to zero, consider this the final frame.
//    *finished = (PC(regs) == 0) ? true : false;
//
//    return return_value;
//}

bool ElfInterfaceArmExidx::GetFunctionName(uint64_t addr, std::string* name, uint64_t* offset) {
  // For ARM, thumb function symbols have bit 0 set, but the address passed
  // in here might not have this bit set and result in a failure to find
  // the thumb function names. Adjust the address and offset to account
  // for this possible case.
  if (ElfInterface32::GetFunctionName(addr | 1, name, offset)) {
    *offset &= ~1;
    return true;
  }
  return false;
}

}  // namespace unwindstack
