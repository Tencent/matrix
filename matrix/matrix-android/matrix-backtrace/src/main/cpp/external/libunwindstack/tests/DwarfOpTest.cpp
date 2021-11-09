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

#include "DwarfOp.h"

#include "MemoryFake.h"
#include "RegsFake.h"

namespace unwindstack {

template <typename TypeParam>
class DwarfOpTest : public ::testing::Test {
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
TYPED_TEST_CASE_P(DwarfOpTest);

TYPED_TEST_P(DwarfOpTest, decode) {
  // Memory error.
  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_MEMORY_INVALID, this->op_->LastErrorCode());
  EXPECT_EQ(0U, this->op_->LastErrorAddress());

  // No error.
  this->op_memory_.SetMemory(0, std::vector<uint8_t>{0x96});
  this->mem_->set_cur_offset(0);
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_NONE, this->op_->LastErrorCode());
  ASSERT_EQ(0x96U, this->op_->cur_op());
  ASSERT_EQ(1U, this->mem_->cur_offset());
}

TYPED_TEST_P(DwarfOpTest, eval) {
  // Memory error.
  ASSERT_FALSE(this->op_->Eval(0, 2));
  ASSERT_EQ(DWARF_ERROR_MEMORY_INVALID, this->op_->LastErrorCode());
  EXPECT_EQ(0U, this->op_->LastErrorAddress());

  // Register set.
  // Do this first, to verify that subsequent calls reset the value.
  this->op_memory_.SetMemory(0, std::vector<uint8_t>{0x50});
  ASSERT_TRUE(this->op_->Eval(0, 1));
  ASSERT_TRUE(this->op_->is_register());
  ASSERT_EQ(1U, this->mem_->cur_offset());
  ASSERT_EQ(1U, this->op_->StackSize());

  // Multi operation opcodes.
  std::vector<uint8_t> opcode_buffer = {
      0x08, 0x04, 0x08, 0x03, 0x08, 0x02, 0x08, 0x01,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_TRUE(this->op_->Eval(0, 8));
  ASSERT_EQ(DWARF_ERROR_NONE, this->op_->LastErrorCode());
  ASSERT_FALSE(this->op_->is_register());
  ASSERT_EQ(8U, this->mem_->cur_offset());
  ASSERT_EQ(4U, this->op_->StackSize());
  ASSERT_EQ(1U, this->op_->StackAt(0));
  ASSERT_EQ(2U, this->op_->StackAt(1));
  ASSERT_EQ(3U, this->op_->StackAt(2));
  ASSERT_EQ(4U, this->op_->StackAt(3));

  // Infinite loop.
  this->op_memory_.SetMemory(0, std::vector<uint8_t>{0x2f, 0xfd, 0xff});
  ASSERT_FALSE(this->op_->Eval(0, 4));
  ASSERT_EQ(DWARF_ERROR_TOO_MANY_ITERATIONS, this->op_->LastErrorCode());
  ASSERT_FALSE(this->op_->is_register());
  ASSERT_EQ(0U, this->op_->StackSize());
}

TYPED_TEST_P(DwarfOpTest, illegal_opcode) {
  // Fill the buffer with all of the illegal opcodes.
  std::vector<uint8_t> opcode_buffer = {0x00, 0x01, 0x02, 0x04, 0x05, 0x07};
  for (size_t opcode = 0xa0; opcode < 256; opcode++) {
    opcode_buffer.push_back(opcode);
  }
  this->op_memory_.SetMemory(0, opcode_buffer);

  for (size_t i = 0; i < opcode_buffer.size(); i++) {
    ASSERT_FALSE(this->op_->Decode());
    ASSERT_EQ(DWARF_ERROR_ILLEGAL_VALUE, this->op_->LastErrorCode());
    ASSERT_EQ(opcode_buffer[i], this->op_->cur_op());
  }
}

TYPED_TEST_P(DwarfOpTest, not_implemented) {
  std::vector<uint8_t> opcode_buffer = {
      // Push values so that any not implemented ops will return the right error.
      0x08, 0x03, 0x08, 0x02, 0x08, 0x01,
      // xderef
      0x18,
      // fbreg
      0x91, 0x01,
      // piece
      0x93, 0x01,
      // xderef_size
      0x95, 0x01,
      // push_object_address
      0x97,
      // call2
      0x98, 0x01, 0x02,
      // call4
      0x99, 0x01, 0x02, 0x03, 0x04,
      // call_ref
      0x9a,
      // form_tls_address
      0x9b,
      // call_frame_cfa
      0x9c,
      // bit_piece
      0x9d, 0x01, 0x01,
      // implicit_value
      0x9e, 0x01,
      // stack_value
      0x9f,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  // Push the stack values.
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_TRUE(this->op_->Decode());

  while (this->mem_->cur_offset() < opcode_buffer.size()) {
    ASSERT_FALSE(this->op_->Decode());
    ASSERT_EQ(DWARF_ERROR_NOT_IMPLEMENTED, this->op_->LastErrorCode());
  }
}

TYPED_TEST_P(DwarfOpTest, op_addr) {
  std::vector<uint8_t> opcode_buffer = {0x03, 0x12, 0x23, 0x34, 0x45};
  if (sizeof(TypeParam) == 8) {
    opcode_buffer.push_back(0x56);
    opcode_buffer.push_back(0x67);
    opcode_buffer.push_back(0x78);
    opcode_buffer.push_back(0x89);
  }
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x03, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  if (sizeof(TypeParam) == 4) {
    ASSERT_EQ(0x45342312U, this->op_->StackAt(0));
  } else {
    ASSERT_EQ(0x8978675645342312UL, this->op_->StackAt(0));
  }
}

TYPED_TEST_P(DwarfOpTest, op_deref) {
  std::vector<uint8_t> opcode_buffer = {
      // Try a dereference with nothing on the stack.
      0x06,
      // Add an address, then dereference.
      0x0a, 0x10, 0x20, 0x06,
      // Now do another dereference that should fail in memory.
      0x06,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);
  TypeParam value = 0x12345678;
  this->regular_memory_.SetMemory(0x2010, &value, sizeof(value));

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x06, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(value, this->op_->StackAt(0));

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_MEMORY_INVALID, this->op_->LastErrorCode());
  ASSERT_EQ(0x12345678U, this->op_->LastErrorAddress());
}

TYPED_TEST_P(DwarfOpTest, op_deref_size) {
  this->op_memory_.SetMemory(0, std::vector<uint8_t>{0x94});
  TypeParam value = 0x12345678;
  this->regular_memory_.SetMemory(0x2010, &value, sizeof(value));

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  // Read all byte sizes up to the sizeof the type.
  for (size_t i = 1; i < sizeof(TypeParam); i++) {
    this->op_memory_.SetMemory(
        0, std::vector<uint8_t>{0x0a, 0x10, 0x20, 0x94, static_cast<uint8_t>(i)});
    ASSERT_TRUE(this->op_->Eval(0, 5)) << "Failed at size " << i;
    ASSERT_EQ(1U, this->op_->StackSize()) << "Failed at size " << i;
    ASSERT_EQ(0x94, this->op_->cur_op()) << "Failed at size " << i;
    TypeParam expected_value = 0;
    memcpy(&expected_value, &value, i);
    ASSERT_EQ(expected_value, this->op_->StackAt(0)) << "Failed at size " << i;
  }

  // Zero byte read.
  this->op_memory_.SetMemory(0, std::vector<uint8_t>{0x0a, 0x10, 0x20, 0x94, 0x00});
  ASSERT_FALSE(this->op_->Eval(0, 5));
  ASSERT_EQ(DWARF_ERROR_ILLEGAL_VALUE, this->op_->LastErrorCode());

  // Read too many bytes.
  this->op_memory_.SetMemory(0, std::vector<uint8_t>{0x0a, 0x10, 0x20, 0x94, sizeof(TypeParam) + 1});
  ASSERT_FALSE(this->op_->Eval(0, 5));
  ASSERT_EQ(DWARF_ERROR_ILLEGAL_VALUE, this->op_->LastErrorCode());

  // Force bad memory read.
  this->op_memory_.SetMemory(0, std::vector<uint8_t>{0x0a, 0x10, 0x40, 0x94, 0x01});
  ASSERT_FALSE(this->op_->Eval(0, 5));
  ASSERT_EQ(DWARF_ERROR_MEMORY_INVALID, this->op_->LastErrorCode());
  EXPECT_EQ(0x4010U, this->op_->LastErrorAddress());
}

TYPED_TEST_P(DwarfOpTest, const_unsigned) {
  std::vector<uint8_t> opcode_buffer = {
      // const1u
      0x08, 0x12, 0x08, 0xff,
      // const2u
      0x0a, 0x45, 0x12, 0x0a, 0x00, 0xff,
      // const4u
      0x0c, 0x12, 0x23, 0x34, 0x45, 0x0c, 0x03, 0x02, 0x01, 0xff,
      // const8u
      0x0e, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x0e, 0x87, 0x98, 0xa9, 0xba, 0xcb,
      0xdc, 0xed, 0xfe,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  // const1u
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x08, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0x12U, this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x08, this->op_->cur_op());
  ASSERT_EQ(2U, this->op_->StackSize());
  ASSERT_EQ(0xffU, this->op_->StackAt(0));

  // const2u
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x0a, this->op_->cur_op());
  ASSERT_EQ(3U, this->op_->StackSize());
  ASSERT_EQ(0x1245U, this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x0a, this->op_->cur_op());
  ASSERT_EQ(4U, this->op_->StackSize());
  ASSERT_EQ(0xff00U, this->op_->StackAt(0));

  // const4u
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x0c, this->op_->cur_op());
  ASSERT_EQ(5U, this->op_->StackSize());
  ASSERT_EQ(0x45342312U, this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x0c, this->op_->cur_op());
  ASSERT_EQ(6U, this->op_->StackSize());
  ASSERT_EQ(0xff010203U, this->op_->StackAt(0));

  // const8u
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x0e, this->op_->cur_op());
  ASSERT_EQ(7U, this->op_->StackSize());
  if (sizeof(TypeParam) == 4) {
    ASSERT_EQ(0x05060708U, this->op_->StackAt(0));
  } else {
    ASSERT_EQ(0x0102030405060708ULL, this->op_->StackAt(0));
  }

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x0e, this->op_->cur_op());
  ASSERT_EQ(8U, this->op_->StackSize());
  if (sizeof(TypeParam) == 4) {
    ASSERT_EQ(0xbaa99887UL, this->op_->StackAt(0));
  } else {
    ASSERT_EQ(0xfeeddccbbaa99887ULL, this->op_->StackAt(0));
  }
}

