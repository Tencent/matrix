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
#include <unordered_map>

#include <gtest/gtest.h>

#include <unwindstack/DwarfError.h>
#include <unwindstack/DwarfLocation.h>
#include <unwindstack/DwarfMemory.h>
#include <unwindstack/DwarfStructs.h>
#include <unwindstack/Log.h>

#include "DwarfCfa.h"

#include "LogFake.h"
#include "MemoryFake.h"

namespace unwindstack {

template <typename TypeParam>
class DwarfCfaTest : public ::testing::Test {
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
    fde_.cie = &cie_;

    cfa_.reset(new DwarfCfa<TypeParam>(dmem_.get(), &fde_));
  }

  MemoryFake memory_;
  std::unique_ptr<DwarfMemory> dmem_;
  std::unique_ptr<DwarfCfa<TypeParam>> cfa_;
  DwarfCie cie_;
  DwarfFde fde_;
};
TYPED_TEST_CASE_P(DwarfCfaTest);

// NOTE: All test class variables need to be referenced as this->.

TYPED_TEST_P(DwarfCfaTest, cfa_illegal) {
  for (uint8_t i = 0x17; i < 0x3f; i++) {
    if (i == 0x2e || i == 0x2f) {
      // Skip gnu extension ops.
      continue;
    }
    this->memory_.SetMemory(0x2000, std::vector<uint8_t>{i});
    dwarf_loc_regs_t loc_regs;

    ASSERT_FALSE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x2000, 0x2001, &loc_regs));
    ASSERT_EQ(DWARF_ERROR_ILLEGAL_VALUE, this->cfa_->LastErrorCode());
    ASSERT_EQ(0x2001U, this->dmem_->cur_offset());

    ASSERT_EQ("", GetFakeLogPrint());
    ASSERT_EQ("", GetFakeLogBuf());
  }
}

TYPED_TEST_P(DwarfCfaTest, cfa_nop) {
  this->memory_.SetMemory(0x2000, std::vector<uint8_t>{0x00});
  dwarf_loc_regs_t loc_regs;

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x2000, 0x2001, &loc_regs));
  ASSERT_EQ(0x2001U, this->dmem_->cur_offset());
  ASSERT_EQ(0U, loc_regs.size());

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

