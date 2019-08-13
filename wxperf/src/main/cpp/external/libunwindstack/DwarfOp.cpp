/*
 * Copyright (C) 2017 The Android Open Source Project
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
#include <vector>

#include <android-base/stringprintf.h>

#include <unwindstack/DwarfError.h>
#include <unwindstack/DwarfMemory.h>
#include <unwindstack/Log.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Regs.h>

#include "DwarfOp.h"

namespace unwindstack {

template <typename AddressType>
constexpr typename DwarfOp<AddressType>::OpCallback DwarfOp<AddressType>::kCallbackTable[256];

template <typename AddressType>
bool DwarfOp<AddressType>::Eval(uint64_t start, uint64_t end) {
  is_register_ = false;
  stack_.clear();
  memory_->set_cur_offset(start);
  dex_pc_set_ = false;

  // Unroll the first Decode calls to be able to check for a special
  // sequence of ops and values that indicate this is the dex pc.
  // The pattern is:
  //   OP_const4u (0x0c)  'D' 'E' 'X' '1'
  //   OP_drop (0x13)
  if (memory_->cur_offset() < end) {
    if (!Decode()) {
      return false;
    }
  } else {
    return true;
  }
  bool check_for_drop;
  if (cur_op_ == 0x0c && operands_.back() == 0x31584544) {
    check_for_drop = true;
  } else {
    check_for_drop = false;
  }
  if (memory_->cur_offset() < end) {
    if (!Decode()) {
      return false;
    }
  } else {
    return true;
  }

  if (check_for_drop && cur_op_ == 0x13) {
    dex_pc_set_ = true;
  }

  uint32_t iterations = 2;
  while (memory_->cur_offset() < end) {
    if (!Decode()) {
      return false;
    }
    // To protect against a branch that creates an infinite loop,
    // terminate if the number of iterations gets too high.
    if (iterations++ == 1000) {
      last_error_.code = DWARF_ERROR_TOO_MANY_ITERATIONS;
      return false;
    }
  }
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::Decode() {
  last_error_.code = DWARF_ERROR_NONE;
  if (!memory_->ReadBytes(&cur_op_, 1)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_->cur_offset();
    return false;
  }

  const auto* op = &kCallbackTable[cur_op_];
  const auto handle_func = op->handle_func;
  if (handle_func == nullptr) {
    last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
    return false;
  }

  // Make sure that the required number of stack elements is available.
  if (stack_.size() < op->num_required_stack_values) {
    last_error_.code = DWARF_ERROR_STACK_INDEX_NOT_VALID;
    return false;
  }

  operands_.clear();
  for (size_t i = 0; i < op->num_operands; i++) {
    uint64_t value;
    if (!memory_->ReadEncodedValue<AddressType>(op->operands[i], &value)) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_->cur_offset();
      return false;
    }
    operands_.push_back(value);
  }
  return (this->*handle_func)();
}

template <typename AddressType>
void DwarfOp<AddressType>::GetLogInfo(uint64_t start, uint64_t end,
                                      std::vector<std::string>* lines) {
  memory_->set_cur_offset(start);
  while (memory_->cur_offset() < end) {
    uint8_t cur_op;
    if (!memory_->ReadBytes(&cur_op, 1)) {
      return;
    }

    std::string raw_string(android::base::StringPrintf("Raw Data: 0x%02x", cur_op));
    std::string log_string;
    const auto* op = &kCallbackTable[cur_op];
    if (op->handle_func == nullptr) {
      log_string = "Illegal";
    } else {
      log_string = op->name;
      uint64_t start_offset = memory_->cur_offset();
      for (size_t i = 0; i < op->num_operands; i++) {
        uint64_t value;
        if (!memory_->ReadEncodedValue<AddressType>(op->operands[i], &value)) {
          return;
        }
        log_string += ' ' + std::to_string(value);
      }
      uint64_t end_offset = memory_->cur_offset();

      memory_->set_cur_offset(start_offset);
      for (size_t i = start_offset; i < end_offset; i++) {
        uint8_t byte;
        if (!memory_->ReadBytes(&byte, 1)) {
          return;
        }
        raw_string += android::base::StringPrintf(" 0x%02x", byte);
      }
      memory_->set_cur_offset(end_offset);
    }
    lines->push_back(std::move(log_string));
    lines->push_back(std::move(raw_string));
  }
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_deref() {
  // Read the address and dereference it.
  AddressType addr = StackPop();
  AddressType value;
  if (!regular_memory()->ReadFully(addr, &value, sizeof(value))) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = addr;
    return false;
  }
  stack_.push_front(value);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_deref_size() {
  AddressType bytes_to_read = OperandAt(0);
  if (bytes_to_read > sizeof(AddressType) || bytes_to_read == 0) {
    last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
    return false;
  }
  // Read the address and dereference it.
  AddressType addr = StackPop();
  AddressType value = 0;
  if (!regular_memory()->ReadFully(addr, &value, bytes_to_read)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = addr;
    return false;
  }
  stack_.push_front(value);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_push() {
  // Push all of the operands.
  for (auto operand : operands_) {
    stack_.push_front(operand);
  }
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_dup() {
  stack_.push_front(StackAt(0));
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_drop() {
  StackPop();
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_over() {
  stack_.push_front(StackAt(1));
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_pick() {
  AddressType index = OperandAt(0);
  if (index > StackSize()) {
    last_error_.code = DWARF_ERROR_STACK_INDEX_NOT_VALID;
    return false;
  }
  stack_.push_front(StackAt(index));
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_swap() {
  AddressType old_value = stack_[0];
  stack_[0] = stack_[1];
  stack_[1] = old_value;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_rot() {
  AddressType top = stack_[0];
  stack_[0] = stack_[1];
  stack_[1] = stack_[2];
  stack_[2] = top;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_abs() {
  SignedType signed_value = static_cast<SignedType>(stack_[0]);
  if (signed_value < 0) {
    signed_value = -signed_value;
  }
  stack_[0] = static_cast<AddressType>(signed_value);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_and() {
  AddressType top = StackPop();
  stack_[0] &= top;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_div() {
  AddressType top = StackPop();
  if (top == 0) {
    last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
    return false;
  }
  SignedType signed_divisor = static_cast<SignedType>(top);
  SignedType signed_dividend = static_cast<SignedType>(stack_[0]);
  stack_[0] = static_cast<AddressType>(signed_dividend / signed_divisor);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_minus() {
  AddressType top = StackPop();
  stack_[0] -= top;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_mod() {
  AddressType top = StackPop();
  if (top == 0) {
    last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
    return false;
  }
  stack_[0] %= top;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_mul() {
  AddressType top = StackPop();
  stack_[0] *= top;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_neg() {
  SignedType signed_value = static_cast<SignedType>(stack_[0]);
  stack_[0] = static_cast<AddressType>(-signed_value);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_not() {
  stack_[0] = ~stack_[0];
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_or() {
  AddressType top = StackPop();
  stack_[0] |= top;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_plus() {
  AddressType top = StackPop();
  stack_[0] += top;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_plus_uconst() {
  stack_[0] += OperandAt(0);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_shl() {
  AddressType top = StackPop();
  stack_[0] <<= top;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_shr() {
  AddressType top = StackPop();
  stack_[0] >>= top;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_shra() {
  AddressType top = StackPop();
  SignedType signed_value = static_cast<SignedType>(stack_[0]) >> top;
  stack_[0] = static_cast<AddressType>(signed_value);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_xor() {
  AddressType top = StackPop();
  stack_[0] ^= top;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_bra() {
  // Requires one stack element.
  AddressType top = StackPop();
  int16_t offset = static_cast<int16_t>(OperandAt(0));
  uint64_t cur_offset;
  if (top != 0) {
    cur_offset = memory_->cur_offset() + offset;
  } else {
    cur_offset = memory_->cur_offset() - offset;
  }
  memory_->set_cur_offset(cur_offset);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_eq() {
  AddressType top = StackPop();
  stack_[0] = bool_to_dwarf_bool(stack_[0] == top);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_ge() {
  AddressType top = StackPop();
  stack_[0] = bool_to_dwarf_bool(stack_[0] >= top);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_gt() {
  AddressType top = StackPop();
  stack_[0] = bool_to_dwarf_bool(stack_[0] > top);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_le() {
  AddressType top = StackPop();
  stack_[0] = bool_to_dwarf_bool(stack_[0] <= top);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_lt() {
  AddressType top = StackPop();
  stack_[0] = bool_to_dwarf_bool(stack_[0] < top);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_ne() {
  AddressType top = StackPop();
  stack_[0] = bool_to_dwarf_bool(stack_[0] != top);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_skip() {
  int16_t offset = static_cast<int16_t>(OperandAt(0));
  uint64_t cur_offset = memory_->cur_offset() + offset;
  memory_->set_cur_offset(cur_offset);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_lit() {
  stack_.push_front(cur_op() - 0x30);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_reg() {
  is_register_ = true;
  stack_.push_front(cur_op() - 0x50);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_regx() {
  is_register_ = true;
  stack_.push_front(OperandAt(0));
  return true;
}

// It's not clear for breg/bregx, if this op should read the current
// value of the register, or where we think that register is located.
// For simplicity, the code will read the value before doing the unwind.
template <typename AddressType>
bool DwarfOp<AddressType>::op_breg() {
  uint16_t reg = cur_op() - 0x70;
  if (reg >= regs_info_->Total()) {
    last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
    return false;
  }
  stack_.push_front(regs_info_->Get(reg) + OperandAt(0));
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_bregx() {
  AddressType reg = OperandAt(0);
  if (reg >= regs_info_->Total()) {
    last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
    return false;
  }
  stack_.push_front(regs_info_->Get(reg) + OperandAt(1));
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_nop() {
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_not_implemented() {
  last_error_.code = DWARF_ERROR_NOT_IMPLEMENTED;
  return false;
}

// Explicitly instantiate DwarfOp.
template class DwarfOp<uint32_t>;
template class DwarfOp<uint64_t>;

}  // namespace unwindstack