TYPED_TEST_P(DwarfOpTest, const_signed) {
  std::vector<uint8_t> opcode_buffer = {
      // const1s
      0x09, 0x12, 0x09, 0xff,
      // const2s
      0x0b, 0x21, 0x32, 0x0b, 0x08, 0xff,
      // const4s
      0x0d, 0x45, 0x34, 0x23, 0x12, 0x0d, 0x01, 0x02, 0x03, 0xff,
      // const8s
      0x0f, 0x89, 0x78, 0x67, 0x56, 0x45, 0x34, 0x23, 0x12, 0x0f, 0x04, 0x03, 0x02, 0x01, 0xef,
      0xef, 0xef, 0xff,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  // const1s
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x09, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0x12U, this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x09, this->op_->cur_op());
  ASSERT_EQ(2U, this->op_->StackSize());
  ASSERT_EQ(static_cast<TypeParam>(-1), this->op_->StackAt(0));

  // const2s
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x0b, this->op_->cur_op());
  ASSERT_EQ(3U, this->op_->StackSize());
  ASSERT_EQ(0x3221U, this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x0b, this->op_->cur_op());
  ASSERT_EQ(4U, this->op_->StackSize());
  ASSERT_EQ(static_cast<TypeParam>(-248), this->op_->StackAt(0));

  // const4s
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x0d, this->op_->cur_op());
  ASSERT_EQ(5U, this->op_->StackSize());
  ASSERT_EQ(0x12233445U, this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x0d, this->op_->cur_op());
  ASSERT_EQ(6U, this->op_->StackSize());
  ASSERT_EQ(static_cast<TypeParam>(-16580095), this->op_->StackAt(0));

  // const8s
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x0f, this->op_->cur_op());
  ASSERT_EQ(7U, this->op_->StackSize());
  if (sizeof(TypeParam) == 4) {
    ASSERT_EQ(0x56677889ULL, this->op_->StackAt(0));
  } else {
    ASSERT_EQ(0x1223344556677889ULL, this->op_->StackAt(0));
  }

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x0f, this->op_->cur_op());
  ASSERT_EQ(8U, this->op_->StackSize());
  if (sizeof(TypeParam) == 4) {
    ASSERT_EQ(0x01020304U, this->op_->StackAt(0));
  } else {
    ASSERT_EQ(static_cast<TypeParam>(-4521264810949884LL), this->op_->StackAt(0));
  }
}