// This test needs to be examined.
TYPED_TEST_P(DwarfCfaTest, cfa_offset) {
  this->memory_.SetMemory(0x2000, std::vector<uint8_t>{0x83, 0x04});
  dwarf_loc_regs_t loc_regs;

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x2000, 0x2002, &loc_regs));
  ASSERT_EQ(0x2002U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  auto location = loc_regs.find(3);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_OFFSET, location->second.type);
  ASSERT_EQ(32U, location->second.values[0]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  this->memory_.SetMemory(0x2100, std::vector<uint8_t>{0x83, 0x84, 0x01});
  loc_regs.clear();

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x2100, 0x2103, &loc_regs));
  ASSERT_EQ(0x2103U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  location = loc_regs.find(3);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_OFFSET, location->second.type);
  ASSERT_EQ(1056U, location->second.values[0]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_offset_extended) {
  this->memory_.SetMemory(0x500, std::vector<uint8_t>{0x05, 0x03, 0x02});
  dwarf_loc_regs_t loc_regs;

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x500, 0x503, &loc_regs));
  ASSERT_EQ(0x503U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  auto location = loc_regs.find(3);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_OFFSET, location->second.type);
  ASSERT_EQ(2U, location->second.values[0]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  loc_regs.clear();
  this->memory_.SetMemory(0x1500, std::vector<uint8_t>{0x05, 0x81, 0x01, 0x82, 0x12});

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x1500, 0x1505, &loc_regs));
  ASSERT_EQ(0x1505U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  location = loc_regs.find(129);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_OFFSET, location->second.type);
  ASSERT_EQ(2306U, location->second.values[0]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_offset_extended_sf) {
  this->memory_.SetMemory(0x500, std::vector<uint8_t>{0x11, 0x05, 0x10});
  dwarf_loc_regs_t loc_regs;

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x500, 0x503, &loc_regs));
  ASSERT_EQ(0x503U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  auto location = loc_regs.find(5);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_OFFSET, location->second.type);
  ASSERT_EQ(0x80U, location->second.values[0]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  // Check a negative value for the offset.
  ResetLogs();
  loc_regs.clear();
  this->memory_.SetMemory(0x1500, std::vector<uint8_t>{0x11, 0x86, 0x01, 0xff, 0x7f});

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x1500, 0x1505, &loc_regs));
  ASSERT_EQ(0x1505U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  location = loc_regs.find(134);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_OFFSET, location->second.type);
  ASSERT_EQ(static_cast<uint64_t>(-8), location->second.values[0]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_restore) {
  this->memory_.SetMemory(0x2000, std::vector<uint8_t>{0xc2});
  dwarf_loc_regs_t loc_regs;

  ASSERT_FALSE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x2000, 0x2001, &loc_regs));
  ASSERT_EQ(DWARF_ERROR_ILLEGAL_STATE, this->cfa_->LastErrorCode());
  ASSERT_EQ(0x2001U, this->dmem_->cur_offset());
  ASSERT_EQ(0U, loc_regs.size());

  ASSERT_EQ("4 unwind restore while processing cie\n", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  dwarf_loc_regs_t cie_loc_regs;
  cie_loc_regs[2] = {.type = DWARF_LOCATION_REGISTER, .values = {0, 0}};
  this->cfa_->set_cie_loc_regs(&cie_loc_regs);
  this->memory_.SetMemory(0x3000, std::vector<uint8_t>{0x82, 0x04, 0xc2});

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x3000, 0x3003, &loc_regs));
  ASSERT_EQ(0x3003U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  auto location = loc_regs.find(2);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_REGISTER, location->second.type);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_restore_extended) {
  this->memory_.SetMemory(0x4000, std::vector<uint8_t>{0x06, 0x08});
  dwarf_loc_regs_t loc_regs;

  ASSERT_FALSE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x4000, 0x4002, &loc_regs));
  ASSERT_EQ(DWARF_ERROR_ILLEGAL_STATE, this->cfa_->LastErrorCode());
  ASSERT_EQ(0x4002U, this->dmem_->cur_offset());
  ASSERT_EQ(0U, loc_regs.size());

  ASSERT_EQ("4 unwind restore while processing cie\n", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  loc_regs.clear();
  this->memory_.SetMemory(0x5000, std::vector<uint8_t>{0x05, 0x82, 0x02, 0x04, 0x06, 0x82, 0x02});
  dwarf_loc_regs_t cie_loc_regs;
  cie_loc_regs[258] = {.type = DWARF_LOCATION_REGISTER, .values = {0, 0}};
  this->cfa_->set_cie_loc_regs(&cie_loc_regs);

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x5000, 0x5007, &loc_regs));
  ASSERT_EQ(0x5007U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  auto location = loc_regs.find(258);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_REGISTER, location->second.type);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_set_loc) {
  uint8_t buffer[1 + sizeof(TypeParam)];
  buffer[0] = 0x1;
  TypeParam address;
  std::string raw_data("Raw Data: 0x01 ");
  std::string address_str;
  if (sizeof(TypeParam) == 4) {
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
  dwarf_loc_regs_t loc_regs;
  ASSERT_TRUE(
      this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x50, 0x51 + sizeof(TypeParam), &loc_regs));
  ASSERT_EQ(0x51 + sizeof(TypeParam), this->dmem_->cur_offset());
  ASSERT_EQ(address, this->cfa_->cur_pc());
  ASSERT_EQ(0U, loc_regs.size());

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  // Check for a set going back.
  ResetLogs();
  loc_regs.clear();
  this->fde_.pc_start = address + 0x10;
  ASSERT_TRUE(
      this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x50, 0x51 + sizeof(TypeParam), &loc_regs));
  ASSERT_EQ(0x51 + sizeof(TypeParam), this->dmem_->cur_offset());
  ASSERT_EQ(address, this->cfa_->cur_pc());
  ASSERT_EQ(0U, loc_regs.size());

  std::string cur_address_str(address_str);
  cur_address_str[cur_address_str.size() - 2] = '8';
  std::string expected = "4 unwind Warning: PC is moving backwards: old " + cur_address_str +
                         " new " + address_str + "\n";
  ASSERT_EQ(expected, GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_advance_loc1) {
  this->memory_.SetMemory(0x200, std::vector<uint8_t>{0x02, 0x04});
  dwarf_loc_regs_t loc_regs;

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x200, 0x202, &loc_regs));
  ASSERT_EQ(0x202U, this->dmem_->cur_offset());
  ASSERT_EQ(this->fde_.pc_start + 0x10, this->cfa_->cur_pc());
  ASSERT_EQ(0U, loc_regs.size());

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_advance_loc2) {
  this->memory_.SetMemory(0x600, std::vector<uint8_t>{0x03, 0x04, 0x03});
  dwarf_loc_regs_t loc_regs;

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x600, 0x603, &loc_regs));
  ASSERT_EQ(0x603U, this->dmem_->cur_offset());
  ASSERT_EQ(this->fde_.pc_start + 0xc10U, this->cfa_->cur_pc());
  ASSERT_EQ(0U, loc_regs.size());

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_advance_loc4) {
  this->memory_.SetMemory(0x500, std::vector<uint8_t>{0x04, 0x04, 0x03, 0x02, 0x01});
  dwarf_loc_regs_t loc_regs;

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x500, 0x505, &loc_regs));
  ASSERT_EQ(0x505U, this->dmem_->cur_offset());
  ASSERT_EQ(this->fde_.pc_start + 0x4080c10, this->cfa_->cur_pc());
  ASSERT_EQ(0U, loc_regs.size());

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_undefined) {
  this->memory_.SetMemory(0xa00, std::vector<uint8_t>{0x07, 0x09});
  dwarf_loc_regs_t loc_regs;

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0xa00, 0xa02, &loc_regs));
  ASSERT_EQ(0xa02U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  auto location = loc_regs.find(9);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_UNDEFINED, location->second.type);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  loc_regs.clear();
  this->memory_.SetMemory(0x1a00, std::vector<uint8_t>{0x07, 0x81, 0x01});

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x1a00, 0x1a03, &loc_regs));
  ASSERT_EQ(0x1a03U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  location = loc_regs.find(129);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_UNDEFINED, location->second.type);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_same) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x08, 0x7f});
  dwarf_loc_regs_t loc_regs;

  loc_regs[127] = {.type = DWARF_LOCATION_REGISTER, .values = {0, 0}};
  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x100, 0x102, &loc_regs));
  ASSERT_EQ(0x102U, this->dmem_->cur_offset());
  ASSERT_EQ(0U, loc_regs.size());
  ASSERT_EQ(0U, loc_regs.count(127));

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  loc_regs.clear();
  this->memory_.SetMemory(0x2100, std::vector<uint8_t>{0x08, 0xff, 0x01});

  loc_regs[255] = {.type = DWARF_LOCATION_REGISTER, .values = {0, 0}};
  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x2100, 0x2103, &loc_regs));
  ASSERT_EQ(0x2103U, this->dmem_->cur_offset());
  ASSERT_EQ(0U, loc_regs.size());
  ASSERT_EQ(0U, loc_regs.count(255));

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_register) {
  this->memory_.SetMemory(0x300, std::vector<uint8_t>{0x09, 0x02, 0x01});
  dwarf_loc_regs_t loc_regs;

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x300, 0x303, &loc_regs));
  ASSERT_EQ(0x303U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  auto location = loc_regs.find(2);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_REGISTER, location->second.type);
  ASSERT_EQ(1U, location->second.values[0]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  loc_regs.clear();
  this->memory_.SetMemory(0x4300, std::vector<uint8_t>{0x09, 0xff, 0x01, 0xff, 0x03});

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x4300, 0x4305, &loc_regs));
  ASSERT_EQ(0x4305U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  location = loc_regs.find(255);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_REGISTER, location->second.type);
  ASSERT_EQ(511U, location->second.values[0]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_state) {
  this->memory_.SetMemory(0x300, std::vector<uint8_t>{0x0a});
  dwarf_loc_regs_t loc_regs;

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x300, 0x301, &loc_regs));
  ASSERT_EQ(0x301U, this->dmem_->cur_offset());
  ASSERT_EQ(0U, loc_regs.size());

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  this->memory_.SetMemory(0x4300, std::vector<uint8_t>{0x0b});

  loc_regs.clear();
  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x4300, 0x4301, &loc_regs));
  ASSERT_EQ(0x4301U, this->dmem_->cur_offset());
  ASSERT_EQ(0U, loc_regs.size());

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  this->memory_.SetMemory(0x2000, std::vector<uint8_t>{0x85, 0x02, 0x0a, 0x86, 0x04, 0x0b});

  loc_regs.clear();
  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x2000, 0x2005, &loc_regs));
  ASSERT_EQ(0x2005U, this->dmem_->cur_offset());
  ASSERT_EQ(2U, loc_regs.size());
  ASSERT_NE(loc_regs.end(), loc_regs.find(5));
  ASSERT_NE(loc_regs.end(), loc_regs.find(6));

  loc_regs.clear();
  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x2000, 0x2006, &loc_regs));
  ASSERT_EQ(0x2006U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  ASSERT_NE(loc_regs.end(), loc_regs.find(5));

  ResetLogs();
  this->memory_.SetMemory(
      0x6000, std::vector<uint8_t>{0x0a, 0x85, 0x02, 0x0a, 0x86, 0x04, 0x0a, 0x87, 0x01, 0x0a, 0x89,
                                   0x05, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b});

  loc_regs.clear();
  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x6000, 0x600c, &loc_regs));
  ASSERT_EQ(0x600cU, this->dmem_->cur_offset());
  ASSERT_EQ(4U, loc_regs.size());
  ASSERT_NE(loc_regs.end(), loc_regs.find(5));
  ASSERT_NE(loc_regs.end(), loc_regs.find(6));
  ASSERT_NE(loc_regs.end(), loc_regs.find(7));
  ASSERT_NE(loc_regs.end(), loc_regs.find(9));

  loc_regs.clear();
  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x6000, 0x600d, &loc_regs));
  ASSERT_EQ(0x600dU, this->dmem_->cur_offset());
  ASSERT_EQ(3U, loc_regs.size());
  ASSERT_NE(loc_regs.end(), loc_regs.find(5));
  ASSERT_NE(loc_regs.end(), loc_regs.find(6));
  ASSERT_NE(loc_regs.end(), loc_regs.find(7));

  loc_regs.clear();
  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x6000, 0x600e, &loc_regs));
  ASSERT_EQ(0x600eU, this->dmem_->cur_offset());
  ASSERT_EQ(2U, loc_regs.size());
  ASSERT_NE(loc_regs.end(), loc_regs.find(5));
  ASSERT_NE(loc_regs.end(), loc_regs.find(6));

  loc_regs.clear();
  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x6000, 0x600f, &loc_regs));
  ASSERT_EQ(0x600fU, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  ASSERT_NE(loc_regs.end(), loc_regs.find(5));

  loc_regs.clear();
  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x6000, 0x6010, &loc_regs));
  ASSERT_EQ(0x6010U, this->dmem_->cur_offset());
  ASSERT_EQ(0U, loc_regs.size());

  loc_regs.clear();
  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x6000, 0x6011, &loc_regs));
  ASSERT_EQ(0x6011U, this->dmem_->cur_offset());
  ASSERT_EQ(0U, loc_regs.size());
}

