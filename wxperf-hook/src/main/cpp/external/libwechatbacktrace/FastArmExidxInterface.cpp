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
#include "FastArmExidxInterface.h"
#include "../../common/Log.h"
#include "FastArmExidx.h"

namespace unwindstack {

bool FastArmExidxInterface::GetPrel31Addr(uint32_t offset, uint32_t* addr) {
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

bool FastArmExidxInterface::FindEntry(uint32_t pc, uint64_t* entry_offset) {
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

bool FastArmExidxInterface::Step(uint64_t pc, uint32_t* regs, Memory* process_memory, bool* finished) {
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

  FastArmExidx arm(regs, memory_, process_memory);
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

}  // namespace unwindstack