TYPED_TEST_P(DwarfOpTest, const_uleb) {
  std::vector<uint8_t> opcode_buffer = {
      // Single byte ULEB128
      0x10, 0x22, 0x10, 0x7f,
      // Multi byte ULEB128
      0x10, 0xa2, 0x22, 0x10, 0xa2, 0x74, 0x10, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
      0x09, 0x10, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x79,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  // Single byte ULEB128
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x10, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0x22U, this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x10, this->op_->cur_op());
  ASSERT_EQ(2U, this->op_->StackSize());
  ASSERT_EQ(0x7fU, this->op_->StackAt(0));

  // Multi byte ULEB128
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x10, this->op_->cur_op());
  ASSERT_EQ(3U, this->op_->StackSize());
  ASSERT_EQ(0x1122U, this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x10, this->op_->cur_op());
  ASSERT_EQ(4U, this->op_->StackSize());
  ASSERT_EQ(0x3a22U, this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x10, this->op_->cur_op());
  ASSERT_EQ(5U, this->op_->StackSize());
  if (sizeof(TypeParam) == 4) {
    ASSERT_EQ(0x5080c101U, this->op_->StackAt(0));
  } else {
    ASSERT_EQ(0x9101c305080c101ULL, this->op_->StackAt(0));
  }

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x10, this->op_->cur_op());
  ASSERT_EQ(6U, this->op_->StackSize());
  if (sizeof(TypeParam) == 4) {
    ASSERT_EQ(0x5080c101U, this->op_->StackAt(0));
  } else {
    ASSERT_EQ(0x79101c305080c101ULL, this->op_->StackAt(0));
  }
}

TYPED_TEST_P(DwarfOpTest, const_sleb) {
  std::vector<uint8_t> opcode_buffer = {
      // Single byte SLEB128
      0x11, 0x22, 0x11, 0x7f,
      // Multi byte SLEB128
      0x11, 0xa2, 0x22, 0x11, 0xa2, 0x74, 0x11, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
      0x09, 0x11,
  };
  if (sizeof(TypeParam) == 4) {
    opcode_buffer.push_back(0xb8);
    opcode_buffer.push_back(0xd3);
    opcode_buffer.push_back(0x63);
  } else {
    opcode_buffer.push_back(0x81);
    opcode_buffer.push_back(0x82);
    opcode_buffer.push_back(0x83);
    opcode_buffer.push_back(0x84);
    opcode_buffer.push_back(0x85);
    opcode_buffer.push_back(0x86);
    opcode_buffer.push_back(0x87);
    opcode_buffer.push_back(0x88);
    opcode_buffer.push_back(0x79);
  }
  this->op_memory_.SetMemory(0, opcode_buffer);

  // Single byte SLEB128
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x11, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0x22U, this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x11, this->op_->cur_op());
  ASSERT_EQ(2U, this->op_->StackSize());
  ASSERT_EQ(static_cast<TypeParam>(-1), this->op_->StackAt(0));

  // Multi byte SLEB128
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x11, this->op_->cur_op());
  ASSERT_EQ(3U, this->op_->StackSize());
  ASSERT_EQ(0x1122U, this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x11, this->op_->cur_op());
  ASSERT_EQ(4U, this->op_->StackSize());
  ASSERT_EQ(static_cast<TypeParam>(-1502), this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x11, this->op_->cur_op());
  ASSERT_EQ(5U, this->op_->StackSize());
  if (sizeof(TypeParam) == 4) {
    ASSERT_EQ(0x5080c101U, this->op_->StackAt(0));
  } else {
    ASSERT_EQ(0x9101c305080c101ULL, this->op_->StackAt(0));
  }

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x11, this->op_->cur_op());
  ASSERT_EQ(6U, this->op_->StackSize());
  if (sizeof(TypeParam) == 4) {
    ASSERT_EQ(static_cast<TypeParam>(-464456), this->op_->StackAt(0));
  } else {
    ASSERT_EQ(static_cast<TypeParam>(-499868564803501823LL), this->op_->StackAt(0));
  }
}

