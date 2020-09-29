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

#include <stdint.h>

#include <deque>
#include <string>

#include <android-base/stringprintf.h>

#include <unwindstack/Log.h>
#include <unwindstack/MachineArm.h>
#include <unwindstack/Memory.h>
#include <unwindstack/RegsArm.h>
#include <deps/log/log.h>
#include <deps/android-base/include/android-base/macros.h>

#include "FastArmExidx.h"
#include "Check.h"
#include "../../common/Log.h"
#include "ArmExidx.h"

namespace unwindstack {
    uint8_t * temp_data_array = static_cast<uint8_t *>(malloc(32 * sizeof(uint8_t)));

bool FastArmExidx::ExtractEntryData(uint32_t entry_offset) {
//  data_.clear();
    if (data_array != nullptr) {
        data_index = 0;
        data_read_index = 0;
    } else {
        data_array = temp_data_array;
    }
  status_ = ARM_STATUS_NONE;

  if (entry_offset & 1) {
    // The offset needs to be at least two byte aligned.
    status_ = ARM_STATUS_INVALID_ALIGNMENT;
    return false;
  }

  // Each entry is a 32 bit prel31 offset followed by 32 bits
  // of unwind information. If bit 31 of the unwind data is zero,
  // then this is a prel31 offset to the start of the unwind data.
  // If the unwind data is 1, then this is a cant unwind entry.
  // Otherwise, this data is the compact form of the unwind information.
  uint32_t data;
  if (!elf_memory_->Read32(entry_offset + 4, &data)) {
    status_ = ARM_STATUS_READ_FAILED;
    status_address_ = entry_offset + 4;
    return false;
  }

  if (data == 1) {
    // This is a CANT UNWIND entry.
    status_ = ARM_STATUS_NO_UNWIND;
    return false;
  }

  if (data & (1UL << 31)) {
    // This is a compact table entry.
    if ((data >> 24) & 0xf) {
      // This is a non-zero index, this code doesn't support
      // other formats.
      status_ = ARM_STATUS_INVALID_PERSONALITY;
      return false;
    }
//    data_.push_back((data >> 16) & 0xff);
//    data_.push_back((data >> 8) & 0xff);
      data_array[data_index++] = (data >> 16) & 0xff;
      data_array[data_index++] = (data >> 8) & 0xff;
    uint8_t last_op = data & 0xff;
//    data_.push_back(last_op);
      data_array[data_index++] = last_op;
    if (last_op != ARM_OP_FINISH) {
      // If this didn't end with a finish op, add one.
//      data_.push_back(ARM_OP_FINISH);
        data_array[data_index++] = ARM_OP_FINISH;
    }
    return true;
  }

  // Get the address of the ops.
  // Sign extend the data value if necessary.
  int32_t signed_data = static_cast<int32_t>(data << 1) >> 1;
  uint32_t addr = (entry_offset + 4) + signed_data;
  if (!elf_memory_->Read32(addr, &data)) {
    status_ = ARM_STATUS_READ_FAILED;
    status_address_ = addr;
    return false;
  }

  size_t num_table_words;
  if (data & (1UL << 31)) {

    // Compact model.
    switch ((data >> 24) & 0xf) {
    case 0:
      num_table_words = 0;
//      data_.push_back((data >> 16) & 0xff);
            data_array[data_index++] = (data >> 16) & 0xff;
      break;
    case 1:
    case 2:
      num_table_words = (data >> 16) & 0xff;
      addr += 4;
      break;
    default:
      // Only a personality of 0, 1, 2 is valid.
      status_ = ARM_STATUS_INVALID_PERSONALITY;

      return false;
    }
//    data_.push_back((data >> 8) & 0xff);
      data_array[data_index++] = (data >> 8) & 0xff;
//    data_.push_back(data & 0xff);
      data_array[data_index++] = data & 0xff;
  } else {
    // Generic model.

    // Skip the personality routine data, it doesn't contain any data
    // needed to decode the unwind information.
    addr += 4;
    if (!elf_memory_->Read32(addr, &data)) {
      status_ = ARM_STATUS_READ_FAILED;
      status_address_ = addr;
      return false;
    }

    num_table_words = (data >> 24) & 0xff;
//    data_.push_back((data >> 16) & 0xff);
      data_array[data_index++] = (data >> 16) & 0xff;
//    data_.push_back((data >> 8) & 0xff);
      data_array[data_index++] = (data >> 8) & 0xff;
//    data_.push_back(data & 0xff);
      data_array[data_index++] = data & 0xff;
    addr += 4;
  }

  if (num_table_words > 5) {
    status_ = ARM_STATUS_MALFORMED;
    return false;
  }

  for (size_t i = 0; i < num_table_words; i++) {
    if (!elf_memory_->Read32(addr, &data)) {
      status_ = ARM_STATUS_READ_FAILED;
      status_address_ = addr;
      return false;
    }

//    data_.push_back((data >> 24) & 0xff);
      data_array[data_index++] = (data >> 24) & 0xff;
//    data_.push_back((data >> 16) & 0xff);
      data_array[data_index++] = (data >> 16) & 0xff;
//    data_.push_back((data >> 8) & 0xff);
      data_array[data_index++] = (data >> 8) & 0xff;
//    data_.push_back(data & 0xff);x
      data_array[data_index++] = data & 0xff;
    addr += 4;
  }

  if (data_array[data_index - 1] != ARM_OP_FINISH) {
    // If this didn't end with a finish op, add one.
//    data_.push_back(ARM_OP_FINISH);
      data_array[data_index++] = ARM_OP_FINISH;
  }
  return true;
}

inline bool FastArmExidx::GetByte(uint8_t* byte) {
  if (UNLIKELY(data_index == data_read_index)) {
    status_ = ARM_STATUS_TRUNCATED;
    return false;
  }
  *byte = data_array[data_read_index++];
//  data_.pop_front();
  return true;
}

inline bool FastArmExidx::DecodePrefix_10_00(uint8_t byte) {
  CHECK((byte >> 4) == 0x8);

  uint16_t registers = (byte & 0xf) << 8;
  if (!GetByte(&byte)) {
    return false;
  }

  registers |= byte;
  if (registers == 0) {
    // 10000000 00000000: Refuse to unwind
    status_ = ARM_STATUS_NO_UNWIND;
    return false;
  }
  // 1000iiii iiiiiiii: Pop up to 12 integer registers under masks {r15-r12}, {r11-r4}
  registers <<= 4;

    for (size_t reg = ARM_REG_R4; reg < ARM_REG_R7; reg++) {
        if (registers & (1 << reg)) {
            cfa_ += 4;
        }
    }

  // r7
  if (registers & (1 << ARM_REG_R7)) {
    if (!process_memory_->Read32(cfa_, &R7(regs_))) {
      status_ = ARM_STATUS_READ_FAILED;
      status_address_ = cfa_;
      return false;
    }
    cfa_ += 4;
  }

    for (size_t reg = ARM_REG_R8; reg < ARM_REG_R11; reg++) {
        if (registers & (1 << reg)) {
            cfa_ += 4;
        }
    }

  // r11
  if (registers & (1 << ARM_REG_R11)) {
    if (!process_memory_->Read32(cfa_, &R11(regs_))) {
      status_ = ARM_STATUS_READ_FAILED;
      status_address_ = cfa_;
      return false;
    }
    cfa_ += 4;
  }

    if (registers & (1 << ARM_REG_R12)) {
        cfa_ += 4;
    }

    // If the sp register is modified, change the cfa value.
    if (registers & (1 << ARM_REG_SP)) {
        cfa_ += 4;
    }

    // lr
    if (registers & (1 << ARM_REG_LR)) {
        if (!process_memory_->Read32(cfa_, &LR(regs_))) {
            status_ = ARM_STATUS_READ_FAILED;
            status_address_ = cfa_;
            return false;
        }

        cfa_ += 4;
    }

  // PC
  if (registers & (1 << ARM_REG_PC)) {
    if (!process_memory_->Read32(cfa_, &PC(regs_))) {
      status_ = ARM_STATUS_READ_FAILED;
      status_address_ = cfa_;
      return false;
    }

//    LOGE("UPDATE-WTF", "update PC %x", PC(regs_));
    cfa_ += 4;
  }

    if (registers & (1 << ARM_REG_SP)) {
        if (!process_memory_->Read32(cfa_, &SP(regs_))) {
            status_ = ARM_STATUS_READ_FAILED;
            status_address_ = cfa_;
            return false;
        }
        cfa_ = SP(regs_);
    }


//  // Indicate if the pc register was set.
  if (registers & (1 << ARM_REG_PC)) {
    pc_set_ = true;
  }
  return true;
}

inline bool FastArmExidx::DecodePrefix_10_01(uint8_t byte) {
  CHECK((byte >> 4) == 0x9);

  uint8_t bits = byte & 0xf;
  if (bits == 13 || bits == 15) {
    // 10011101: Reserved as prefix for ARM register to register moves
    // 10011111: Reserved as prefix for Intel Wireless MMX register to register moves
    status_ = ARM_STATUS_RESERVED;
    return false;
  }

  // 1001nnnn: Set vsp = r[nnnn] (nnnn != 13, 15)

  // It is impossible for bits to be larger than the total number of
  // arm registers, so don't bother checking if bits is a valid register.

  // r7
  if (bits == ARM_REG_R7) {
    cfa_ = R7(regs_);
  }

  // r11
  else if (bits == ARM_REG_R11) {
    cfa_ = R11(regs_);
  }

  else {
    // TODO !!!! Need do some statistic!!!
  }

  return true;
}

inline bool FastArmExidx::DecodePrefix_10_10(uint8_t byte) {
  CHECK((byte >> 4) == 0xa);

  // 10100nnn: Pop r4-r[4+nnn]
  // 10101nnn: Pop r4-r[4+nnn], r14

  for (size_t i = 4; i <= 4 + (byte & 0x7); i++) {

    // r7
    if (i == ARM_REG_R7) {
      if (!process_memory_->Read32(cfa_, &R7(regs_))) {
        status_ = ARM_STATUS_READ_FAILED;
        status_address_ = cfa_;
        return false;
      }

    // r11
    } else if (i == ARM_REG_R11) {
      if (!process_memory_->Read32(cfa_, &R11(regs_))) {
        status_ = ARM_STATUS_READ_FAILED;
        status_address_ = cfa_;
        return false;
      }
    }

    cfa_ += 4;
  }
  if (byte & 0x8) {

//    LOGE("UPDATE-WTF", "update 2 LR %x before", LR(regs_));
    if (!process_memory_->Read32(cfa_, &LR(regs_))) {
      status_ = ARM_STATUS_READ_FAILED;
      status_address_ = cfa_;
      return false;
    }
//    LOGE("UPDATE-WTF", "update 2 LR %x", PC(regs_));
    cfa_ += 4;
  }
  return true;
}

inline bool FastArmExidx::DecodePrefix_10_11_0000() {
  // 10110000: Finish
  status_ = ARM_STATUS_FINISH;
  return false;
}

inline bool FastArmExidx::DecodePrefix_10_11_0001() {
  uint8_t byte;
  if (!GetByte(&byte)) {
    return false;
  }

  if (byte == 0) {
    // 10110001 00000000: Spare
    status_ = ARM_STATUS_SPARE;
    return false;
  }
  if (byte >> 4) {
    // 10110001 xxxxyyyy: Spare (xxxx != 0000)
    status_ = ARM_STATUS_SPARE;
    return false;
  }

  // 10110001 0000iiii: Pop integer registers under mask {r3, r2, r1, r0}

  for (size_t reg = 0; reg < 4; reg++) {
    if (byte & (1 << reg)) {
//      if (!process_memory_->Read32(cfa_, &(*regs_)[reg])) {
//        status_ = ARM_STATUS_READ_FAILED;
//        status_address_ = cfa_;
//        return false;
//      }
      cfa_ += 4;
    }
  }
  return true;
}

inline bool FastArmExidx::DecodePrefix_10_11_0010() {
  // 10110010 uleb128: vsp = vsp + 0x204 + (uleb128 << 2)
  uint32_t result = 0;
  uint32_t shift = 0;
  uint8_t byte;
  do {
    if (!GetByte(&byte)) {
      return false;
    }

    result |= (byte & 0x7f) << shift;
    shift += 7;
  } while (byte & 0x80);
  result <<= 2;
  cfa_ += 0x204 + result;
  return true;
}

inline bool FastArmExidx::DecodePrefix_10_11_0011() {
  // 10110011 sssscccc: Pop VFP double precision registers D[ssss]-D[ssss+cccc] by FSTMFDX
  uint8_t byte;
  if (!GetByte(&byte)) {
    return false;
  }

  cfa_ += (byte & 0xf) * 8 + 12;
  return true;
}

inline bool FastArmExidx::DecodePrefix_10_11_01nn() {
  // 101101nn: Spare
  status_ = ARM_STATUS_SPARE;
  return false;
}

inline bool FastArmExidx::DecodePrefix_10_11_1nnn(uint8_t byte) {
  CHECK((byte & ~0x07) == 0xb8);

  // 10111nnn: Pop VFP double-precision registers D[8]-D[8+nnn] by FSTMFDX
  // Only update the cfa.
  cfa_ += (byte & 0x7) * 8 + 12;
  return true;
}

inline bool FastArmExidx::DecodePrefix_10(uint8_t byte) {
  CHECK((byte >> 6) == 0x2);

  switch ((byte >> 4) & 0x3) {
  case 0:
    return DecodePrefix_10_00(byte);
  case 1:
    return DecodePrefix_10_01(byte);
  case 2:
    return DecodePrefix_10_10(byte);
  default:
    switch (byte & 0xf) {
    case 0:
      return DecodePrefix_10_11_0000();
    case 1:
      return DecodePrefix_10_11_0001();
    case 2:
      return DecodePrefix_10_11_0010();
    case 3:
      return DecodePrefix_10_11_0011();
    default:
      if (byte & 0x8) {
        return DecodePrefix_10_11_1nnn(byte);
      } else {
        return DecodePrefix_10_11_01nn();
      }
    }
  }
}

inline bool FastArmExidx::DecodePrefix_11_000(uint8_t byte) {
  CHECK((byte & ~0x07) == 0xc0);

  uint8_t bits = byte & 0x7;
  if (bits == 6) {
    if (!GetByte(&byte)) {
      return false;
    }

    // 11000110 sssscccc: Intel Wireless MMX pop wR[ssss]-wR[ssss+cccc]
    // Only update the cfa.
    cfa_ += (byte & 0xf) * 8 + 8;
  } else if (bits == 7) {
    if (!GetByte(&byte)) {
      return false;
    }

    if (byte == 0) {
      // 11000111 00000000: Spare
      status_ = ARM_STATUS_SPARE;
      return false;
    } else if ((byte >> 4) == 0) {
      // 11000111 0000iiii: Intel Wireless MMX pop wCGR registers {wCGR0,1,2,3}
      // Only update the cfa.
      cfa_ += __builtin_popcount(byte) * 4;
    } else {
      // 11000111 xxxxyyyy: Spare (xxxx != 0000)
      status_ = ARM_STATUS_SPARE;
      return false;
    }
  } else {
    // 11000nnn: Intel Wireless MMX pop wR[10]-wR[10+nnn] (nnn != 6, 7)
    // Only update the cfa.
    cfa_ += (byte & 0x7) * 8 + 8;
  }
  return true;
}

inline bool FastArmExidx::DecodePrefix_11_001(uint8_t byte) {
  CHECK((byte & ~0x07) == 0xc8);

  uint8_t bits = byte & 0x7;
  if (bits == 0) {
    // 11001000 sssscccc: Pop VFP double precision registers D[16+ssss]-D[16+ssss+cccc] by VPUSH
    if (!GetByte(&byte)) {
      return false;
    }

    // Only update the cfa.
    cfa_ += (byte & 0xf) * 8 + 8;
  } else if (bits == 1) {
    // 11001001 sssscccc: Pop VFP double precision registers D[ssss]-D[ssss+cccc] by VPUSH
    if (!GetByte(&byte)) {
      return false;
    }

    // Only update the cfa.
    cfa_ += (byte & 0xf) * 8 + 8;
  } else {
    // 11001yyy: Spare (yyy != 000, 001)
    status_ = ARM_STATUS_SPARE;
    return false;
  }
  return true;
}

inline bool FastArmExidx::DecodePrefix_11_010(uint8_t byte) {
  CHECK((byte & ~0x07) == 0xd0);

  // 11010nnn: Pop VFP double precision registers D[8]-D[8+nnn] by VPUSH
  cfa_ += (byte & 0x7) * 8 + 8;
  return true;
}

inline bool FastArmExidx::DecodePrefix_11(uint8_t byte) {
  CHECK((byte >> 6) == 0x3);

  switch ((byte >> 3) & 0x7) {
  case 0:
    return DecodePrefix_11_000(byte);
  case 1:
    return DecodePrefix_11_001(byte);
  case 2:
    return DecodePrefix_11_010(byte);
  default:
    // 11xxxyyy: Spare (xxx != 000, 001, 010)
    status_ = ARM_STATUS_SPARE;
    return false;
  }
}

bool FastArmExidx::Decode() {
  status_ = ARM_STATUS_NONE;
  uint8_t byte;
  if (!GetByte(&byte)) {
    return false;
  }

//  LOGE("ArmExidx WTF", "Arm exidx instruction: " BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(byte));

  switch (byte >> 6) {
  case 0:
    // 00xxxxxx: vsp = vsp + (xxxxxxx << 2) + 4
    cfa_ += ((byte & 0x3f) << 2) + 4;
    break;
  case 1:
    // 01xxxxxx: vsp = vsp - (xxxxxxx << 2) + 4
    cfa_ -= ((byte & 0x3f) << 2) + 4;
    break;
  case 2:
    return DecodePrefix_10(byte);
  default:
    return DecodePrefix_11(byte);
  }
  return true;
}

bool FastArmExidx::Eval() {
  pc_set_ = false;
//  LOGE("UPDATE-WTF", "Arm exidx eval started.");
  while (Decode());
//  LOGE("UPDATE-WTF", "Arm exidx eval stopped.");
  return status_ == ARM_STATUS_FINISH;
}

}  // namespace unwindstack