// This test verifies that the cfa offset is saved and restored properly.
// Even though the spec is not clear about whether the offset is also
// restored, the gcc unwinder does, and libunwind does too.
TYPED_TEST_P(DwarfCfaTest, cfa_state_cfa_offset_restore) {
  this->memory_.SetMemory(0x3000, std::vector<uint8_t>{0x0a, 0x0e, 0x40, 0x0b});
  dwarf_loc_regs_t loc_regs;
  loc_regs[CFA_REG] = {.type = DWARF_LOCATION_REGISTER, .values = {5, 100}};

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x3000, 0x3004, &loc_regs));
  ASSERT_EQ(0x3004U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  ASSERT_EQ(DWARF_LOCATION_REGISTER, loc_regs[CFA_REG].type);
  ASSERT_EQ(5U, loc_regs[CFA_REG].values[0]);
  ASSERT_EQ(100U, loc_regs[CFA_REG].values[1]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_def_cfa) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x0c, 0x7f, 0x74});
  dwarf_loc_regs_t loc_regs;

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x100, 0x103, &loc_regs));
  ASSERT_EQ(0x103U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  ASSERT_EQ(DWARF_LOCATION_REGISTER, loc_regs[CFA_REG].type);
  ASSERT_EQ(0x7fU, loc_regs[CFA_REG].values[0]);
  ASSERT_EQ(0x74U, loc_regs[CFA_REG].values[1]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  loc_regs.clear();
  this->memory_.SetMemory(0x200, std::vector<uint8_t>{0x0c, 0xff, 0x02, 0xf4, 0x04});

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x200, 0x205, &loc_regs));
  ASSERT_EQ(0x205U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  ASSERT_EQ(DWARF_LOCATION_REGISTER, loc_regs[CFA_REG].type);
  ASSERT_EQ(0x17fU, loc_regs[CFA_REG].values[0]);
  ASSERT_EQ(0x274U, loc_regs[CFA_REG].values[1]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_def_cfa_sf) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x12, 0x30, 0x25});
  dwarf_loc_regs_t loc_regs;

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x100, 0x103, &loc_regs));
  ASSERT_EQ(0x103U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  ASSERT_EQ(DWARF_LOCATION_REGISTER, loc_regs[CFA_REG].type);
  ASSERT_EQ(0x30U, loc_regs[CFA_REG].values[0]);
  ASSERT_EQ(0x128U, loc_regs[CFA_REG].values[1]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  // Test a negative value.
  ResetLogs();
  loc_regs.clear();
  this->memory_.SetMemory(0x200, std::vector<uint8_t>{0x12, 0xa3, 0x01, 0xfa, 0x7f});

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x200, 0x205, &loc_regs));
  ASSERT_EQ(0x205U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  ASSERT_EQ(DWARF_LOCATION_REGISTER, loc_regs[CFA_REG].type);
  ASSERT_EQ(0xa3U, loc_regs[CFA_REG].values[0]);
  ASSERT_EQ(static_cast<uint64_t>(-48), loc_regs[CFA_REG].values[1]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_def_cfa_register) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x0d, 0x72});
  dwarf_loc_regs_t loc_regs;

  // This fails because the cfa is not defined as a register.
  ASSERT_FALSE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x100, 0x102, &loc_regs));
  ASSERT_EQ(0U, loc_regs.size());
  ASSERT_EQ(DWARF_ERROR_ILLEGAL_STATE, this->cfa_->LastErrorCode());

  ASSERT_EQ("4 unwind Attempt to set new register, but cfa is not already set to a register.\n",
            GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  loc_regs.clear();
  loc_regs[CFA_REG] = {.type = DWARF_LOCATION_REGISTER, .values = {3, 20}};

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x100, 0x102, &loc_regs));
  ASSERT_EQ(0x102U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  ASSERT_EQ(DWARF_LOCATION_REGISTER, loc_regs[CFA_REG].type);
  ASSERT_EQ(0x72U, loc_regs[CFA_REG].values[0]);
  ASSERT_EQ(20U, loc_regs[CFA_REG].values[1]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  this->memory_.SetMemory(0x200, std::vector<uint8_t>{0x0d, 0xf9, 0x20});
  loc_regs.clear();
  loc_regs[CFA_REG] = {.type = DWARF_LOCATION_REGISTER, .values = {3, 60}};

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x200, 0x203, &loc_regs));
  ASSERT_EQ(0x203U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  ASSERT_EQ(DWARF_LOCATION_REGISTER, loc_regs[CFA_REG].type);
  ASSERT_EQ(0x1079U, loc_regs[CFA_REG].values[0]);
  ASSERT_EQ(60U, loc_regs[CFA_REG].values[1]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_def_cfa_offset) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x0e, 0x59});
  dwarf_loc_regs_t loc_regs;

  // This fails because the cfa is not defined as a register.
  ASSERT_FALSE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x100, 0x102, &loc_regs));
  ASSERT_EQ(0U, loc_regs.size());
  ASSERT_EQ(DWARF_ERROR_ILLEGAL_STATE, this->cfa_->LastErrorCode());

  ASSERT_EQ("4 unwind Attempt to set offset, but cfa is not set to a register.\n",
            GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  loc_regs.clear();
  loc_regs[CFA_REG] = {.type = DWARF_LOCATION_REGISTER, .values = {3}};

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x100, 0x102, &loc_regs));
  ASSERT_EQ(0x102U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  ASSERT_EQ(DWARF_LOCATION_REGISTER, loc_regs[CFA_REG].type);
  ASSERT_EQ(3U, loc_regs[CFA_REG].values[0]);
  ASSERT_EQ(0x59U, loc_regs[CFA_REG].values[1]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  this->memory_.SetMemory(0x200, std::vector<uint8_t>{0x0e, 0xd4, 0x0a});
  loc_regs.clear();
  loc_regs[CFA_REG] = {.type = DWARF_LOCATION_REGISTER, .values = {3}};

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x200, 0x203, &loc_regs));
  ASSERT_EQ(0x203U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  ASSERT_EQ(DWARF_LOCATION_REGISTER, loc_regs[CFA_REG].type);
  ASSERT_EQ(3U, loc_regs[CFA_REG].values[0]);
  ASSERT_EQ(0x554U, loc_regs[CFA_REG].values[1]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_def_cfa_offset_sf) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x13, 0x23});
  dwarf_loc_regs_t loc_regs;

  // This fails because the cfa is not defined as a register.
  ASSERT_FALSE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x100, 0x102, &loc_regs));
  ASSERT_EQ(DWARF_ERROR_ILLEGAL_STATE, this->cfa_->LastErrorCode());

  ASSERT_EQ("4 unwind Attempt to set offset, but cfa is not set to a register.\n",
            GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  loc_regs.clear();
  loc_regs[CFA_REG] = {.type = DWARF_LOCATION_REGISTER, .values = {3}};

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x100, 0x102, &loc_regs));
  ASSERT_EQ(0x102U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  ASSERT_EQ(DWARF_LOCATION_REGISTER, loc_regs[CFA_REG].type);
  ASSERT_EQ(3U, loc_regs[CFA_REG].values[0]);
  ASSERT_EQ(0x118U, loc_regs[CFA_REG].values[1]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  // Negative offset.
  ResetLogs();
  this->memory_.SetMemory(0x200, std::vector<uint8_t>{0x13, 0xf6, 0x7f});
  loc_regs.clear();
  loc_regs[CFA_REG] = {.type = DWARF_LOCATION_REGISTER, .values = {3}};

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x200, 0x203, &loc_regs));
  ASSERT_EQ(0x203U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  ASSERT_EQ(DWARF_LOCATION_REGISTER, loc_regs[CFA_REG].type);
  ASSERT_EQ(3U, loc_regs[CFA_REG].values[0]);
  ASSERT_EQ(static_cast<TypeParam>(-80), static_cast<TypeParam>(loc_regs[CFA_REG].values[1]));

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_def_cfa_expression) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x0f, 0x04, 0x01, 0x02, 0x03, 0x04});
  dwarf_loc_regs_t loc_regs;

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x100, 0x106, &loc_regs));
  ASSERT_EQ(0x106U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  std::vector<uint8_t> ops{0x0f, 0x81, 0x01};
  for (uint8_t i = 3; i < 132; i++) {
    ops.push_back(i - 1);
  }
  this->memory_.SetMemory(0x200, ops);
  loc_regs.clear();
  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x200, 0x284, &loc_regs));
  ASSERT_EQ(0x284U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  ASSERT_EQ(DWARF_LOCATION_VAL_EXPRESSION, loc_regs[CFA_REG].type);
  ASSERT_EQ(0x81U, loc_regs[CFA_REG].values[0]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_expression) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x10, 0x04, 0x02, 0x40, 0x20});
  dwarf_loc_regs_t loc_regs;

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x100, 0x105, &loc_regs));
  ASSERT_EQ(0x105U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  auto location = loc_regs.find(4);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_EXPRESSION, location->second.type);
  ASSERT_EQ(2U, location->second.values[0]);
  ASSERT_EQ(0x105U, location->second.values[1]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  std::vector<uint8_t> ops{0x10, 0xff, 0x01, 0x82, 0x01};
  for (uint8_t i = 5; i < 135; i++) {
    ops.push_back(i - 4);
  }

  this->memory_.SetMemory(0x200, ops);
  loc_regs.clear();
  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x200, 0x287, &loc_regs));
  ASSERT_EQ(0x287U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  location = loc_regs.find(255);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_EXPRESSION, location->second.type);
  ASSERT_EQ(130U, location->second.values[0]);
  ASSERT_EQ(0x287U, location->second.values[1]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_val_offset) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x14, 0x45, 0x54});
  dwarf_loc_regs_t loc_regs;

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x100, 0x103, &loc_regs));
  ASSERT_EQ(0x103U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  auto location = loc_regs.find(69);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_VAL_OFFSET, location->second.type);
  ASSERT_EQ(0x2a0U, location->second.values[0]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  loc_regs.clear();
  this->memory_.SetMemory(0x400, std::vector<uint8_t>{0x14, 0xa2, 0x02, 0xb4, 0x05});

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x400, 0x405, &loc_regs));
  ASSERT_EQ(0x405U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  location = loc_regs.find(290);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_VAL_OFFSET, location->second.type);
  ASSERT_EQ(0x15a0U, location->second.values[0]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_val_offset_sf) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x15, 0x56, 0x12});
  dwarf_loc_regs_t loc_regs;

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x100, 0x103, &loc_regs));
  ASSERT_EQ(0x103U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  auto location = loc_regs.find(86);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_VAL_OFFSET, location->second.type);
  ASSERT_EQ(0x90U, location->second.values[0]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  // Negative value.
  ResetLogs();
  loc_regs.clear();
  this->memory_.SetMemory(0xa00, std::vector<uint8_t>{0x15, 0xff, 0x01, 0xc0, 0x7f});

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0xa00, 0xa05, &loc_regs));
  ASSERT_EQ(0xa05U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  location = loc_regs.find(255);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_VAL_OFFSET, location->second.type);
  ASSERT_EQ(static_cast<uint64_t>(-512), location->second.values[0]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_val_expression) {
  this->memory_.SetMemory(0x100, std::vector<uint8_t>{0x16, 0x05, 0x02, 0x10, 0x20});
  dwarf_loc_regs_t loc_regs;

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x100, 0x105, &loc_regs));
  ASSERT_EQ(0x105U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  auto location = loc_regs.find(5);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_VAL_EXPRESSION, location->second.type);
  ASSERT_EQ(2U, location->second.values[0]);
  ASSERT_EQ(0x105U, location->second.values[1]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  std::vector<uint8_t> ops{0x16, 0x83, 0x10, 0xa8, 0x01};
  for (uint8_t i = 0; i < 168; i++) {
    ops.push_back(i);
  }

  this->memory_.SetMemory(0xa00, ops);
  loc_regs.clear();

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0xa00, 0xaad, &loc_regs));
  ASSERT_EQ(0xaadU, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  location = loc_regs.find(2051);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_VAL_EXPRESSION, location->second.type);
  ASSERT_EQ(168U, location->second.values[0]);
  ASSERT_EQ(0xaadU, location->second.values[1]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_gnu_args_size) {
  this->memory_.SetMemory(0x2000, std::vector<uint8_t>{0x2e, 0x04});
  dwarf_loc_regs_t loc_regs;

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x2000, 0x2002, &loc_regs));
  ASSERT_EQ(0x2002U, this->dmem_->cur_offset());
  ASSERT_EQ(0U, loc_regs.size());

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  loc_regs.clear();
  this->memory_.SetMemory(0x5000, std::vector<uint8_t>{0x2e, 0xa4, 0x80, 0x04});

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x5000, 0x5004, &loc_regs));
  ASSERT_EQ(0x5004U, this->dmem_->cur_offset());
  ASSERT_EQ(0U, loc_regs.size());

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_gnu_negative_offset_extended) {
  this->memory_.SetMemory(0x500, std::vector<uint8_t>{0x2f, 0x08, 0x10});
  dwarf_loc_regs_t loc_regs;

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x500, 0x503, &loc_regs));
  ASSERT_EQ(0x503U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  auto location = loc_regs.find(8);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_OFFSET, location->second.type);
  ASSERT_EQ(static_cast<uint64_t>(-16), location->second.values[0]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());

  ResetLogs();
  loc_regs.clear();
  this->memory_.SetMemory(0x1500, std::vector<uint8_t>{0x2f, 0x81, 0x02, 0xff, 0x01});

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x1500, 0x1505, &loc_regs));
  ASSERT_EQ(0x1505U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  location = loc_regs.find(257);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_OFFSET, location->second.type);
  ASSERT_EQ(static_cast<uint64_t>(-255), location->second.values[0]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

TYPED_TEST_P(DwarfCfaTest, cfa_register_override) {
  this->memory_.SetMemory(0x300, std::vector<uint8_t>{0x09, 0x02, 0x01, 0x09, 0x02, 0x04});
  dwarf_loc_regs_t loc_regs;

  ASSERT_TRUE(this->cfa_->GetLocationInfo(this->fde_.pc_start, 0x300, 0x306, &loc_regs));
  ASSERT_EQ(0x306U, this->dmem_->cur_offset());
  ASSERT_EQ(1U, loc_regs.size());
  auto location = loc_regs.find(2);
  ASSERT_NE(loc_regs.end(), location);
  ASSERT_EQ(DWARF_LOCATION_REGISTER, location->second.type);
  ASSERT_EQ(4U, location->second.values[0]);

  ASSERT_EQ("", GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

REGISTER_TYPED_TEST_CASE_P(DwarfCfaTest, cfa_illegal, cfa_nop, cfa_offset, cfa_offset_extended,
                           cfa_offset_extended_sf, cfa_restore, cfa_restore_extended, cfa_set_loc,
                           cfa_advance_loc1, cfa_advance_loc2, cfa_advance_loc4, cfa_undefined,
                           cfa_same, cfa_register, cfa_state, cfa_state_cfa_offset_restore,
                           cfa_def_cfa, cfa_def_cfa_sf, cfa_def_cfa_register, cfa_def_cfa_offset,
                           cfa_def_cfa_offset_sf, cfa_def_cfa_expression, cfa_expression,
                           cfa_val_offset, cfa_val_offset_sf, cfa_val_expression, cfa_gnu_args_size,
                           cfa_gnu_negative_offset_extended, cfa_register_override);

typedef ::testing::Types<uint32_t, uint64_t> DwarfCfaTestTypes;
INSTANTIATE_TYPED_TEST_CASE_P(, DwarfCfaTest, DwarfCfaTestTypes);

}  // namespace unwindstack