TYPED_TEST_P(DwarfOpTest, op_dup) {
  std::vector<uint8_t> opcode_buffer = {
      // Should fail since nothing is on the stack.
      0x12,
      // Push on a value and dup.
      0x08, 0x15, 0x12,
      // Do it again.
      0x08, 0x23, 0x12,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(0x12, this->op_->cur_op());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x12, this->op_->cur_op());
  ASSERT_EQ(2U, this->op_->StackSize());
  ASSERT_EQ(0x15U, this->op_->StackAt(0));
  ASSERT_EQ(0x15U, this->op_->StackAt(1));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(3U, this->op_->StackSize());
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x12, this->op_->cur_op());
  ASSERT_EQ(4U, this->op_->StackSize());
  ASSERT_EQ(0x23U, this->op_->StackAt(0));
  ASSERT_EQ(0x23U, this->op_->StackAt(1));
  ASSERT_EQ(0x15U, this->op_->StackAt(2));
  ASSERT_EQ(0x15U, this->op_->StackAt(3));
}

TYPED_TEST_P(DwarfOpTest, op_drop) {
  std::vector<uint8_t> opcode_buffer = {
      // Push a couple of values.
      0x08, 0x10, 0x08, 0x20,
      // Drop the values.
      0x13, 0x13,
      // Attempt to drop empty stack.
      0x13,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x13, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0x10U, this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x13, this->op_->cur_op());
  ASSERT_EQ(0U, this->op_->StackSize());

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(0x13, this->op_->cur_op());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());
}

TYPED_TEST_P(DwarfOpTest, op_over) {
  std::vector<uint8_t> opcode_buffer = {
      // Push a couple of values.
      0x08, 0x1a, 0x08, 0xed,
      // Copy a value.
      0x14,
      // Remove all but one element.
      0x13, 0x13,
      // Provoke a failure with this opcode.
      0x14,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x14, this->op_->cur_op());
  ASSERT_EQ(3U, this->op_->StackSize());
  ASSERT_EQ(0x1aU, this->op_->StackAt(0));
  ASSERT_EQ(0xedU, this->op_->StackAt(1));
  ASSERT_EQ(0x1aU, this->op_->StackAt(2));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(0x14, this->op_->cur_op());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());
}

TYPED_TEST_P(DwarfOpTest, op_pick) {
  std::vector<uint8_t> opcode_buffer = {
      // Push a few values.
      0x08, 0x1a, 0x08, 0xed, 0x08, 0x34,
      // Copy the value at offset 2.
      0x15, 0x01,
      // Copy the last value in the stack.
      0x15, 0x03,
      // Choose an invalid index.
      0x15, 0x10,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(3U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x15, this->op_->cur_op());
  ASSERT_EQ(4U, this->op_->StackSize());
  ASSERT_EQ(0xedU, this->op_->StackAt(0));
  ASSERT_EQ(0x34U, this->op_->StackAt(1));
  ASSERT_EQ(0xedU, this->op_->StackAt(2));
  ASSERT_EQ(0x1aU, this->op_->StackAt(3));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x15, this->op_->cur_op());
  ASSERT_EQ(5U, this->op_->StackSize());
  ASSERT_EQ(0x1aU, this->op_->StackAt(0));
  ASSERT_EQ(0xedU, this->op_->StackAt(1));
  ASSERT_EQ(0x34U, this->op_->StackAt(2));
  ASSERT_EQ(0xedU, this->op_->StackAt(3));
  ASSERT_EQ(0x1aU, this->op_->StackAt(4));

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(0x15, this->op_->cur_op());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());
}

TYPED_TEST_P(DwarfOpTest, op_swap) {
  std::vector<uint8_t> opcode_buffer = {
      // Push a couple of values.
      0x08, 0x26, 0x08, 0xab,
      // Swap values.
      0x16,
      // Pop a value to cause a failure.
      0x13, 0x16,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());
  ASSERT_EQ(0xabU, this->op_->StackAt(0));
  ASSERT_EQ(0x26U, this->op_->StackAt(1));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x16, this->op_->cur_op());
  ASSERT_EQ(2U, this->op_->StackSize());
  ASSERT_EQ(0x26U, this->op_->StackAt(0));
  ASSERT_EQ(0xabU, this->op_->StackAt(1));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(0x16, this->op_->cur_op());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());
}

TYPED_TEST_P(DwarfOpTest, op_rot) {
  std::vector<uint8_t> opcode_buffer = {
      // Rotate that should cause a failure.
      0x17, 0x08, 0x10,
      // Only 1 value on stack, should fail.
      0x17, 0x08, 0x20,
      // Only 2 values on stack, should fail.
      0x17, 0x08, 0x30,
      // Should rotate properly.
      0x17,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(3U, this->op_->StackSize());
  ASSERT_EQ(0x30U, this->op_->StackAt(0));
  ASSERT_EQ(0x20U, this->op_->StackAt(1));
  ASSERT_EQ(0x10U, this->op_->StackAt(2));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x17, this->op_->cur_op());
  ASSERT_EQ(3U, this->op_->StackSize());
  ASSERT_EQ(0x20U, this->op_->StackAt(0));
  ASSERT_EQ(0x10U, this->op_->StackAt(1));
  ASSERT_EQ(0x30U, this->op_->StackAt(2));
}

