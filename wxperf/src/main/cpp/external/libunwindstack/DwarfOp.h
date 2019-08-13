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

#ifndef _LIBUNWINDSTACK_DWARF_OP_H
#define _LIBUNWINDSTACK_DWARF_OP_H

#include <stdint.h>

#include <deque>
#include <string>
#include <type_traits>
#include <vector>

#include <unwindstack/DwarfError.h>

#include "DwarfEncoding.h"
#include "RegsInfo.h"

namespace unwindstack {

// Forward declarations.
class DwarfMemory;
class Memory;
template <typename AddressType>
class RegsImpl;

template <typename AddressType>
class DwarfOp {
  // Signed version of AddressType
  typedef typename std::make_signed<AddressType>::type SignedType;

  struct OpCallback {
    const char* name;
    bool (DwarfOp::*handle_func)();
    uint8_t num_required_stack_values;
    uint8_t num_operands;
    uint8_t operands[2];
  };

 public:
  DwarfOp(DwarfMemory* memory, Memory* regular_memory)
      : memory_(memory), regular_memory_(regular_memory) {}
  virtual ~DwarfOp() = default;

  bool Decode();

  bool Eval(uint64_t start, uint64_t end);

  void GetLogInfo(uint64_t start, uint64_t end, std::vector<std::string>* lines);

  AddressType StackAt(size_t index) { return stack_[index]; }
  size_t StackSize() { return stack_.size(); }

  void set_regs_info(RegsInfo<AddressType>* regs_info) { regs_info_ = regs_info; }

  const DwarfErrorData& last_error() { return last_error_; }
  DwarfErrorCode LastErrorCode() { return last_error_.code; }
  uint64_t LastErrorAddress() { return last_error_.address; }

  bool dex_pc_set() { return dex_pc_set_; }

  bool is_register() { return is_register_; }

  uint8_t cur_op() { return cur_op_; }

  Memory* regular_memory() { return regular_memory_; }

 protected:
  AddressType OperandAt(size_t index) { return operands_[index]; }
  size_t OperandsSize() { return operands_.size(); }

  AddressType StackPop() {
    AddressType value = stack_.front();
    stack_.pop_front();
    return value;
  }

 private:
  DwarfMemory* memory_;
  Memory* regular_memory_;

  RegsInfo<AddressType>* regs_info_;
  bool dex_pc_set_ = false;
  bool is_register_ = false;
  DwarfErrorData last_error_{DWARF_ERROR_NONE, 0};
  uint8_t cur_op_;
  std::vector<AddressType> operands_;
  std::deque<AddressType> stack_;

  inline AddressType bool_to_dwarf_bool(bool value) { return value ? 1 : 0; }

  // Op processing functions.
  bool op_deref();
  bool op_deref_size();
  bool op_push();
  bool op_dup();
  bool op_drop();
  bool op_over();
  bool op_pick();
  bool op_swap();
  bool op_rot();
  bool op_abs();
  bool op_and();
  bool op_div();
  bool op_minus();
  bool op_mod();
  bool op_mul();
  bool op_neg();
  bool op_not();
  bool op_or();
  bool op_plus();
  bool op_plus_uconst();
  bool op_shl();
  bool op_shr();
  bool op_shra();
  bool op_xor();
  bool op_bra();
  bool op_eq();
  bool op_ge();
  bool op_gt();
  bool op_le();
  bool op_lt();
  bool op_ne();
  bool op_skip();
  bool op_lit();
  bool op_reg();
  bool op_regx();
  bool op_breg();
  bool op_bregx();
  bool op_nop();
  bool op_not_implemented();

