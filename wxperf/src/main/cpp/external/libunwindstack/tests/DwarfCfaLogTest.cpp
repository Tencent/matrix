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

#include <memory>
#include <type_traits>
#include <unordered_map>

#include <android-base/stringprintf.h>
#include <gtest/gtest.h>

#include <unwindstack/DwarfLocation.h>
#include <unwindstack/DwarfMemory.h>
#include <unwindstack/DwarfStructs.h>
#include <unwindstack/Log.h>

#include "DwarfCfa.h"

#include "LogFake.h"
#include "MemoryFake.h"

namespace unwindstack {

template <typename TypeParam>
class DwarfCfaLogTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ResetLogs();
    memory_.Clear();

    dmem_.reset(new DwarfMemory(&memory_));

    cie_.cfa_instructions_offset = 0x1000;
    cie_.cfa_instructions_end = 0x1030;
    // These two values should be different to distinguish between
    // operations that deal with code versus data.
    cie_.code_alignment_factor = 4;
    cie_.data_alignment_factor = 8;

    fde_.cfa_instructions_offset = 0x2000;
    fde_.cfa_instructions_end = 0x2030;
    fde_.pc_start = 0x2000;
    fde_.pc_end = 0x2000;
    fde_.pc_end = 0x10000;
    fde_.cie = &cie_;
    cfa_.reset(new DwarfCfa<TypeParam>(dmem_.get(), &fde_));
  }

  MemoryFake memory_;
  std::unique_ptr<DwarfMemory> dmem_;
  std::unique_ptr<DwarfCfa<TypeParam>> cfa_;
  DwarfCie cie_;
  DwarfFde fde_;
};
TYPED_TEST_CASE_P(DwarfCfaLogTest);

// NOTE: All class variable references have to be prefaced with this->.