TYPED_TEST_P(DwarfOpTest, op_abs) {
  std::vector<uint8_t> opcode_buffer = {
      // Abs that should fail.
      0x19,
      // A value that is already positive.
      0x08, 0x10, 0x19,
      // A value that is negative.
      0x11, 0x7f, 0x19,
      // A value that is large and negative.
      0x11, 0x81, 0x80, 0x80, 0x80,
  };
  if (sizeof(TypeParam) == 4) {
    opcode_buffer.push_back(0x08);
  } else {
    opcode_buffer.push_back(0x80);
    opcode_buffer.push_back(0x80);
    opcode_buffer.push_back(0x01);
  }
  opcode_buffer.push_back(0x19);
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0x10U, this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x19, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0x10U, this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x19, this->op_->cur_op());
  ASSERT_EQ(2U, this->op_->StackSize());
  ASSERT_EQ(0x1U, this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(3U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x19, this->op_->cur_op());
  ASSERT_EQ(3U, this->op_->StackSize());
  if (sizeof(TypeParam) == 4) {
    ASSERT_EQ(2147483647U, this->op_->StackAt(0));
  } else {
    ASSERT_EQ(4398046511105UL, this->op_->StackAt(0));
  }
}

TYPED_TEST_P(DwarfOpTest, op_and) {
  std::vector<uint8_t> opcode_buffer = {
      // No stack, and op will fail.
      0x1b,
      // Push a single value.
      0x08, 0x20,
      // One element stack, and op will fail.
      0x1b,
      // Push another value.
      0x08, 0x02, 0x1b,
      // Push on two negative values.
      0x11, 0x7c, 0x11, 0x7f, 0x1b,
      // Push one negative, one positive.
      0x11, 0x10, 0x11, 0x7c, 0x1b,
      // Divide by zero.
      0x11, 0x10, 0x11, 0x00, 0x1b,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  // Two positive values.
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x1b, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0x10U, this->op_->StackAt(0));

  // Two negative values.
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(3U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x1b, this->op_->cur_op());
  ASSERT_EQ(2U, this->op_->StackSize());
  ASSERT_EQ(0x04U, this->op_->StackAt(0));

  // One negative value, one positive value.
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(3U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(4U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x1b, this->op_->cur_op());
  ASSERT_EQ(3U, this->op_->StackSize());
  ASSERT_EQ(static_cast<TypeParam>(-4), this->op_->StackAt(0));

  // Divide by zero.
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(4U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(5U, this->op_->StackSize());

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_ILLEGAL_VALUE, this->op_->LastErrorCode());
}

TYPED_TEST_P(DwarfOpTest, op_div) {
  std::vector<uint8_t> opcode_buffer = {
      // No stack, and op will fail.
      0x1a,
      // Push a single value.
      0x08, 0x48,
      // One element stack, and op will fail.
      0x1a,
      // Push another value.
      0x08, 0xf0, 0x1a,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x1a, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0x40U, this->op_->StackAt(0));
}

TYPED_TEST_P(DwarfOpTest, op_minus) {
  std::vector<uint8_t> opcode_buffer = {
      // No stack, and op will fail.
      0x1c,
      // Push a single value.
      0x08, 0x48,
      // One element stack, and op will fail.
      0x1c,
      // Push another value.
      0x08, 0x04, 0x1c,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x1c, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0x44U, this->op_->StackAt(0));
}

TYPED_TEST_P(DwarfOpTest, op_mod) {
  std::vector<uint8_t> opcode_buffer = {
      // No stack, and op will fail.
      0x1d,
      // Push a single value.
      0x08, 0x47,
      // One element stack, and op will fail.
      0x1d,
      // Push another value.
      0x08, 0x04, 0x1d,
      // Try a mod of zero.
      0x08, 0x01, 0x08, 0x00, 0x1d,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x1d, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0x03U, this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(3U, this->op_->StackSize());

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_ILLEGAL_VALUE, this->op_->LastErrorCode());
}

TYPED_TEST_P(DwarfOpTest, op_mul) {
  std::vector<uint8_t> opcode_buffer = {
      // No stack, and op will fail.
      0x1e,
      // Push a single value.
      0x08, 0x48,
      // One element stack, and op will fail.
      0x1e,
      // Push another value.
      0x08, 0x04, 0x1e,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x1e, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0x120U, this->op_->StackAt(0));
}

TYPED_TEST_P(DwarfOpTest, op_neg) {
  std::vector<uint8_t> opcode_buffer = {
      // No stack, and op will fail.
      0x1f,
      // Push a single value.
      0x08, 0x48, 0x1f,
      // Push a negative value.
      0x11, 0x7f, 0x1f,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x1f, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(static_cast<TypeParam>(-72), this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x1f, this->op_->cur_op());
  ASSERT_EQ(2U, this->op_->StackSize());
  ASSERT_EQ(0x01U, this->op_->StackAt(0));
}

TYPED_TEST_P(DwarfOpTest, op_not) {
  std::vector<uint8_t> opcode_buffer = {
      // No stack, and op will fail.
      0x20,
      // Push a single value.
      0x08, 0x4, 0x20,
      // Push a negative value.
      0x11, 0x7c, 0x20,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x20, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(static_cast<TypeParam>(-5), this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x20, this->op_->cur_op());
  ASSERT_EQ(2U, this->op_->StackSize());
  ASSERT_EQ(0x03U, this->op_->StackAt(0));
}

TYPED_TEST_P(DwarfOpTest, op_or) {
  std::vector<uint8_t> opcode_buffer = {
      // No stack, and op will fail.
      0x21,
      // Push a single value.
      0x08, 0x48,
      // One element stack, and op will fail.
      0x21,
      // Push another value.
      0x08, 0xf4, 0x21,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x21, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0xfcU, this->op_->StackAt(0));
}

TYPED_TEST_P(DwarfOpTest, op_plus) {
  std::vector<uint8_t> opcode_buffer = {
      // No stack, and op will fail.
      0x22,
      // Push a single value.
      0x08, 0xff,
      // One element stack, and op will fail.
      0x22,
      // Push another value.
      0x08, 0xf2, 0x22,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x22, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0x1f1U, this->op_->StackAt(0));
}

TYPED_TEST_P(DwarfOpTest, op_plus_uconst) {
  std::vector<uint8_t> opcode_buffer = {
      // No stack, and op will fail.
      0x23,
      // Push a single value.
      0x08, 0x50, 0x23, 0x80, 0x51,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x23, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0x28d0U, this->op_->StackAt(0));
}

TYPED_TEST_P(DwarfOpTest, op_shl) {
  std::vector<uint8_t> opcode_buffer = {
      // No stack, and op will fail.
      0x24,
      // Push a single value.
      0x08, 0x67,
      // One element stack, and op will fail.
      0x24,
      // Push another value.
      0x08, 0x03, 0x24,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x24, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0x338U, this->op_->StackAt(0));
}

TYPED_TEST_P(DwarfOpTest, op_shr) {
  std::vector<uint8_t> opcode_buffer = {
      // No stack, and op will fail.
      0x25,
      // Push a single value.
      0x11, 0x70,
      // One element stack, and op will fail.
      0x25,
      // Push another value.
      0x08, 0x03, 0x25,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x25, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  if (sizeof(TypeParam) == 4) {
    ASSERT_EQ(0x1ffffffeU, this->op_->StackAt(0));
  } else {
    ASSERT_EQ(0x1ffffffffffffffeULL, this->op_->StackAt(0));
  }
}

TYPED_TEST_P(DwarfOpTest, op_shra) {
  std::vector<uint8_t> opcode_buffer = {
      // No stack, and op will fail.
      0x26,
      // Push a single value.
      0x11, 0x70,
      // One element stack, and op will fail.
      0x26,
      // Push another value.
      0x08, 0x03, 0x26,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x26, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(static_cast<TypeParam>(-2), this->op_->StackAt(0));
}

TYPED_TEST_P(DwarfOpTest, op_xor) {
  std::vector<uint8_t> opcode_buffer = {
      // No stack, and op will fail.
      0x27,
      // Push a single value.
      0x08, 0x11,
      // One element stack, and op will fail.
      0x27,
      // Push another value.
      0x08, 0x41, 0x27,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(2U, this->op_->StackSize());

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x27, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0x50U, this->op_->StackAt(0));
}

TYPED_TEST_P(DwarfOpTest, op_bra) {
  std::vector<uint8_t> opcode_buffer = {
      // No stack, and op will fail.
      0x28,
      // Push on a non-zero value with a positive branch.
      0x08, 0x11, 0x28, 0x02, 0x01,
      // Push on a zero value with a positive branch.
      0x08, 0x00, 0x28, 0x05, 0x00,
      // Push on a non-zero value with a negative branch.
      0x08, 0x11, 0x28, 0xfc, 0xff,
      // Push on a zero value with a negative branch.
      0x08, 0x00, 0x28, 0xf0, 0xff,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_FALSE(this->op_->Decode());
  ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

  // Push on a non-zero value with a positive branch.
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  uint64_t offset = this->mem_->cur_offset() + 3;
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x28, this->op_->cur_op());
  ASSERT_EQ(0U, this->op_->StackSize());
  ASSERT_EQ(offset + 0x102, this->mem_->cur_offset());

  // Push on a zero value with a positive branch.
  this->mem_->set_cur_offset(offset);
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  offset = this->mem_->cur_offset() + 3;
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x28, this->op_->cur_op());
  ASSERT_EQ(0U, this->op_->StackSize());
  ASSERT_EQ(offset - 5, this->mem_->cur_offset());

  // Push on a non-zero value with a negative branch.
  this->mem_->set_cur_offset(offset);
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  offset = this->mem_->cur_offset() + 3;
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x28, this->op_->cur_op());
  ASSERT_EQ(0U, this->op_->StackSize());
  ASSERT_EQ(offset - 4, this->mem_->cur_offset());

  // Push on a zero value with a negative branch.
  this->mem_->set_cur_offset(offset);
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(1U, this->op_->StackSize());

  offset = this->mem_->cur_offset() + 3;
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x28, this->op_->cur_op());
  ASSERT_EQ(0U, this->op_->StackSize());
  ASSERT_EQ(offset + 16, this->mem_->cur_offset());
}

TYPED_TEST_P(DwarfOpTest, compare_opcode_stack_error) {
  // All of the ops require two stack elements. Loop through all of these
  // ops with potential errors.
  std::vector<uint8_t> opcode_buffer = {
      0xff,  // Place holder for compare op.
      0x08, 0x11,
      0xff,  // Place holder for compare op.
  };

  for (uint8_t opcode = 0x29; opcode <= 0x2e; opcode++) {
    opcode_buffer[0] = opcode;
    opcode_buffer[3] = opcode;
    this->op_memory_.SetMemory(0, opcode_buffer);

    ASSERT_FALSE(this->op_->Eval(0, 1));
    ASSERT_EQ(opcode, this->op_->cur_op());
    ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());

    ASSERT_FALSE(this->op_->Eval(1, 4));
    ASSERT_EQ(opcode, this->op_->cur_op());
    ASSERT_EQ(1U, this->op_->StackSize());
    ASSERT_EQ(DWARF_ERROR_STACK_INDEX_NOT_VALID, this->op_->LastErrorCode());
  }
}

TYPED_TEST_P(DwarfOpTest, compare_opcodes) {
  // Have three different checks for each compare op:
  //   - Both values the same.
  //   - The first value larger than the second.
  //   - The second value larger than the first.
  std::vector<uint8_t> opcode_buffer = {
      // Values the same.
      0x08, 0x11, 0x08, 0x11,
      0xff,  // Placeholder.
      // First value larger.
      0x08, 0x12, 0x08, 0x10,
      0xff,  // Placeholder.
      // Second value larger.
      0x08, 0x10, 0x08, 0x12,
      0xff,  // Placeholder.
  };

  // Opcode followed by the expected values on the stack.
  std::vector<uint8_t> expected = {
      0x29, 1, 0, 0,  // eq
      0x2a, 1, 1, 0,  // ge
      0x2b, 0, 1, 0,  // gt
      0x2c, 1, 0, 1,  // le
      0x2d, 0, 0, 1,  // lt
      0x2e, 0, 1, 1,  // ne
  };
  for (size_t i = 0; i < expected.size(); i += 4) {
    opcode_buffer[4] = expected[i];
    opcode_buffer[9] = expected[i];
    opcode_buffer[14] = expected[i];
    this->op_memory_.SetMemory(0, opcode_buffer);

    ASSERT_TRUE(this->op_->Eval(0, 15))
        << "Op: 0x" << std::hex << static_cast<uint32_t>(expected[i]) << " failed";

    ASSERT_EQ(3U, this->op_->StackSize());
    ASSERT_EQ(expected[i + 1], this->op_->StackAt(2));
    ASSERT_EQ(expected[i + 2], this->op_->StackAt(1));
    ASSERT_EQ(expected[i + 3], this->op_->StackAt(0));
  }
}

TYPED_TEST_P(DwarfOpTest, op_skip) {
  std::vector<uint8_t> opcode_buffer = {
      // Positive value.
      0x2f, 0x10, 0x20,
      // Negative value.
      0x2f, 0xfd, 0xff,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  uint64_t offset = this->mem_->cur_offset() + 3;
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x2f, this->op_->cur_op());
  ASSERT_EQ(0U, this->op_->StackSize());
  ASSERT_EQ(offset + 0x2010, this->mem_->cur_offset());

  this->mem_->set_cur_offset(offset);
  offset = this->mem_->cur_offset() + 3;
  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x2f, this->op_->cur_op());
  ASSERT_EQ(0U, this->op_->StackSize());
  ASSERT_EQ(offset - 3, this->mem_->cur_offset());
}