  constexpr static OpCallback kCallbackTable[256] = {
      {nullptr, nullptr, 0, 0, {}},  // 0x00 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0x01 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0x02 illegal op
      {
          // 0x03 DW_OP_addr
          "DW_OP_addr",
          &DwarfOp::op_push,
          0,
          1,
          {DW_EH_PE_absptr},
      },
      {nullptr, nullptr, 0, 0, {}},  // 0x04 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0x05 illegal op
      {
          // 0x06 DW_OP_deref
          "DW_OP_deref",
          &DwarfOp::op_deref,
          1,
          0,
          {},
      },
      {nullptr, nullptr, 0, 0, {}},  // 0x07 illegal op
      {
          // 0x08 DW_OP_const1u
          "DW_OP_const1u",
          &DwarfOp::op_push,
          0,
          1,
          {DW_EH_PE_udata1},
      },
      {
          // 0x09 DW_OP_const1s
          "DW_OP_const1s",
          &DwarfOp::op_push,
          0,
          1,
          {DW_EH_PE_sdata1},
      },
      {
          // 0x0a DW_OP_const2u
          "DW_OP_const2u",
          &DwarfOp::op_push,
          0,
          1,
          {DW_EH_PE_udata2},
      },
      {
          // 0x0b DW_OP_const2s
          "DW_OP_const2s",
          &DwarfOp::op_push,
          0,
          1,
          {DW_EH_PE_sdata2},
      },
      {
          // 0x0c DW_OP_const4u
          "DW_OP_const4u",
          &DwarfOp::op_push,
          0,
          1,
          {DW_EH_PE_udata4},
      },
      {
          // 0x0d DW_OP_const4s
          "DW_OP_const4s",
          &DwarfOp::op_push,
          0,
          1,
          {DW_EH_PE_sdata4},
      },
      {
          // 0x0e DW_OP_const8u
          "DW_OP_const8u",
          &DwarfOp::op_push,
          0,
          1,
          {DW_EH_PE_udata8},
      },
      {
          // 0x0f DW_OP_const8s
          "DW_OP_const8s",
          &DwarfOp::op_push,
          0,
          1,
          {DW_EH_PE_sdata8},
      },
      {
          // 0x10 DW_OP_constu
          "DW_OP_constu",
          &DwarfOp::op_push,
          0,
          1,
          {DW_EH_PE_uleb128},
      },
      {
          // 0x11 DW_OP_consts
          "DW_OP_consts",
          &DwarfOp::op_push,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x12 DW_OP_dup
          "DW_OP_dup",
          &DwarfOp::op_dup,
          1,
          0,
          {},
      },
      {
          // 0x13 DW_OP_drop
          "DW_OP_drop",
          &DwarfOp::op_drop,
          1,
          0,
          {},
      },
      {
          // 0x14 DW_OP_over
          "DW_OP_over",
          &DwarfOp::op_over,
          2,
          0,
          {},
      },
      {
          // 0x15 DW_OP_pick
          "DW_OP_pick",
          &DwarfOp::op_pick,
          0,
          1,
          {DW_EH_PE_udata1},
      },
      {
          // 0x16 DW_OP_swap
          "DW_OP_swap",
          &DwarfOp::op_swap,
          2,
          0,
          {},
      },
      {
          // 0x17 DW_OP_rot
          "DW_OP_rot",
          &DwarfOp::op_rot,
          3,
          0,
          {},
      },
      {
          // 0x18 DW_OP_xderef
          "DW_OP_xderef",
          &DwarfOp::op_not_implemented,
          2,
          0,
          {},
      },
      {
          // 0x19 DW_OP_abs
          "DW_OP_abs",
          &DwarfOp::op_abs,
          1,
          0,
          {},
      },
      {
          // 0x1a DW_OP_and
          "DW_OP_and",
          &DwarfOp::op_and,
          2,
          0,
          {},
      },
      {
          // 0x1b DW_OP_div
          "DW_OP_div",
          &DwarfOp::op_div,
          2,
          0,
          {},
      },
      {
          // 0x1c DW_OP_minus
          "DW_OP_minus",
          &DwarfOp::op_minus,
          2,
          0,
          {},
      },
      {
          // 0x1d DW_OP_mod
          "DW_OP_mod",
          &DwarfOp::op_mod,
          2,
          0,
          {},
      },
      {
          // 0x1e DW_OP_mul
          "DW_OP_mul",
          &DwarfOp::op_mul,
          2,
          0,
          {},
      },
      {
          // 0x1f DW_OP_neg
          "DW_OP_neg",
          &DwarfOp::op_neg,
          1,
          0,
          {},
      },
      {
          // 0x20 DW_OP_not
          "DW_OP_not",
          &DwarfOp::op_not,
          1,
          0,
          {},
      },
      {
          // 0x21 DW_OP_or
          "DW_OP_or",
          &DwarfOp::op_or,
          2,
          0,
          {},
      },
      {
          // 0x22 DW_OP_plus
          "DW_OP_plus",
          &DwarfOp::op_plus,
          2,
          0,
          {},
      },
      {
          // 0x23 DW_OP_plus_uconst
          "DW_OP_plus_uconst",
          &DwarfOp::op_plus_uconst,
          1,
          1,
          {DW_EH_PE_uleb128},
      },
      {
          // 0x24 DW_OP_shl
          "DW_OP_shl",
          &DwarfOp::op_shl,
          2,
          0,
          {},
      },
      {
          // 0x25 DW_OP_shr
          "DW_OP_shr",
          &DwarfOp::op_shr,
          2,
          0,
          {},
      },
      {
          // 0x26 DW_OP_shra
          "DW_OP_shra",
          &DwarfOp::op_shra,
          2,
          0,
          {},
      },
      {
          // 0x27 DW_OP_xor
          "DW_OP_xor",
          &DwarfOp::op_xor,
          2,
          0,
          {},
      },
      {
          // 0x28 DW_OP_bra
          "DW_OP_bra",
          &DwarfOp::op_bra,
          1,
          1,
          {DW_EH_PE_sdata2},
      },
      {
          // 0x29 DW_OP_eq
          "DW_OP_eq",
          &DwarfOp::op_eq,
          2,
          0,
          {},
      },
      {
          // 0x2a DW_OP_ge
          "DW_OP_ge",
          &DwarfOp::op_ge,
          2,
          0,
          {},
      },
      {
          // 0x2b DW_OP_gt
          "DW_OP_gt",
          &DwarfOp::op_gt,
          2,
          0,
          {},
      },
      {
          // 0x2c DW_OP_le
          "DW_OP_le",
          &DwarfOp::op_le,
          2,
          0,
          {},
      },
      {
          // 0x2d DW_OP_lt
          "DW_OP_lt",
          &DwarfOp::op_lt,
          2,
          0,
          {},
      },
      {
          // 0x2e DW_OP_ne
          "DW_OP_ne",
          &DwarfOp::op_ne,
          2,
          0,
          {},
      },
      {
          // 0x2f DW_OP_skip
          "DW_OP_skip",
          &DwarfOp::op_skip,
          0,
          1,
          {DW_EH_PE_sdata2},
      },
      {
          // 0x30 DW_OP_lit0
          "DW_OP_lit0",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x31 DW_OP_lit1
          "DW_OP_lit1",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x32 DW_OP_lit2
          "DW_OP_lit2",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x33 DW_OP_lit3
          "DW_OP_lit3",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x34 DW_OP_lit4
          "DW_OP_lit4",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x35 DW_OP_lit5
          "DW_OP_lit5",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x36 DW_OP_lit6
          "DW_OP_lit6",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x37 DW_OP_lit7
          "DW_OP_lit7",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x38 DW_OP_lit8
          "DW_OP_lit8",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x39 DW_OP_lit9
          "DW_OP_lit9",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x3a DW_OP_lit10
          "DW_OP_lit10",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x3b DW_OP_lit11
          "DW_OP_lit11",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x3c DW_OP_lit12
          "DW_OP_lit12",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x3d DW_OP_lit13
          "DW_OP_lit13",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x3e DW_OP_lit14
          "DW_OP_lit14",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x3f DW_OP_lit15
          "DW_OP_lit15",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x40 DW_OP_lit16
          "DW_OP_lit16",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x41 DW_OP_lit17
          "DW_OP_lit17",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x42 DW_OP_lit18
          "DW_OP_lit18",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x43 DW_OP_lit19
          "DW_OP_lit19",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x44 DW_OP_lit20
          "DW_OP_lit20",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x45 DW_OP_lit21
          "DW_OP_lit21",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x46 DW_OP_lit22
          "DW_OP_lit22",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x47 DW_OP_lit23
          "DW_OP_lit23",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x48 DW_OP_lit24
          "DW_OP_lit24",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x49 DW_OP_lit25
          "DW_OP_lit25",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x4a DW_OP_lit26
          "DW_OP_lit26",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x4b DW_OP_lit27
          "DW_OP_lit27",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x4c DW_OP_lit28
          "DW_OP_lit28",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x4d DW_OP_lit29
          "DW_OP_lit29",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x4e DW_OP_lit30
          "DW_OP_lit30",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x4f DW_OP_lit31
          "DW_OP_lit31",
          &DwarfOp::op_lit,
          0,
          0,
          {},
      },
      {
          // 0x50 DW_OP_reg0
          "DW_OP_reg0",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x51 DW_OP_reg1
          "DW_OP_reg1",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x52 DW_OP_reg2
          "DW_OP_reg2",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x53 DW_OP_reg3
          "DW_OP_reg3",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x54 DW_OP_reg4
          "DW_OP_reg4",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x55 DW_OP_reg5
          "DW_OP_reg5",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x56 DW_OP_reg6
          "DW_OP_reg6",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x57 DW_OP_reg7
          "DW_OP_reg7",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x58 DW_OP_reg8
          "DW_OP_reg8",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x59 DW_OP_reg9
          "DW_OP_reg9",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x5a DW_OP_reg10
          "DW_OP_reg10",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x5b DW_OP_reg11
          "DW_OP_reg11",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x5c DW_OP_reg12
          "DW_OP_reg12",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x5d DW_OP_reg13
          "DW_OP_reg13",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x5e DW_OP_reg14
          "DW_OP_reg14",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x5f DW_OP_reg15
          "DW_OP_reg15",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x60 DW_OP_reg16
          "DW_OP_reg16",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x61 DW_OP_reg17
          "DW_OP_reg17",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x62 DW_OP_reg18
          "DW_OP_reg18",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x63 DW_OP_reg19
          "DW_OP_reg19",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x64 DW_OP_reg20
          "DW_OP_reg20",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x65 DW_OP_reg21
          "DW_OP_reg21",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x66 DW_OP_reg22
          "DW_OP_reg22",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x67 DW_OP_reg23
          "DW_OP_reg23",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x68 DW_OP_reg24
          "DW_OP_reg24",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x69 DW_OP_reg25
          "DW_OP_reg25",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x6a DW_OP_reg26
          "DW_OP_reg26",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x6b DW_OP_reg27
          "DW_OP_reg27",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x6c DW_OP_reg28
          "DW_OP_reg28",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x6d DW_OP_reg29
          "DW_OP_reg29",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x6e DW_OP_reg30
          "DW_OP_reg30",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x6f DW_OP_reg31
          "DW_OP_reg31",
          &DwarfOp::op_reg,
          0,
          0,
          {},
      },
      {
          // 0x70 DW_OP_breg0
          "DW_OP_breg0",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x71 DW_OP_breg1
          "DW_OP_breg1",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x72 DW_OP_breg2
          "DW_OP_breg2",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x73 DW_OP_breg3
          "DW_OP_breg3",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x74 DW_OP_breg4
          "DW_OP_breg4",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x75 DW_OP_breg5
          "DW_OP_breg5",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x76 DW_OP_breg6
          "DW_OP_breg6",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x77 DW_OP_breg7
          "DW_OP_breg7",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x78 DW_OP_breg8
          "DW_OP_breg8",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x79 DW_OP_breg9
          "DW_OP_breg9",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x7a DW_OP_breg10
          "DW_OP_breg10",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x7b DW_OP_breg11
          "DW_OP_breg11",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x7c DW_OP_breg12
          "DW_OP_breg12",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x7d DW_OP_breg13
          "DW_OP_breg13",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x7e DW_OP_breg14
          "DW_OP_breg14",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x7f DW_OP_breg15
          "DW_OP_breg15",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x80 DW_OP_breg16
          "DW_OP_breg16",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x81 DW_OP_breg17
          "DW_OP_breg17",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x82 DW_OP_breg18
          "DW_OP_breg18",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x83 DW_OP_breg19
          "DW_OP_breg19",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x84 DW_OP_breg20
          "DW_OP_breg20",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x85 DW_OP_breg21
          "DW_OP_breg21",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x86 DW_OP_breg22
          "DW_OP_breg22",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x87 DW_OP_breg23
          "DW_OP_breg23",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x88 DW_OP_breg24
          "DW_OP_breg24",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x89 DW_OP_breg25
          "DW_OP_breg25",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x8a DW_OP_breg26
          "DW_OP_breg26",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x8b DW_OP_breg27
          "DW_OP_breg27",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x8c DW_OP_breg28
          "DW_OP_breg28",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x8d DW_OP_breg29
          "DW_OP_breg29",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x8e DW_OP_breg30
          "DW_OP_breg30",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x8f DW_OP_breg31
          "DW_OP_breg31",
          &DwarfOp::op_breg,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x90 DW_OP_regx
          "DW_OP_regx",
          &DwarfOp::op_regx,
          0,
          1,
          {DW_EH_PE_uleb128},
      },
      {
          // 0x91 DW_OP_fbreg
          "DW_OP_fbreg",
          &DwarfOp::op_not_implemented,
          0,
          1,
          {DW_EH_PE_sleb128},
      },
      {
          // 0x92 DW_OP_bregx
          "DW_OP_bregx",
          &DwarfOp::op_bregx,
          0,
          2,
          {DW_EH_PE_uleb128, DW_EH_PE_sleb128},
      },
      {
          // 0x93 DW_OP_piece
          "DW_OP_piece",
          &DwarfOp::op_not_implemented,
          0,
          1,
          {DW_EH_PE_uleb128},
      },
      {
          // 0x94 DW_OP_deref_size
          "DW_OP_deref_size",
          &DwarfOp::op_deref_size,
          1,
          1,
          {DW_EH_PE_udata1},
      },
      {
          // 0x95 DW_OP_xderef_size
          "DW_OP_xderef_size",
          &DwarfOp::op_not_implemented,
          0,
          1,
          {DW_EH_PE_udata1},
      },
      {
          // 0x96 DW_OP_nop
          "DW_OP_nop",
          &DwarfOp::op_nop,
          0,
          0,
          {},
      },
      {
          // 0x97 DW_OP_push_object_address
          "DW_OP_push_object_address",
          &DwarfOp::op_not_implemented,
          0,
          0,
          {},
      },
      {
          // 0x98 DW_OP_call2
          "DW_OP_call2",
          &DwarfOp::op_not_implemented,
          0,
          1,
          {DW_EH_PE_udata2},
      },
      {
          // 0x99 DW_OP_call4
          "DW_OP_call4",
          &DwarfOp::op_not_implemented,
          0,
          1,
          {DW_EH_PE_udata4},
      },
      {
          // 0x9a DW_OP_call_ref
          "DW_OP_call_ref",
          &DwarfOp::op_not_implemented,
          0,
          0,  // Has a different sized operand (4 bytes or 8 bytes).
          {},
      },
      {
          // 0x9b DW_OP_form_tls_address
          "DW_OP_form_tls_address",
          &DwarfOp::op_not_implemented,
          0,
          0,
          {},
      },
      {
          // 0x9c DW_OP_call_frame_cfa
          "DW_OP_call_frame_cfa",
          &DwarfOp::op_not_implemented,
          0,
          0,
          {},
      },
      {
          // 0x9d DW_OP_bit_piece
          "DW_OP_bit_piece",
          &DwarfOp::op_not_implemented,
          0,
          2,
          {DW_EH_PE_uleb128, DW_EH_PE_uleb128},
      },
      {
          // 0x9e DW_OP_implicit_value
          "DW_OP_implicit_value",
          &DwarfOp::op_not_implemented,
          0,
          1,
          {DW_EH_PE_uleb128},
      },
      {
          // 0x9f DW_OP_stack_value
          "DW_OP_stack_value",
          &DwarfOp::op_not_implemented,
          1,
          0,
          {},
      },
      {nullptr, nullptr, 0, 0, {}},  // 0xa0 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xa1 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xa2 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xa3 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xa4 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xa5 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xa6 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xa7 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xa8 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xa9 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xaa illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xab illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xac illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xad illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xae illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xaf illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xb0 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xb1 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xb2 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xb3 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xb4 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xb5 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xb6 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xb7 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xb8 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xb9 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xba illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xbb illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xbc illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xbd illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xbe illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xbf illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xc0 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xc1 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xc2 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xc3 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xc4 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xc5 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xc6 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xc7 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xc8 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xc9 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xca illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xcb illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xcc illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xcd illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xce illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xcf illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xd0 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xd1 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xd2 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xd3 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xd4 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xd5 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xd6 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xd7 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xd8 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xd9 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xda illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xdb illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xdc illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xdd illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xde illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xdf illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xe0 DW_OP_lo_user
      {nullptr, nullptr, 0, 0, {}},  // 0xe1 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xe2 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xe3 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xe4 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xe5 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xe6 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xe7 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xe8 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xe9 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xea illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xeb illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xec illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xed illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xee illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xef illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xf0 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xf1 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xf2 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xf3 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xf4 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xf5 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xf6 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xf7 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xf8 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xf9 illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xfa illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xfb illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xfc illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xfd illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xfe illegal op
      {nullptr, nullptr, 0, 0, {}},  // 0xff DW_OP_hi_user
  };
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_DWARF_OP_H