TYPED_TEST_P(DwarfCfaLogTest, cfa_illegal) {
  for (uint8_t i = 0x17; i < 0x3f; i++) {
    if (i == 0x2e || i == 0x2f) {
      // Skip gnu extension ops.
      continue;
    }
    this->memory_.SetMemory(0x2000, std::vector<uint8_t>{i});

    ResetLogs();
    ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x2000, 0x2001));
    std::string expected = "4 unwind Illegal\n";
    expected += android::base::StringPrintf("4 unwind Raw Data: 0x%02x\n", i);
    ASSERT_EQ(expected, GetFakeLogPrint());
    ASSERT_EQ("", GetFakeLogBuf());
  }
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_nop) {
  this->memory_.SetMemory(0x2000, std::vector<uint8_t>{0x00});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x2000, 0x2001));
  std::string expected =
      "4 unwind DW_CFA_nop\n"
      "4 unwind Raw Data: 0x00\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_offset) {
  this->memory_.SetMemory(0x2000, std::vector<uint8_t>{0x83, 0x04});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x2000, 0x2002));
  std::string expected =
      "4 unwind DW_CFA_offset register(3) 4\n"
      "4 unwind Raw Data: 0x83 0x04\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  this->memory_.SetMemory(0x2100, std::vector<uint8_t>{0x83, 0x84, 0x01});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x2100, 0x2103));
  expected =
      "4 unwind DW_CFA_offset register(3) 132\n"
      "4 unwind Raw Data: 0x83 0x84 0x01\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_offset_extended) {
  this->memory_.SetMemory(0x500, std::vector<uint8_t>{0x05, 0x03, 0x02});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x500, 0x503));
  std::string expected =
      "4 unwind DW_CFA_offset_extended register(3) 2\n"
      "4 unwind Raw Data: 0x05 0x03 0x02\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  this->memory_.SetMemory(0x1500, std::vector<uint8_t>{0x05, 0x81, 0x01, 0x82, 0x12});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x1500, 0x1505));
  expected =
      "4 unwind DW_CFA_offset_extended register(129) 2306\n"
      "4 unwind Raw Data: 0x05 0x81 0x01 0x82 0x12\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_offset_extended_sf) {
  this->memory_.SetMemory(0x500, std::vector<uint8_t>{0x11, 0x05, 0x10});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x500, 0x503));
  std::string expected =
      "4 unwind DW_CFA_offset_extended_sf register(5) 16\n"
      "4 unwind Raw Data: 0x11 0x05 0x10\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  // Check a negative value for the offset.
  ResetLogs();
  this->memory_.SetMemory(0x1500, std::vector<uint8_t>{0x11, 0x86, 0x01, 0xff, 0x7f});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x1500, 0x1505));
  expected =
      "4 unwind DW_CFA_offset_extended_sf register(134) -1\n"
      "4 unwind Raw Data: 0x11 0x86 0x01 0xff 0x7f\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_restore) {
  this->memory_.SetMemory(0x2000, std::vector<uint8_t>{0xc2});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x2000, 0x2001));
  std::string expected =
      "4 unwind DW_CFA_restore register(2)\n"
      "4 unwind Raw Data: 0xc2\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  this->memory_.SetMemory(0x3000, std::vector<uint8_t>{0x82, 0x04, 0xc2});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x3000, 0x3003));
  expected =
      "4 unwind DW_CFA_offset register(2) 4\n"
      "4 unwind Raw Data: 0x82 0x04\n"
      "4 unwind DW_CFA_restore register(2)\n"
      "4 unwind Raw Data: 0xc2\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_restore_extended) {
  this->memory_.SetMemory(0x4000, std::vector<uint8_t>{0x06, 0x08});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x4000, 0x4002));
  std::string expected =
      "4 unwind DW_CFA_restore_extended register(8)\n"
      "4 unwind Raw Data: 0x06 0x08\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  this->memory_.SetMemory(0x5000, std::vector<uint8_t>{0x05, 0x82, 0x02, 0x04, 0x06, 0x82, 0x02});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x5000, 0x5007));
  expected =
      "4 unwind DW_CFA_offset_extended register(258) 4\n"
      "4 unwind Raw Data: 0x05 0x82 0x02 0x04\n"
      "4 unwind DW_CFA_restore_extended register(258)\n"
      "4 unwind Raw Data: 0x06 0x82 0x02\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_set_loc) {
  uint8_t buffer[1 + sizeof(TypeParam)];
  buffer[0] = 0x1;
  TypeParam address;
  std::string raw_data("Raw Data: 0x01 ");
  std::string address_str;
  if (std::is_same<TypeParam, uint32_t>::value) {
    address = 0x81234578U;
    address_str = "0x81234578";
    raw_data += "0x78 0x45 0x23 0x81";
  } else {
    address = 0x8123456712345678ULL;
    address_str = "0x8123456712345678";
    raw_data += "0x78 0x56 0x34 0x12 0x67 0x45 0x23 0x81";
  }
  memcpy(&buffer[1], &address, sizeof(address));

  this->memory_.SetMemory(0x50, buffer, sizeof(buffer));
  ResetLogs();

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x50, 0x51 + sizeof(TypeParam)));
  std::string expected = "4 unwind DW_CFA_set_loc " + address_str + "\n";
  expected += "4 unwind " + raw_data + "\n";
  expected += "4 unwind \n";
  expected += "4 unwind PC " + address_str + "\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  // Check for a set going back.
  ResetLogs();
  this->fde_.pc_start = address + 0x10;

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x50, 0x51 + sizeof(TypeParam)));
  expected = "4 unwind DW_CFA_set_loc " + address_str + "\n";
  expected += "4 unwind " + raw_data + "\n";
  expected += "4 unwind \n";
  expected += "4 unwind PC " + address_str + "\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_advance_loc) {
  this->memory_.SetMemory(0x200, std::vector<uint8_t>{0x44});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x200, 0x201));
  std::string expected =
      "4 unwind DW_CFA_advance_loc 4\n"
      "4 unwind Raw Data: 0x44\n"
      "4 unwind \n"
      "4 unwind PC 0x2010\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0x100, 0x200, 0x201));
  expected =
      "4 unwind DW_CFA_advance_loc 4\n"
      "4 unwind Raw Data: 0x44\n"
      "4 unwind \n"
      "4 unwind PC 0x2110\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_advance_loc1) {
  this->memory_.SetMemory(0x200, std::vector<uint8_t>{0x02, 0x04});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x200, 0x202));
  std::string expected =
      "4 unwind DW_CFA_advance_loc1 4\n"
      "4 unwind Raw Data: 0x02 0x04\n"
      "4 unwind \n"
      "4 unwind PC 0x2004\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0x10, 0x200, 0x202));
  expected =
      "4 unwind DW_CFA_advance_loc1 4\n"
      "4 unwind Raw Data: 0x02 0x04\n"
      "4 unwind \n"
      "4 unwind PC 0x2014\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_advance_loc2) {
  this->memory_.SetMemory(0x600, std::vector<uint8_t>{0x03, 0x04, 0x03});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x600, 0x603));
  std::string expected =
      "4 unwind DW_CFA_advance_loc2 772\n"
      "4 unwind Raw Data: 0x03 0x04 0x03\n"
      "4 unwind \n"
      "4 unwind PC 0x2304\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0x1000, 0x600, 0x603));
  expected =
      "4 unwind DW_CFA_advance_loc2 772\n"
      "4 unwind Raw Data: 0x03 0x04 0x03\n"
      "4 unwind \n"
      "4 unwind PC 0x3304\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_advance_loc4) {
  this->memory_.SetMemory(0x500, std::vector<uint8_t>{0x04, 0x04, 0x03, 0x02, 0x01});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x500, 0x505));
  std::string expected =
      "4 unwind DW_CFA_advance_loc4 16909060\n"
      "4 unwind Raw Data: 0x04 0x04 0x03 0x02 0x01\n"
      "4 unwind \n"
      "4 unwind PC 0x1022304\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0x2000, 0x500, 0x505));
  expected =
      "4 unwind DW_CFA_advance_loc4 16909060\n"
      "4 unwind Raw Data: 0x04 0x04 0x03 0x02 0x01\n"
      "4 unwind \n"
      "4 unwind PC 0x1024304\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_undefined) {
  this->memory_.SetMemory(0xa00, std::vector<uint8_t>{0x07, 0x09});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0xa00, 0xa02));
  std::string expected =
      "4 unwind DW_CFA_undefined register(9)\n"
      "4 unwind Raw Data: 0x07 0x09\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  dwarf_loc_regs_t cie_loc_regs;
  this->memory_.SetMemory(0x1a00, std::vector<uint8_t>{0x07, 0x81, 0x01});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x1a00, 0x1a03));
  expected =
      "4 unwind DW_CFA_undefined register(129)\n"
      "4 unwind Raw Data: 0x07 0x81 0x01\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_same) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x08, 0x7f});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x100, 0x102));
  std::string expected =
      "4 unwind DW_CFA_same_value register(127)\n"
      "4 unwind Raw Data: 0x08 0x7f\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  this->memory_.SetMemory(0x2100, std::vector<uint8_t>{0x08, 0xff, 0x01});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x2100, 0x2103));
  expected =
      "4 unwind DW_CFA_same_value register(255)\n"
      "4 unwind Raw Data: 0x08 0xff 0x01\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_register) {
  this->memory_.SetMemory(0x300, std::vector<uint8_t>{0x09, 0x02, 0x01});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x300, 0x303));
  std::string expected =
      "4 unwind DW_CFA_register register(2) register(1)\n"
      "4 unwind Raw Data: 0x09 0x02 0x01\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  this->memory_.SetMemory(0x4300, std::vector<uint8_t>{0x09, 0xff, 0x01, 0xff, 0x03});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x4300, 0x4305));
  expected =
      "4 unwind DW_CFA_register register(255) register(511)\n"
      "4 unwind Raw Data: 0x09 0xff 0x01 0xff 0x03\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_state) {
  this->memory_.SetMemory(0x300, std::vector<uint8_t>{0x0a});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x300, 0x301));

  std::string expected =
      "4 unwind DW_CFA_remember_state\n"
      "4 unwind Raw Data: 0x0a\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  this->memory_.SetMemory(0x4300, std::vector<uint8_t>{0x0b});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x4300, 0x4301));

  expected =
      "4 unwind DW_CFA_restore_state\n"
      "4 unwind Raw Data: 0x0b\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_state_cfa_offset_restore) {
  this->memory_.SetMemory(0x3000, std::vector<uint8_t>{0x0a, 0x0e, 0x40, 0x0b});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x3000, 0x3004));

  std::string expected =
      "4 unwind DW_CFA_remember_state\n"
      "4 unwind Raw Data: 0x0a\n"
      "4 unwind DW_CFA_def_cfa_offset 64\n"
      "4 unwind Raw Data: 0x0e 0x40\n"
      "4 unwind DW_CFA_restore_state\n"
      "4 unwind Raw Data: 0x0b\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_def_cfa) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x0c, 0x7f, 0x74});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x100, 0x103));

  std::string expected =
      "4 unwind DW_CFA_def_cfa register(127) 116\n"
      "4 unwind Raw Data: 0x0c 0x7f 0x74\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  this->memory_.SetMemory(0x200, std::vector<uint8_t>{0x0c, 0xff, 0x02, 0xf4, 0x04});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x200, 0x205));

  expected =
      "4 unwind DW_CFA_def_cfa register(383) 628\n"
      "4 unwind Raw Data: 0x0c 0xff 0x02 0xf4 0x04\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_def_cfa_sf) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x12, 0x30, 0x25});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x100, 0x103));

  std::string expected =
      "4 unwind DW_CFA_def_cfa_sf register(48) 37\n"
      "4 unwind Raw Data: 0x12 0x30 0x25\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  // Test a negative value.
  ResetLogs();
  this->memory_.SetMemory(0x200, std::vector<uint8_t>{0x12, 0xa3, 0x01, 0xfa, 0x7f});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x200, 0x205));

  expected =
      "4 unwind DW_CFA_def_cfa_sf register(163) -6\n"
      "4 unwind Raw Data: 0x12 0xa3 0x01 0xfa 0x7f\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_def_cfa_register) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x0d, 0x72});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x100, 0x102));

  std::string expected =
      "4 unwind DW_CFA_def_cfa_register register(114)\n"
      "4 unwind Raw Data: 0x0d 0x72\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  this->memory_.SetMemory(0x200, std::vector<uint8_t>{0x0d, 0xf9, 0x20});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x200, 0x203));

  expected =
      "4 unwind DW_CFA_def_cfa_register register(4217)\n"
      "4 unwind Raw Data: 0x0d 0xf9 0x20\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_def_cfa_offset) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x0e, 0x59});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x100, 0x102));

  std::string expected =
      "4 unwind DW_CFA_def_cfa_offset 89\n"
      "4 unwind Raw Data: 0x0e 0x59\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x100, 0x102));

  expected =
      "4 unwind DW_CFA_def_cfa_offset 89\n"
      "4 unwind Raw Data: 0x0e 0x59\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  this->memory_.SetMemory(0x200, std::vector<uint8_t>{0x0e, 0xd4, 0x0a});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x200, 0x203));

  expected =
      "4 unwind DW_CFA_def_cfa_offset 1364\n"
      "4 unwind Raw Data: 0x0e 0xd4 0x0a\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_def_cfa_offset_sf) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x13, 0x23});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x100, 0x102));

  std::string expected =
      "4 unwind DW_CFA_def_cfa_offset_sf 35\n"
      "4 unwind Raw Data: 0x13 0x23\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x100, 0x102));

  expected =
      "4 unwind DW_CFA_def_cfa_offset_sf 35\n"
      "4 unwind Raw Data: 0x13 0x23\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  // Negative offset.
  ResetLogs();
  this->memory_.SetMemory(0x200, std::vector<uint8_t>{0x13, 0xf6, 0x7f});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x200, 0x203));

  expected =
      "4 unwind DW_CFA_def_cfa_offset_sf -10\n"
      "4 unwind Raw Data: 0x13 0xf6 0x7f\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_def_cfa_expression) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x0f, 0x04, 0x01, 0x02, 0x04, 0x05});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x100, 0x106));

  std::string expected =
      "4 unwind DW_CFA_def_cfa_expression 4\n"
      "4 unwind Raw Data: 0x0f 0x04 0x01 0x02 0x04 0x05\n"
      "4 unwind   Illegal\n"
      "4 unwind   Raw Data: 0x01\n"
      "4 unwind   Illegal\n"
      "4 unwind   Raw Data: 0x02\n"
      "4 unwind   Illegal\n"
      "4 unwind   Raw Data: 0x04\n"
      "4 unwind   Illegal\n"
      "4 unwind   Raw Data: 0x05\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  std::vector<uint8_t> ops{0x0f, 0x81, 0x01};
  expected = "4 unwind Raw Data: 0x0f 0x81 0x01";
  std::string op_string;
  for (uint8_t i = 3; i < 132; i++) {
    ops.push_back(0x05);
    op_string +=
        "4 unwind   Illegal\n"
        "4 unwind   Raw Data: 0x05\n";
    expected += " 0x05";
    if (((i + 1) % 10) == 0) {
      expected += "\n4 unwind Raw Data:";
    }
  }
  expected += '\n';
  this->memory_.SetMemory(0x200, ops);
  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x200, 0x284));

  expected = "4 unwind DW_CFA_def_cfa_expression 129\n" + expected;
  ASSERT_EQ(expected + op_string, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_expression) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x10, 0x04, 0x02, 0xc0, 0xc1});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x100, 0x105));

  std::string expected =
      "4 unwind DW_CFA_expression register(4) 2\n"
      "4 unwind Raw Data: 0x10 0x04 0x02 0xc0 0xc1\n"
      "4 unwind   Illegal\n"
      "4 unwind   Raw Data: 0xc0\n"
      "4 unwind   Illegal\n"
      "4 unwind   Raw Data: 0xc1\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  std::vector<uint8_t> ops{0x10, 0xff, 0x01, 0x82, 0x01};
  expected = "4 unwind Raw Data: 0x10 0xff 0x01 0x82 0x01";
  std::string op_string;
  for (uint8_t i = 5; i < 135; i++) {
    ops.push_back(0xa0 + (i - 5) % 96);
    op_string += "4 unwind   Illegal\n";
    op_string += android::base::StringPrintf("4 unwind   Raw Data: 0x%02x\n", ops.back());
    expected += android::base::StringPrintf(" 0x%02x", ops.back());
    if (((i + 1) % 10) == 0) {
      expected += "\n4 unwind Raw Data:";
    }
  }
  expected = "4 unwind DW_CFA_expression register(255) 130\n" + expected + "\n";

  this->memory_.SetMemory(0x200, ops);
  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x200, 0x287));

  ASSERT_EQ(expected + op_string, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_val_offset) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x14, 0x45, 0x54});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x100, 0x103));

  std::string expected =
      "4 unwind DW_CFA_val_offset register(69) 84\n"
      "4 unwind Raw Data: 0x14 0x45 0x54\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  this->memory_.SetMemory(0x400, std::vector<uint8_t>{0x14, 0xa2, 0x02, 0xb4, 0x05});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x400, 0x405));

  expected =
      "4 unwind DW_CFA_val_offset register(290) 692\n"
      "4 unwind Raw Data: 0x14 0xa2 0x02 0xb4 0x05\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_val_offset_sf) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x15, 0x56, 0x12});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x100, 0x103));

  std::string expected =
      "4 unwind DW_CFA_val_offset_sf register(86) 18\n"
      "4 unwind Raw Data: 0x15 0x56 0x12\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  // Negative value.
  ResetLogs();
  this->memory_.SetMemory(0xa00, std::vector<uint8_t>{0x15, 0xff, 0x01, 0xc0, 0x7f});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0xa00, 0xa05));

  expected =
      "4 unwind DW_CFA_val_offset_sf register(255) -64\n"
      "4 unwind Raw Data: 0x15 0xff 0x01 0xc0 0x7f\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_val_expression) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x16, 0x05, 0x02, 0xb0, 0xb1});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x100, 0x105));

  std::string expected =
      "4 unwind DW_CFA_val_expression register(5) 2\n"
      "4 unwind Raw Data: 0x16 0x05 0x02 0xb0 0xb1\n"
      "4 unwind   Illegal\n"
      "4 unwind   Raw Data: 0xb0\n"
      "4 unwind   Illegal\n"
      "4 unwind   Raw Data: 0xb1\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  std::vector<uint8_t> ops{0x16, 0x83, 0x10, 0xa8, 0x01};
  expected = "4 unwind Raw Data: 0x16 0x83 0x10 0xa8 0x01";
  std::string op_string;
  for (uint8_t i = 0; i < 168; i++) {
    ops.push_back(0xa0 + (i % 96));
    op_string += "4 unwind   Illegal\n";
    op_string += android::base::StringPrintf("4 unwind   Raw Data: 0x%02x\n", ops.back());
    expected += android::base::StringPrintf(" 0x%02x", ops.back());
    if (((i + 6) % 10) == 0) {
      expected += "\n4 unwind Raw Data:";
    }
  }
  expected = "4 unwind DW_CFA_val_expression register(2051) 168\n" + expected + "\n";

  this->memory_.SetMemory(0xa00, ops);

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0xa00, 0xaad));

  ASSERT_EQ(expected + op_string, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_gnu_args_size) {
  this->memory_.SetMemory(0x2000, std::vector<uint8_t>{0x2e, 0x04});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x2000, 0x2002));

  std::string expected =
      "4 unwind DW_CFA_GNU_args_size 4\n"
      "4 unwind Raw Data: 0x2e 0x04\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  this->memory_.SetMemory(0x5000, std::vector<uint8_t>{0x2e, 0xa4, 0x80, 0x04});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x5000, 0x5004));

  expected =
      "4 unwind DW_CFA_GNU_args_size 65572\n"
      "4 unwind Raw Data: 0x2e 0xa4 0x80 0x04\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_gnu_negative_offset_extended) {
  this->memory_.SetMemory(0x500, std::vector<uint8_t>{0x2f, 0x08, 0x10});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x500, 0x503));

  std::string expected =
      "4 unwind DW_CFA_GNU_negative_offset_extended register(8) 16\n"
      "4 unwind Raw Data: 0x2f 0x08 0x10\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  this->memory_.SetMemory(0x1500, std::vector<uint8_t>{0x2f, 0x81, 0x02, 0xff, 0x01});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x1500, 0x1505));

  expected =
      "4 unwind DW_CFA_GNU_negative_offset_extended register(257) 255\n"
      "4 unwind Raw Data: 0x2f 0x81 0x02 0xff 0x01\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaLogTest, cfa_register_override) {
  this->memory_.SetMemory(0x300, std::vector<uint8_t>{0x09, 0x02, 0x01, 0x09, 0x02, 0x04});

  ASSERT_TRUE(this->cfa_->Log(0, this->fde_.pc_start, 0, 0x300, 0x306));

  std::string expected =
      "4 unwind DW_CFA_register register(2) register(1)\n"
      "4 unwind Raw Data: 0x09 0x02 0x01\n"
      "4 unwind DW_CFA_register register(2) register(4)\n"
      "4 unwind Raw Data: 0x09 0x02 0x04\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

REGISTER_TYPED_TEST_CASE_P(DwarfCfaLogTest, cfa_illegal, cfa_nop, cfa_offset, cfa_offset_extended,
                           cfa_offset_extended_sf, cfa_restore, cfa_restore_extended, cfa_set_loc,
                           cfa_advance_loc, cfa_advance_loc1, cfa_advance_loc2, cfa_advance_loc4,
                           cfa_undefined, cfa_same, cfa_register, cfa_state,
                           cfa_state_cfa_offset_restore, cfa_def_cfa, cfa_def_cfa_sf,
                           cfa_def_cfa_register, cfa_def_cfa_offset, cfa_def_cfa_offset_sf,
                           cfa_def_cfa_expression, cfa_expression, cfa_val_offset,
                           cfa_val_offset_sf, cfa_val_expression, cfa_gnu_args_size,
                           cfa_gnu_negative_offset_extended, cfa_register_override);

typedef ::testing::Types<uint32_t, uint64_t> DwarfCfaLogTestTypes;
INSTANTIATE_TYPED_TEST_CASE_P(, DwarfCfaLogTest, DwarfCfaLogTestTypes);

}  // namespace unwindstack