TYPED_TEST_P(DwarfOpTest, op_lit) {
  std::vector<uint8_t> opcode_buffer;

  // Verify every lit opcode.
  for (uint8_t op = 0x30; op <= 0x4f; op++) {
    opcode_buffer.push_back(op);
  }
  this->op_memory_.SetMemory(0, opcode_buffer);

  for (size_t i = 0; i < opcode_buffer.size(); i++) {
    uint32_t op = opcode_buffer[i];
    ASSERT_TRUE(this->op_->Eval(i, i + 1)) << "Failed op: 0x" << std::hex << op;
    ASSERT_EQ(op, this->op_->cur_op());
    ASSERT_EQ(1U, this->op_->StackSize()) << "Failed op: 0x" << std::hex << op;
    ASSERT_EQ(op - 0x30U, this->op_->StackAt(0)) << "Failed op: 0x" << std::hex << op;
  }
}

TYPED_TEST_P(DwarfOpTest, op_reg) {
  std::vector<uint8_t> opcode_buffer;

  // Verify every reg opcode.
  for (uint8_t op = 0x50; op <= 0x6f; op++) {
    opcode_buffer.push_back(op);
  }
  this->op_memory_.SetMemory(0, opcode_buffer);

  for (size_t i = 0; i < opcode_buffer.size(); i++) {
    uint32_t op = opcode_buffer[i];
    ASSERT_TRUE(this->op_->Eval(i, i + 1)) << "Failed op: 0x" << std::hex << op;
    ASSERT_EQ(op, this->op_->cur_op());
    ASSERT_TRUE(this->op_->is_register()) << "Failed op: 0x" << std::hex << op;
    ASSERT_EQ(1U, this->op_->StackSize()) << "Failed op: 0x" << std::hex << op;
    ASSERT_EQ(op - 0x50U, this->op_->StackAt(0)) << "Failed op: 0x" << std::hex << op;
  }
}

