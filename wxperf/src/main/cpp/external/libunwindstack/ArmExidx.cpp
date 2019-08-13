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

#include "ArmExidx.h"
#include "Check.h"

namespace unwindstack {

void ArmExidx::LogRawData() {
  std::string log_str("Raw Data:");
  for (const uint8_t data : data_) {
    log_str += android::base::StringPrintf(" 0x%02x", data);
  }
  log(log_indent_, log_str.c_str());
}

bool ArmExidx::ExtractEntryData(uint32_t entry_offset) {
  data_.clear();
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
    if (log_) {
      log(log_indent_, "Raw Data: 0x00 0x00 0x00 0x01");
      log(log_indent_, "[cantunwind]");
    }
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
    data_.push_back((data >> 16) & 0xff);
    data_.push_back((data >> 8) & 0xff);
    uint8_t last_op = data & 0xff;
    data_.push_back(last_op);
    if (last_op != ARM_OP_FINISH) {
      // If this didn't end with a finish op, add one.
      data_.push_back(ARM_OP_FINISH);
    }
    if (log_) {
      LogRawData();
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
      data_.push_back((data >> 16) & 0xff);
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
    data_.push_back((data >> 8) & 0xff);
    data_.push_back(data & 0xff);
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
    data_.push_back((data >> 16) & 0xff);
    data_.push_back((data >> 8) & 0xff);
    data_.push_back(data & 0xff);
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
    data_.push_back((data >> 24) & 0xff);
    data_.push_back((data >> 16) & 0xff);
    data_.push_back((data >> 8) & 0xff);
    data_.push_back(data & 0xff);
    addr += 4;
  }

  if (data_.back() != ARM_OP_FINISH) {
    // If this didn't end with a finish op, add one.
    data_.push_back(ARM_OP_FINISH);
  }

  if (log_) {
    LogRawData();
  }
  return true;
}

inline bool ArmExidx::GetByte(uint8_t* byte) {
  if (data_.empty()) {
    status_ = ARM_STATUS_TRUNCATED;
    return false;
  }
  *byte = data_.front();
  data_.pop_front();
  return true;
}

inline bool ArmExidx::DecodePrefix_10_00(uint8_t byte) {
  CHECK((byte >> 4) == 0x8);

  uint16_t registers = (byte & 0xf) << 8;
  if (!GetByte(&byte)) {
    return false;
  }

  registers |= byte;
  if (registers == 0) {
    // 10000000 00000000: Refuse to unwind
    if (log_) {
      log(log_indent_, "Refuse to unwind");
    }
    status_ = ARM_STATUS_NO_UNWIND;
    return false;
  }
  // 1000iiii iiiiiiii: Pop up to 12 integer registers under masks {r15-r12}, {r11-r4}
  if (log_) {
    bool add_comma = false;
    std::string msg = "pop {";
    for (size_t i = 0; i < 12; i++) {
      if (registers & (1 << i)) {
        if (add_comma) {
          msg += ", ";
        }
        msg += android::base::StringPrintf("r%zu", i + 4);
        add_comma = true;
      }
    }
    log(log_indent_, "%s}", msg.c_str());
    if (log_skip_execution_) {
      return true;
    }
  }

  registers <<= 4;
  for (size_t reg = 4; reg < 16; reg++) {
    if (registers & (1 << reg)) {
      if (!process_memory_->Read32(cfa_, &(*regs_)[reg])) {
        status_ = ARM_STATUS_READ_FAILED;
        status_address_ = cfa_;
        return false;
      }
      cfa_ += 4;
    }
  }

  // If the sp register is modified, change the cfa value.
  if (registers & (1 << ARM_REG_SP)) {
    cfa_ = (*regs_)[ARM_REG_SP];
  }

  // Indicate if the pc register was set.
  if (registers & (1 << ARM_REG_PC)) {
    pc_set_ = true;
  }
  return true;
}

inline bool ArmExidx::DecodePrefix_10_01(uint8_t byte) {
  CHECK((byte >> 4) == 0x9);

  uint8_t bits = byte & 0xf;
  if (bits == 13 || bits == 15) {
    // 10011101: Reserved as prefix for ARM register to register moves
    // 10011111: Reserved as prefix for Intel Wireless MMX register to register moves
    if (log_) {
      log(log_indent_, "[Reserved]");
    }
    status_ = ARM_STATUS_RESERVED;
    return false;
  }
  // 1001nnnn: Set vsp = r[nnnn] (nnnn != 13, 15)
  if (log_) {
    log(log_indent_, "vsp = r%d", bits);
    if (log_skip_execution_) {
      return true;
    }
  }
  // It is impossible for bits to be larger than the total number of
  // arm registers, so don't bother checking if bits is a valid register.
  cfa_ = (*regs_)[bits];
  return true;
}

inline bool ArmExidx::DecodePrefix_10_10(uint8_t byte) {
  CHECK((byte >> 4) == 0xa);

  // 10100nnn: Pop r4-r[4+nnn]
  // 10101nnn: Pop r4-r[4+nnn], r14
  if (log_) {
    std::string msg = "pop {r4";
    uint8_t end_reg = byte & 0x7;
    if (end_reg) {
      msg += android::base::StringPrintf("-r%d", 4 + end_reg);
    }
    if (byte & 0x8) {
      log(log_indent_, "%s, r14}", msg.c_str());
    } else {
      log(log_indent_, "%s}", msg.c_str());
    }
    if (log_skip_execution_) {
      return true;
    }
  }

  for (size_t i = 4; i <= 4 + (byte & 0x7); i++) {
    if (!process_memory_->Read32(cfa_, &(*regs_)[i])) {
      status_ = ARM_STATUS_READ_FAILED;
      status_address_ = cfa_;
      return false;
    }
    cfa_ += 4;
  }
  if (byte & 0x8) {
    if (!process_memory_->Read32(cfa_, &(*regs_)[ARM_REG_R14])) {
      status_ = ARM_STATUS_READ_FAILED;
      status_address_ = cfa_;
      return false;
    }
    cfa_ += 4;
  }
  return true;
}

inline bool ArmExidx::DecodePrefix_10_11_0000() {
  // 10110000: Finish
  if (log_) {
    log(log_indent_, "finish");
    if (log_skip_execution_) {
      status_ = ARM_STATUS_FINISH;
      return false;
    }
  }
  status_ = ARM_STATUS_FINISH;
  return false;
}

inline bool ArmExidx::DecodePrefix_10_11_0001() {
  uint8_t byte;
  if (!GetByte(&byte)) {
    return false;
  }

  if (byte == 0) {
    // 10110001 00000000: Spare
    if (log_) {
      log(log_indent_, "Spare");
    }
    status_ = ARM_STATUS_SPARE;
    return false;
  }
  if (byte >> 4) {
    // 10110001 xxxxyyyy: Spare (xxxx != 0000)
    if (log_) {
      log(log_indent_, "Spare");
    }
    status_ = ARM_STATUS_SPARE;
    return false;
  }

  // 10110001 0000iiii: Pop integer registers under mask {r3, r2, r1, r0}
  if (log_) {
    bool add_comma = false;
    std::string msg = "pop {";
    for (size_t i = 0; i < 4; i++) {
      if (byte & (1 << i)) {
        if (add_comma) {
          msg += ", ";
        }
        msg += android::base::StringPrintf("r%zu", i);
        add_comma = true;
      }
    }
    log(log_indent_, "%s}", msg.c_str());
    if (log_skip_execution_) {
      return true;
    }
  }

  for (size_t reg = 0; reg < 4; reg++) {
    if (byte & (1 << reg)) {
      if (!process_memory_->Read32(cfa_, &(*regs_)[reg])) {
        status_ = ARM_STATUS_READ_FAILED;
        status_address_ = cfa_;
        return false;
      }
      cfa_ += 4;
    }
  }
  return true;
}

inline bool ArmExidx::DecodePrefix_10_11_0010() {
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
  if (log_) {
    log(log_indent_, "vsp = vsp + %d", 0x204 + result);
    if (log_skip_execution_) {
      return true;
    }
  }
  cfa_ += 0x204 + result;
  return true;
}

inline bool ArmExidx::DecodePrefix_10_11_0011() {
  // 10110011 sssscccc: Pop VFP double precision registers D[ssss]-D[ssss+cccc] by FSTMFDX
  uint8_t byte;
  if (!GetByte(&byte)) {
    return false;
  }

  if (log_) {
    uint8_t start_reg = byte >> 4;
    std::string msg = android::base::StringPrintf("pop {d%d", start_reg);
    uint8_t end_reg = start_reg + (byte & 0xf);
    if (end_reg) {
      msg += android::base::StringPrintf("-d%d", end_reg);
    }
    log(log_indent_, "%s}", msg.c_str());
    if (log_skip_execution_) {
      return true;
    }
  }
  cfa_ += (byte & 0xf) * 8 + 12;
  return true;
}

inline bool ArmExidx::DecodePrefix_10_11_01nn() {
  // 101101nn: Spare
  if (log_) {
    log(log_indent_, "Spare");
  }
  status_ = ARM_STATUS_SPARE;
  return false;
}

inline bool ArmExidx::DecodePrefix_10_11_1nnn(uint8_t byte) {
  CHECK((byte & ~0x07) == 0xb8);

  // 10111nnn: Pop VFP double-precision registers D[8]-D[8+nnn] by FSTMFDX
  if (log_) {
    std::string msg = "pop {d8";
    uint8_t last_reg = (byte & 0x7);
    if (last_reg) {
      msg += android::base::StringPrintf("-d%d", last_reg + 8);
    }
    log(log_indent_, "%s}", msg.c_str());
    if (log_skip_execution_) {
      return true;
    }
  }
  // Only update the cfa.
  cfa_ += (byte & 0x7) * 8 + 12;
  return true;
}

inline bool ArmExidx::DecodePrefix_10(uint8_t byte) {
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

inline bool ArmExidx::DecodePrefix_11_000(uint8_t byte) {
  CHECK((byte & ~0x07) == 0xc0);

  uint8_t bits = byte & 0x7;
  if (bits == 6) {
    if (!GetByte(&byte)) {
      return false;
    }

    // 11000110 sssscccc: Intel Wireless MMX pop wR[ssss]-wR[ssss+cccc]
    if (log_) {
      uint8_t start_reg = byte >> 4;
      std::string msg = android::base::StringPrintf("pop {wR%d", start_reg);
      uint8_t end_reg = byte & 0xf;
      if (end_reg) {
        msg += android::base::StringPrintf("-wR%d", start_reg + end_reg);
      }
      log(log_indent_, "%s}", msg.c_str());
      if (log_skip_execution_) {
        return true;
      }
    }
    // Only update the cfa.
    cfa_ += (byte & 0xf) * 8 + 8;
  } else if (bits == 7) {
    if (!GetByte(&byte)) {
      return false;
    }

    if (byte == 0) {
      // 11000111 00000000: Spare
      if (log_) {
        log(log_indent_, "Spare");
      }
      status_ = ARM_STATUS_SPARE;
      return false;
    } else if ((byte >> 4) == 0) {
      // 11000111 0000iiii: Intel Wireless MMX pop wCGR registers {wCGR0,1,2,3}
      if (log_) {
        bool add_comma = false;
        std::string msg = "pop {";
        for (size_t i = 0; i < 4; i++) {
          if (byte & (1 << i)) {
            if (add_comma) {
              msg += ", ";
            }
            msg += android::base::StringPrintf("wCGR%zu", i);
            add_comma = true;
          }
        }
        log(log_indent_, "%s}", msg.c_str());
      }
      // Only update the cfa.
      cfa_ += __builtin_popcount(byte) * 4;
    } else {
      // 11000111 xxxxyyyy: Spare (xxxx != 0000)
      if (log_) {
        log(log_indent_, "Spare");
      }
      status_ = ARM_STATUS_SPARE;
      return false;
    }
  } else {
    // 11000nnn: Intel Wireless MMX pop wR[10]-wR[10+nnn] (nnn != 6, 7)
    if (log_) {
      std::string msg = "pop {wR10";
      uint8_t nnn = byte & 0x7;
      if (nnn) {
        msg += android::base::StringPrintf("-wR%d", 10 + nnn);
      }
      log(log_indent_, "%s}", msg.c_str());
      if (log_skip_execution_) {
        return true;
      }
    }
    // Only update the cfa.
    cfa_ += (byte & 0x7) * 8 + 8;
  }
  return true;
}

inline bool ArmExidx::DecodePrefix_11_001(uint8_t byte) {
  CHECK((byte & ~0x07) == 0xc8);

  uint8_t bits = byte & 0x7;
  if (bits == 0) {
    // 11001000 sssscccc: Pop VFP double precision registers D[16+ssss]-D[16+ssss+cccc] by VPUSH
    if (!GetByte(&byte)) {
      return false;
    }

    if (log_) {
      uint8_t start_reg = byte >> 4;
      std::string msg = android::base::StringPrintf("pop {d%d", 16 + start_reg);
      uint8_t end_reg = byte & 0xf;
      if (end_reg) {
        msg += android::base::StringPrintf("-d%d", 16 + start_reg + end_reg);
      }
      log(log_indent_, "%s}", msg.c_str());
      if (log_skip_execution_) {
        return true;
      }
    }
    // Only update the cfa.
    cfa_ += (byte & 0xf) * 8 + 8;
  } else if (bits == 1) {
    // 11001001 sssscccc: Pop VFP double precision registers D[ssss]-D[ssss+cccc] by VPUSH
    if (!GetByte(&byte)) {
      return false;
    }

    if (log_) {
      uint8_t start_reg = byte >> 4;
      std::string msg = android::base::StringPrintf("pop {d%d", start_reg);
      uint8_t end_reg = byte & 0xf;
      if (end_reg) {
        msg += android::base::StringPrintf("-d%d", start_reg + end_reg);
      }
      log(log_indent_, "%s}", msg.c_str());
      if (log_skip_execution_) {
        return true;
      }
    }
    // Only update the cfa.
    cfa_ += (byte & 0xf) * 8 + 8;
  } else {
    // 11001yyy: Spare (yyy != 000, 001)
    if (log_) {
      log(log_indent_, "Spare");
    }
    status_ = ARM_STATUS_SPARE;
    return false;
  }
  return true;
}

inline bool ArmExidx::DecodePrefix_11_010(uint8_t byte) {
  CHECK((byte & ~0x07) == 0xd0);

  // 11010nnn: Pop VFP double precision registers D[8]-D[8+nnn] by VPUSH
  if (log_) {
    std::string msg = "pop {d8";
    uint8_t end_reg = byte & 0x7;
    if (end_reg) {
      msg += android::base::StringPrintf("-d%d", 8 + end_reg);
    }
    log(log_indent_, "%s}", msg.c_str());
    if (log_skip_execution_) {
      return true;
    }
  }
  cfa_ += (byte & 0x7) * 8 + 8;
  return true;
}

inline bool ArmExidx::DecodePrefix_11(uint8_t byte) {
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
    if (log_) {
      log(log_indent_, "Spare");
    }
    status_ = ARM_STATUS_SPARE;
    return false;
  }
}

bool ArmExidx::Decode() {
  status_ = ARM_STATUS_NONE;
  uint8_t byte;
  if (!GetByte(&byte)) {
    return false;
  }

  switch (byte >> 6) {
  case 0:
    // 00xxxxxx: vsp = vsp + (xxxxxxx << 2) + 4
    if (log_) {
      log(log_indent_, "vsp = vsp + %d", ((byte & 0x3f) << 2) + 4);
      if (log_skip_execution_) {
        break;
      }
    }
    cfa_ += ((byte & 0x3f) << 2) + 4;
    break;
  case 1:
    // 01xxxxxx: vsp = vsp - (xxxxxxx << 2) + 4
    if (log_) {
      log(log_indent_, "vsp = vsp - %d", ((byte & 0x3f) << 2) + 4);
      if (log_skip_execution_) {
        break;
      }
    }
    cfa_ -= ((byte & 0x3f) << 2) + 4;
    break;
  case 2:
    return DecodePrefix_10(byte);
  default:
    return DecodePrefix_11(byte);
  }
  return true;
}

bool ArmExidx::Eval() {
  pc_set_ = false;
  while (Decode());
  return status_ == ARM_STATUS_FINISH;
}

}  // namespace unwindstack
