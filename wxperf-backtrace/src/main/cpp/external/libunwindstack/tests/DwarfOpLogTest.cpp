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

#include <ios>
#include <vector>

#include <gtest/gtest.h>

#include <unwindstack/DwarfError.h>
#include <unwindstack/DwarfMemory.h>
#include <unwindstack/Log.h>
#include <unwindstack/Regs.h>

#include "DwarfOp.h"

#include "MemoryFake.h"

namespace unwindstack {

template <typename TypeParam>
class DwarfOpLogTest : public ::testing::Test {
 protected:
  void SetUp() override {
    op_memory_.Clear();
    regular_memory_.Clear();
    mem_.reset(new DwarfMemory(&op_memory_));
    op_.reset(new DwarfOp<TypeParam>(mem_.get(), &regular_memory_));
  }

  MemoryFake op_memory_;
  MemoryFake regular_memory_;

  std::unique_ptr<DwarfMemory> mem_;
  std::unique_ptr<DwarfOp<TypeParam>> op_;
};
TYPED_TEST_CASE_P(DwarfOpLogTest);

TYPED_TEST_P(DwarfOpLogTest, multiple_ops) {
  // Multi operation opcodes.
  std::vector<uint8_t> opcode_buffer = {
      0x0a, 0x20, 0x10, 0x08, 0x03, 0x12, 0x27,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  std::vector<std::string> lines;
  this->op_->GetLogInfo(0, opcode_buffer.size(), &lines);
  std::vector<std::string> expected{
      "DW_OP_const2u 4128", "Raw Data: 0x0a 0x20 0x10", "DW_OP_const1u 3", "Raw Data: 0x08 0x03",
      "DW_OP_dup",          "Raw Data: 0x12",           "DW_OP_xor",       "Raw Data: 0x27"};
  ASSERT_EQ(expected, lines);
}

REGISTER_TYPED_TEST_CASE_P(DwarfOpLogTest, multiple_ops);

typedef ::testing::Types<uint32_t, uint64_t> DwarfOpLogTestTypes;
INSTANTIATE_TYPED_TEST_CASE_P(, DwarfOpLogTest, DwarfOpLogTestTypes);

}  // namespace unwindstack