TYPED_TEST_P(DwarfOpTest, op_regx) {
  std::vector<uint8_t> opcode_buffer = {
      0x90, 0x02, 0x90, 0x80, 0x15,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  ASSERT_TRUE(this->op_->Eval(0, 2));
  ASSERT_EQ(0x90, this->op_->cur_op());
  ASSERT_TRUE(this->op_->is_register());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0x02U, this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Eval(2, 5));
  ASSERT_EQ(0x90, this->op_->cur_op());
  ASSERT_TRUE(this->op_->is_register());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0xa80U, this->op_->StackAt(0));
}

TYPED_TEST_P(DwarfOpTest, op_breg) {
  std::vector<uint8_t> opcode_buffer;

  // Verify every reg opcode.
  for (uint8_t op = 0x70; op <= 0x8f; op++) {
    // Positive value added to register.
    opcode_buffer.push_back(op);
    opcode_buffer.push_back(0x12);
    // Negative value added to register.
    opcode_buffer.push_back(op);
    opcode_buffer.push_back(0x7e);
  }
  this->op_memory_.SetMemory(0, opcode_buffer);

  RegsImplFake<TypeParam> regs(32);
  for (size_t i = 0; i < 32; i++) {
    regs[i] = i + 10;
  }
  RegsInfo<TypeParam> regs_info(&regs);
  this->op_->set_regs_info(&regs_info);

  uint64_t offset = 0;
  for (uint32_t op = 0x70; op <= 0x8f; op++) {
    // Positive value added to register.
    ASSERT_TRUE(this->op_->Eval(offset, offset + 2)) << "Failed op: 0x" << std::hex << op;
    ASSERT_EQ(op, this->op_->cur_op());
    ASSERT_EQ(1U, this->op_->StackSize()) << "Failed op: 0x" << std::hex << op;
    ASSERT_EQ(op - 0x70 + 10 + 0x12, this->op_->StackAt(0)) << "Failed op: 0x" << std::hex << op;
    offset += 2;

    // Negative value added to register.
    ASSERT_TRUE(this->op_->Eval(offset, offset + 2)) << "Failed op: 0x" << std::hex << op;
    ASSERT_EQ(op, this->op_->cur_op());
    ASSERT_EQ(1U, this->op_->StackSize()) << "Failed op: 0x" << std::hex << op;
    ASSERT_EQ(op - 0x70 + 10 - 2, this->op_->StackAt(0)) << "Failed op: 0x" << std::hex << op;
    offset += 2;
  }
}

TYPED_TEST_P(DwarfOpTest, op_breg_invalid_register) {
  std::vector<uint8_t> opcode_buffer = {
      0x7f, 0x12, 0x80, 0x12,
  };
  this->op_memory_.SetMemory(0, opcode_buffer);

  RegsImplFake<TypeParam> regs(16);
  for (size_t i = 0; i < 16; i++) {
    regs[i] = i + 10;
  }
  RegsInfo<TypeParam> regs_info(&regs);
  this->op_->set_regs_info(&regs_info);

  // Should pass since this references the last regsister.
  ASSERT_TRUE(this->op_->Eval(0, 2));
  ASSERT_EQ(0x7fU, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0x2bU, this->op_->StackAt(0));

  // Should fail since this references a non-existent register.
  ASSERT_FALSE(this->op_->Eval(2, 4));
  ASSERT_EQ(DWARF_ERROR_ILLEGAL_VALUE, this->op_->LastErrorCode());
}

TYPED_TEST_P(DwarfOpTest, op_bregx) {
  std::vector<uint8_t> opcode_buffer = {// Positive value added to register.
                                        0x92, 0x05, 0x20,
                                        // Negative value added to register.
                                        0x92, 0x06, 0x80, 0x7e,
                                        // Illegal register.
                                        0x92, 0x80, 0x15, 0x80, 0x02};
  this->op_memory_.SetMemory(0, opcode_buffer);

  RegsImplFake<TypeParam> regs(10);
  regs[5] = 0x45;
  regs[6] = 0x190;
  RegsInfo<TypeParam> regs_info(&regs);
  this->op_->set_regs_info(&regs_info);

  ASSERT_TRUE(this->op_->Eval(0, 3));
  ASSERT_EQ(0x92, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0x65U, this->op_->StackAt(0));

  ASSERT_TRUE(this->op_->Eval(3, 7));
  ASSERT_EQ(0x92, this->op_->cur_op());
  ASSERT_EQ(1U, this->op_->StackSize());
  ASSERT_EQ(0x90U, this->op_->StackAt(0));

  ASSERT_FALSE(this->op_->Eval(7, 12));
  ASSERT_EQ(DWARF_ERROR_ILLEGAL_VALUE, this->op_->LastErrorCode());
}

TYPED_TEST_P(DwarfOpTest, op_nop) {
  this->op_memory_.SetMemory(0, std::vector<uint8_t>{0x96});

  ASSERT_TRUE(this->op_->Decode());
  ASSERT_EQ(0x96, this->op_->cur_op());
  ASSERT_EQ(0U, this->op_->StackSize());
}

TYPED_TEST_P(DwarfOpTest, is_dex_pc) {
  // Special sequence that indicates this is a dex pc.
  this->op_memory_.SetMemory(0, std::vector<uint8_t>{0x0c, 'D', 'E', 'X', '1', 0x13});

  ASSERT_TRUE(this->op_->Eval(0, 6));
  EXPECT_TRUE(this->op_->dex_pc_set());

  // Try without the last op.
  ASSERT_TRUE(this->op_->Eval(0, 5));
  EXPECT_FALSE(this->op_->dex_pc_set());

  // Change the constant.
  this->op_memory_.SetMemory(0, std::vector<uint8_t>{0x0c, 'D', 'E', 'X', '2', 0x13});
  ASSERT_TRUE(this->op_->Eval(0, 6));
  EXPECT_FALSE(this->op_->dex_pc_set());
}

REGISTER_TYPED_TEST_CASE_P(DwarfOpTest, decode, eval, illegal_opcode, not_implemented, op_addr,
                           op_deref, op_deref_size, const_unsigned, const_signed, const_uleb,
                           const_sleb, op_dup, op_drop, op_over, op_pick, op_swap, op_rot, op_abs,
                           op_and, op_div, op_minus, op_mod, op_mul, op_neg, op_not, op_or, op_plus,
                           op_plus_uconst, op_shl, op_shr, op_shra, op_xor, op_bra,
                           compare_opcode_stack_error, compare_opcodes, op_skip, op_lit, op_reg,
                           op_regx, op_breg, op_breg_invalid_register, op_bregx, op_nop, is_dex_pc);

typedef ::testing::Types<uint32_t, uint64_t> DwarfOpTestTypes;
INSTANTIATE_TYPED_TEST_CASE_P(, DwarfOpTest, DwarfOpTestTypes);

}  // namespace unwindstack
