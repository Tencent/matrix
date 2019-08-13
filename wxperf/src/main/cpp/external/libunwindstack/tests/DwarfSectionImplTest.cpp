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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <unwindstack/DwarfError.h>
#include <unwindstack/DwarfSection.h>

#include "DwarfEncoding.h"

#include "LogFake.h"
#include "MemoryFake.h"
#include "RegsFake.h"

namespace unwindstack {

template <typename TypeParam>
class MockDwarfSectionImpl : public DwarfSectionImpl<TypeParam> {
 public:
  MockDwarfSectionImpl(Memory* memory) : DwarfSectionImpl<TypeParam>(memory) {}
  virtual ~MockDwarfSectionImpl() = default;

  MOCK_METHOD2(Init, bool(uint64_t, uint64_t));

  MOCK_METHOD2(GetFdeOffsetFromPc, bool(uint64_t, uint64_t*));

  MOCK_METHOD1(GetFdeFromIndex, const DwarfFde*(size_t));

  MOCK_METHOD1(GetCieOffsetFromFde32, uint64_t(uint32_t));

  MOCK_METHOD1(GetCieOffsetFromFde64, uint64_t(uint64_t));

  MOCK_METHOD1(AdjustPcFromFde, uint64_t(uint64_t));

  void TestSetCie32Value(uint32_t value32) { this->cie32_value_ = value32; }

  void TestSetCie64Value(uint64_t value64) { this->cie64_value_ = value64; }

  void TestSetCachedCieEntry(uint64_t offset, const DwarfCie& cie) {
    this->cie_entries_[offset] = cie;
  }
  void TestClearCachedCieEntry() { this->cie_entries_.clear(); }

  void TestSetCachedFdeEntry(uint64_t offset, const DwarfFde& fde) {
    this->fde_entries_[offset] = fde;
  }
  void TestClearCachedFdeEntry() { this->fde_entries_.clear(); }

  void TestSetCachedCieLocRegs(uint64_t offset, const dwarf_loc_regs_t& loc_regs) {
    this->cie_loc_regs_[offset] = loc_regs;
  }
  void TestClearCachedCieLocRegs() { this->cie_loc_regs_.clear(); }

  void TestClearError() { this->last_error_.code = DWARF_ERROR_NONE; }
};

template <typename TypeParam>
class DwarfSectionImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    memory_.Clear();
    section_ = new MockDwarfSectionImpl<TypeParam>(&memory_);
    ResetLogs();
    section_->TestSetCie32Value(static_cast<uint32_t>(-1));
    section_->TestSetCie64Value(static_cast<uint64_t>(-1));
  }

  void TearDown() override { delete section_; }

  MemoryFake memory_;
  MockDwarfSectionImpl<TypeParam>* section_ = nullptr;
};
TYPED_TEST_CASE_P(DwarfSectionImplTest);

// NOTE: All test class variables need to be referenced as this->.

TYPED_TEST_P(DwarfSectionImplTest, Eval_cfa_expr_eval_fail) {
  DwarfCie cie{.version = 3, .return_address_register = 5};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  regs.set_pc(0x100);
  regs.set_sp(0x2000);
  regs[5] = 0x20;
  regs[9] = 0x3000;
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_VAL_EXPRESSION, {0x2, 0x5002}};
  bool finished;
  ASSERT_FALSE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_EQ(DWARF_ERROR_MEMORY_INVALID, this->section_->LastErrorCode());
  EXPECT_EQ(0x5000U, this->section_->LastErrorAddress());
}

TYPED_TEST_P(DwarfSectionImplTest, Eval_cfa_expr_no_stack) {
  DwarfCie cie{.version = 3, .return_address_register = 5};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  regs.set_pc(0x100);
  regs.set_sp(0x2000);
  regs[5] = 0x20;
  regs[9] = 0x3000;
  this->memory_.SetMemory(0x5000, std::vector<uint8_t>{0x96, 0x96, 0x96});
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_VAL_EXPRESSION, {0x2, 0x5002}};
  bool finished;
  ASSERT_FALSE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_EQ(DWARF_ERROR_ILLEGAL_STATE, this->section_->LastErrorCode());
}

TYPED_TEST_P(DwarfSectionImplTest, Eval_cfa_expr) {
  DwarfCie cie{.version = 3, .return_address_register = 5};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  regs.set_pc(0x100);
  regs.set_sp(0x2000);
  regs[5] = 0x20;
  regs[9] = 0x3000;
  this->memory_.SetMemory(0x5000, std::vector<uint8_t>{0x0c, 0x00, 0x00, 0x00, 0x80});
  TypeParam cfa_value = 0x12345;
  this->memory_.SetMemory(0x80000000, &cfa_value, sizeof(cfa_value));
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_EXPRESSION, {0x4, 0x5004}};
  bool finished;
  ASSERT_FALSE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_EQ(DWARF_ERROR_ILLEGAL_VALUE, this->section_->LastErrorCode());
}

TYPED_TEST_P(DwarfSectionImplTest, Eval_cfa_val_expr) {
  DwarfCie cie{.version = 3, .return_address_register = 5};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  regs.set_pc(0x100);
  regs.set_sp(0x2000);
  regs[5] = 0x20;
  regs[9] = 0x3000;
  this->memory_.SetMemory(0x5000, std::vector<uint8_t>{0x0c, 0x00, 0x00, 0x00, 0x80});
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_VAL_EXPRESSION, {0x4, 0x5004}};
  bool finished;
  ASSERT_TRUE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  ASSERT_FALSE(finished);
  EXPECT_EQ(0x80000000U, regs.sp());
  EXPECT_EQ(0x20U, regs.pc());
}

TYPED_TEST_P(DwarfSectionImplTest, Eval_cfa_expr_is_register) {
  DwarfCie cie{.version = 3, .return_address_register = 5};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  regs.set_pc(0x100);
  regs.set_sp(0x2000);
  regs[5] = 0x20;
  regs[9] = 0x3000;
  this->memory_.SetMemory(0x5000, std::vector<uint8_t>{0x50, 0x96, 0x96});
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_VAL_EXPRESSION, {0x2, 0x5002}};
  bool finished;
  ASSERT_FALSE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_EQ(DWARF_ERROR_NOT_IMPLEMENTED, this->section_->LastErrorCode());
}

TYPED_TEST_P(DwarfSectionImplTest, Eval_bad_regs) {
  DwarfCie cie{.return_address_register = 60};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  bool finished;
  ASSERT_FALSE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_EQ(DWARF_ERROR_ILLEGAL_VALUE, this->section_->LastErrorCode());
}

TYPED_TEST_P(DwarfSectionImplTest, Eval_no_cfa) {
  DwarfCie cie{.return_address_register = 5};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  bool finished;
  ASSERT_FALSE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_EQ(DWARF_ERROR_CFA_NOT_DEFINED, this->section_->LastErrorCode());
}

TYPED_TEST_P(DwarfSectionImplTest, Eval_cfa_bad) {
  DwarfCie cie{.return_address_register = 5};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_REGISTER, {20, 0}};
  bool finished;
  ASSERT_FALSE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_EQ(DWARF_ERROR_ILLEGAL_VALUE, this->section_->LastErrorCode());

  this->section_->TestClearError();
  loc_regs.erase(CFA_REG);
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_INVALID, {0, 0}};
  ASSERT_FALSE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_EQ(DWARF_ERROR_ILLEGAL_VALUE, this->section_->LastErrorCode());

  this->section_->TestClearError();
  loc_regs.erase(CFA_REG);
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_OFFSET, {0, 0}};
  ASSERT_FALSE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_EQ(DWARF_ERROR_ILLEGAL_VALUE, this->section_->LastErrorCode());

  this->section_->TestClearError();
  loc_regs.erase(CFA_REG);
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_VAL_OFFSET, {0, 0}};
  ASSERT_FALSE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_EQ(DWARF_ERROR_ILLEGAL_VALUE, this->section_->LastErrorCode());
}

TYPED_TEST_P(DwarfSectionImplTest, Eval_cfa_register_prev) {
  DwarfCie cie{.return_address_register = 5};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  regs.set_pc(0x100);
  regs.set_sp(0x2000);
  regs[5] = 0x20;
  regs[9] = 0x3000;
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_REGISTER, {9, 0}};
  bool finished;
  ASSERT_TRUE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_FALSE(finished);
  EXPECT_EQ(0x20U, regs.pc());
  EXPECT_EQ(0x3000U, regs.sp());
}

TYPED_TEST_P(DwarfSectionImplTest, Eval_cfa_register_from_value) {
  DwarfCie cie{.return_address_register = 5};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  regs.set_pc(0x100);
  regs.set_sp(0x2000);
  regs[5] = 0x20;
  regs[6] = 0x4000;
  regs[9] = 0x3000;
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_REGISTER, {6, 0}};
  bool finished;
  ASSERT_TRUE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_FALSE(finished);
  EXPECT_EQ(0x20U, regs.pc());
  EXPECT_EQ(0x4000U, regs.sp());
}

TYPED_TEST_P(DwarfSectionImplTest, Eval_double_indirection) {
  DwarfCie cie{.return_address_register = 5};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  regs.set_pc(0x100);
  regs.set_sp(0x2000);
  regs[1] = 0x100;
  regs[3] = 0x300;
  regs[8] = 0x10;
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_REGISTER, {8, 0}};
  loc_regs[1] = DwarfLocation{DWARF_LOCATION_REGISTER, {3, 1}};
  loc_regs[9] = DwarfLocation{DWARF_LOCATION_REGISTER, {1, 2}};
  bool finished;
  ASSERT_TRUE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_EQ(0x301U, regs[1]);
  EXPECT_EQ(0x300U, regs[3]);
  EXPECT_EQ(0x10U, regs[8]);
  EXPECT_EQ(0x102U, regs[9]);
}

TYPED_TEST_P(DwarfSectionImplTest, Eval_register_reference_chain) {
  DwarfCie cie{.return_address_register = 5};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  regs.set_pc(0x100);
  regs.set_sp(0x2000);
  regs[0] = 0x10;
  regs[1] = 0x20;
  regs[2] = 0x30;
  regs[3] = 0x40;
  regs[4] = 0x50;
  regs[5] = 0x60;
  regs[8] = 0x20;
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_REGISTER, {8, 0}};
  loc_regs[1] = DwarfLocation{DWARF_LOCATION_REGISTER, {0, 1}};
  loc_regs[2] = DwarfLocation{DWARF_LOCATION_REGISTER, {1, 2}};
  loc_regs[3] = DwarfLocation{DWARF_LOCATION_REGISTER, {2, 3}};
  loc_regs[4] = DwarfLocation{DWARF_LOCATION_REGISTER, {3, 4}};
  loc_regs[5] = DwarfLocation{DWARF_LOCATION_REGISTER, {4, 5}};
  bool finished;
  ASSERT_TRUE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_EQ(0x10U, regs[0]);
  EXPECT_EQ(0x11U, regs[1]);
  EXPECT_EQ(0x22U, regs[2]);
  EXPECT_EQ(0x33U, regs[3]);
  EXPECT_EQ(0x44U, regs[4]);
  EXPECT_EQ(0x55U, regs[5]);
  EXPECT_EQ(0x20U, regs[8]);
}

TYPED_TEST_P(DwarfSectionImplTest, Eval_dex_pc) {
  DwarfCie cie{.return_address_register = 5};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  regs.set_pc(0x100);
  regs.set_sp(0x2000);
  regs[0] = 0x10;
  regs[8] = 0x20;
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_REGISTER, {8, 0}};
  loc_regs[1] = DwarfLocation{DWARF_LOCATION_VAL_EXPRESSION, {0x8, 0x5008}};
  this->memory_.SetMemory(0x5000, std::vector<uint8_t>{0x0c, 'D', 'E', 'X', '1', 0x13, 0x08, 0x11});
  bool finished;
  ASSERT_TRUE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_EQ(0x10U, regs[0]);
  EXPECT_EQ(0x20U, regs[8]);
  EXPECT_EQ(0x11U, regs.dex_pc());
}

TYPED_TEST_P(DwarfSectionImplTest, Eval_invalid_register) {
  DwarfCie cie{.return_address_register = 5};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  regs.set_pc(0x100);
  regs.set_sp(0x2000);
  regs[8] = 0x10;
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_REGISTER, {8, 0}};
  loc_regs[1] = DwarfLocation{DWARF_LOCATION_REGISTER, {10, 0}};
  bool finished;
  ASSERT_FALSE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_EQ(DWARF_ERROR_ILLEGAL_VALUE, this->section_->LastErrorCode());
}

TYPED_TEST_P(DwarfSectionImplTest, Eval_different_reg_locations) {
  DwarfCie cie{.return_address_register = 5};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  if (sizeof(TypeParam) == sizeof(uint64_t)) {
    this->memory_.SetData64(0x2150, 0x12345678abcdef00ULL);
  } else {
    this->memory_.SetData32(0x2150, 0x12345678);
  }

  regs.set_pc(0x100);
  regs.set_sp(0x2000);
  regs[3] = 0x234;
  regs[5] = 0x10;
  regs[8] = 0x2100;
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_REGISTER, {8, 0}};
  loc_regs[1] = DwarfLocation{DWARF_LOCATION_VAL_OFFSET, {0x100, 0}};
  loc_regs[2] = DwarfLocation{DWARF_LOCATION_OFFSET, {0x50, 0}};
  loc_regs[3] = DwarfLocation{DWARF_LOCATION_UNDEFINED, {0, 0}};
  bool finished;
  ASSERT_TRUE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_FALSE(finished);
  EXPECT_EQ(0x10U, regs.pc());
  EXPECT_EQ(0x2100U, regs.sp());
  EXPECT_EQ(0x2200U, regs[1]);
  EXPECT_EQ(0x234U, regs[3]);
  if (sizeof(TypeParam) == sizeof(uint64_t)) {
    EXPECT_EQ(0x12345678abcdef00ULL, regs[2]);
  } else {
    EXPECT_EQ(0x12345678U, regs[2]);
  }
}

TYPED_TEST_P(DwarfSectionImplTest, Eval_return_address_undefined) {
  DwarfCie cie{.return_address_register = 5};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  regs.set_pc(0x100);
  regs.set_sp(0x2000);
  regs[5] = 0x20;
  regs[8] = 0x10;
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_REGISTER, {8, 0}};
  loc_regs[5] = DwarfLocation{DWARF_LOCATION_UNDEFINED, {0, 0}};
  bool finished;
  ASSERT_TRUE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_TRUE(finished);
  EXPECT_EQ(0U, regs.pc());
  EXPECT_EQ(0x10U, regs.sp());
}

TYPED_TEST_P(DwarfSectionImplTest, Eval_pc_zero) {
  DwarfCie cie{.return_address_register = 5};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  regs.set_pc(0x100);
  regs.set_sp(0x2000);
  regs[5] = 0;
  regs[8] = 0x10;
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_REGISTER, {8, 0}};
  bool finished;
  ASSERT_TRUE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_TRUE(finished);
  EXPECT_EQ(0U, regs.pc());
  EXPECT_EQ(0x10U, regs.sp());
}

TYPED_TEST_P(DwarfSectionImplTest, Eval_return_address) {
  DwarfCie cie{.return_address_register = 5};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  regs.set_pc(0x100);
  regs.set_sp(0x2000);
  regs[5] = 0x20;
  regs[8] = 0x10;
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_REGISTER, {8, 0}};
  bool finished;
  ASSERT_TRUE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_FALSE(finished);
  EXPECT_EQ(0x20U, regs.pc());
  EXPECT_EQ(0x10U, regs.sp());
}

TYPED_TEST_P(DwarfSectionImplTest, Eval_ignore_large_reg_loc) {
  DwarfCie cie{.return_address_register = 5};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  regs.set_pc(0x100);
  regs.set_sp(0x2000);
  regs[5] = 0x20;
  regs[8] = 0x10;
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_REGISTER, {8, 0}};
  // This should not result in any errors.
  loc_regs[20] = DwarfLocation{DWARF_LOCATION_REGISTER, {8, 0}};
  bool finished;
  ASSERT_TRUE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_FALSE(finished);
  EXPECT_EQ(0x20U, regs.pc());
  EXPECT_EQ(0x10U, regs.sp());
}

TYPED_TEST_P(DwarfSectionImplTest, Eval_reg_expr) {
  DwarfCie cie{.version = 3, .return_address_register = 5};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  regs.set_pc(0x100);
  regs.set_sp(0x2000);
  regs[8] = 0x3000;
  this->memory_.SetMemory(0x5000, std::vector<uint8_t>{0x0c, 0x00, 0x00, 0x00, 0x80});
  TypeParam cfa_value = 0x12345;
  this->memory_.SetMemory(0x80000000, &cfa_value, sizeof(cfa_value));
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_REGISTER, {8, 0}};
  loc_regs[5] = DwarfLocation{DWARF_LOCATION_EXPRESSION, {0x4, 0x5004}};
  bool finished;
  ASSERT_TRUE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_FALSE(finished);
  EXPECT_EQ(0x3000U, regs.sp());
  EXPECT_EQ(0x12345U, regs.pc());
}

TYPED_TEST_P(DwarfSectionImplTest, Eval_reg_val_expr) {
  DwarfCie cie{.version = 3, .return_address_register = 5};
  RegsImplFake<TypeParam> regs(10);
  dwarf_loc_regs_t loc_regs;

  regs.set_pc(0x100);
  regs.set_sp(0x2000);
  regs[8] = 0x3000;
  this->memory_.SetMemory(0x5000, std::vector<uint8_t>{0x0c, 0x00, 0x00, 0x00, 0x80});
  loc_regs[CFA_REG] = DwarfLocation{DWARF_LOCATION_REGISTER, {8, 0}};
  loc_regs[5] = DwarfLocation{DWARF_LOCATION_VAL_EXPRESSION, {0x4, 0x5004}};
  bool finished;
  ASSERT_TRUE(this->section_->Eval(&cie, &this->memory_, loc_regs, &regs, &finished));
  EXPECT_FALSE(finished);
  EXPECT_EQ(0x3000U, regs.sp());
  EXPECT_EQ(0x80000000U, regs.pc());
}

TYPED_TEST_P(DwarfSectionImplTest, GetCie_fail_should_not_cache) {
  ASSERT_TRUE(this->section_->GetCie(0x4000) == nullptr);
  EXPECT_EQ(DWARF_ERROR_MEMORY_INVALID, this->section_->LastErrorCode());
  EXPECT_EQ(0x4000U, this->section_->LastErrorAddress());
  this->section_->TestClearError();
  ASSERT_TRUE(this->section_->GetCie(0x4000) == nullptr);
  EXPECT_EQ(DWARF_ERROR_MEMORY_INVALID, this->section_->LastErrorCode());
  EXPECT_EQ(0x4000U, this->section_->LastErrorAddress());
}

TYPED_TEST_P(DwarfSectionImplTest, GetCie_32_version_check) {
  this->memory_.SetData32(0x5000, 0x100);
  this->memory_.SetData32(0x5004, 0xffffffff);
  this->memory_.SetData8(0x5008, 0x1);
  this->memory_.SetData8(0x5009, '\0');
  this->memory_.SetData8(0x500a, 4);
  this->memory_.SetData8(0x500b, 8);
  this->memory_.SetData8(0x500c, 0x20);

  const DwarfCie* cie = this->section_->GetCie(0x5000);
  ASSERT_TRUE(cie != nullptr);
  EXPECT_EQ(1U, cie->version);
  EXPECT_EQ(DW_EH_PE_sdata4, cie->fde_address_encoding);
  EXPECT_EQ(DW_EH_PE_omit, cie->lsda_encoding);
  EXPECT_EQ(0U, cie->segment_size);
  EXPECT_EQ(1U, cie->augmentation_string.size());
  EXPECT_EQ('\0', cie->augmentation_string[0]);
  EXPECT_EQ(0U, cie->personality_handler);
  EXPECT_EQ(0x500dU, cie->cfa_instructions_offset);
  EXPECT_EQ(0x5104U, cie->cfa_instructions_end);
  EXPECT_EQ(4U, cie->code_alignment_factor);
  EXPECT_EQ(8, cie->data_alignment_factor);
  EXPECT_EQ(0x20U, cie->return_address_register);
  EXPECT_EQ(DWARF_ERROR_NONE, this->section_->LastErrorCode());

  this->section_->TestClearCachedCieEntry();
  // Set version to 0, 2, 5 and verify we fail.
  this->memory_.SetData8(0x5008, 0x0);
  this->section_->TestClearError();
  ASSERT_TRUE(this->section_->GetCie(0x5000) == nullptr);
  EXPECT_EQ(DWARF_ERROR_UNSUPPORTED_VERSION, this->section_->LastErrorCode());

  this->memory_.SetData8(0x5008, 0x2);
  this->section_->TestClearError();
  ASSERT_TRUE(this->section_->GetCie(0x5000) == nullptr);
  EXPECT_EQ(DWARF_ERROR_UNSUPPORTED_VERSION, this->section_->LastErrorCode());

  this->memory_.SetData8(0x5008, 0x5);
  this->section_->TestClearError();
  ASSERT_TRUE(this->section_->GetCie(0x5000) == nullptr);
  EXPECT_EQ(DWARF_ERROR_UNSUPPORTED_VERSION, this->section_->LastErrorCode());
}

TYPED_TEST_P(DwarfSectionImplTest, GetCie_negative_data_alignment_factor) {
  this->memory_.SetData32(0x5000, 0x100);
  this->memory_.SetData32(0x5004, 0xffffffff);
  this->memory_.SetData8(0x5008, 0x1);
  this->memory_.SetData8(0x5009, '\0');
  this->memory_.SetData8(0x500a, 4);
  this->memory_.SetMemory(0x500b, std::vector<uint8_t>{0xfc, 0xff, 0xff, 0xff, 0x7f});
  this->memory_.SetData8(0x5010, 0x20);

  const DwarfCie* cie = this->section_->GetCie(0x5000);
  ASSERT_TRUE(cie != nullptr);
  EXPECT_EQ(1U, cie->version);
  EXPECT_EQ(DW_EH_PE_sdata4, cie->fde_address_encoding);
  EXPECT_EQ(DW_EH_PE_omit, cie->lsda_encoding);
  EXPECT_EQ(0U, cie->segment_size);
  EXPECT_EQ(1U, cie->augmentation_string.size());
  EXPECT_EQ('\0', cie->augmentation_string[0]);
  EXPECT_EQ(0U, cie->personality_handler);
  EXPECT_EQ(0x5011U, cie->cfa_instructions_offset);
  EXPECT_EQ(0x5104U, cie->cfa_instructions_end);
  EXPECT_EQ(4U, cie->code_alignment_factor);
  EXPECT_EQ(-4, cie->data_alignment_factor);
  EXPECT_EQ(0x20U, cie->return_address_register);
}

TYPED_TEST_P(DwarfSectionImplTest, GetCie_64_no_augment) {
  this->memory_.SetData32(0x8000, 0xffffffff);
  this->memory_.SetData64(0x8004, 0x200);
  this->memory_.SetData64(0x800c, 0xffffffffffffffffULL);
  this->memory_.SetData8(0x8014, 0x1);
  this->memory_.SetData8(0x8015, '\0');
  this->memory_.SetData8(0x8016, 4);
  this->memory_.SetData8(0x8017, 8);
  this->memory_.SetData8(0x8018, 0x20);

  const DwarfCie* cie = this->section_->GetCie(0x8000);
  ASSERT_TRUE(cie != nullptr);
  EXPECT_EQ(1U, cie->version);
  EXPECT_EQ(DW_EH_PE_sdata8, cie->fde_address_encoding);
  EXPECT_EQ(DW_EH_PE_omit, cie->lsda_encoding);
  EXPECT_EQ(0U, cie->segment_size);
  EXPECT_EQ(1U, cie->augmentation_string.size());
  EXPECT_EQ('\0', cie->augmentation_string[0]);
  EXPECT_EQ(0U, cie->personality_handler);
  EXPECT_EQ(0x8019U, cie->cfa_instructions_offset);
  EXPECT_EQ(0x820cU, cie->cfa_instructions_end);
  EXPECT_EQ(4U, cie->code_alignment_factor);
  EXPECT_EQ(8, cie->data_alignment_factor);
  EXPECT_EQ(0x20U, cie->return_address_register);
}

TYPED_TEST_P(DwarfSectionImplTest, GetCie_augment) {
  this->memory_.SetData32(0x5000, 0x100);
  this->memory_.SetData32(0x5004, 0xffffffff);
  this->memory_.SetData8(0x5008, 0x1);
  this->memory_.SetMemory(0x5009, std::vector<uint8_t>{'z', 'L', 'P', 'R', '\0'});
  this->memory_.SetData8(0x500e, 4);
  this->memory_.SetData8(0x500f, 8);
  this->memory_.SetData8(0x5010, 0x10);
  // Augment length.
  this->memory_.SetData8(0x5011, 0xf);
  // L data.
  this->memory_.SetData8(0x5012, DW_EH_PE_textrel | DW_EH_PE_udata2);
  // P data.
  this->memory_.SetData8(0x5013, DW_EH_PE_udata4);
  this->memory_.SetData32(0x5014, 0x12345678);
  // R data.
  this->memory_.SetData8(0x5018, DW_EH_PE_udata2);

  const DwarfCie* cie = this->section_->GetCie(0x5000);
  ASSERT_TRUE(cie != nullptr);
  EXPECT_EQ(1U, cie->version);
  EXPECT_EQ(DW_EH_PE_udata2, cie->fde_address_encoding);
  EXPECT_EQ(DW_EH_PE_textrel | DW_EH_PE_udata2, cie->lsda_encoding);
  EXPECT_EQ(0U, cie->segment_size);
  EXPECT_EQ(5U, cie->augmentation_string.size());
  EXPECT_EQ('z', cie->augmentation_string[0]);
  EXPECT_EQ('L', cie->augmentation_string[1]);
  EXPECT_EQ('P', cie->augmentation_string[2]);
  EXPECT_EQ('R', cie->augmentation_string[3]);
  EXPECT_EQ('\0', cie->augmentation_string[4]);
  EXPECT_EQ(0x12345678U, cie->personality_handler);
  EXPECT_EQ(0x5021U, cie->cfa_instructions_offset);
  EXPECT_EQ(0x5104U, cie->cfa_instructions_end);
  EXPECT_EQ(4U, cie->code_alignment_factor);
  EXPECT_EQ(8, cie->data_alignment_factor);
  EXPECT_EQ(0x10U, cie->return_address_register);
}

TYPED_TEST_P(DwarfSectionImplTest, GetCie_version_3) {
  this->memory_.SetData32(0x5000, 0x100);
  this->memory_.SetData32(0x5004, 0xffffffff);
  this->memory_.SetData8(0x5008, 0x3);
  this->memory_.SetData8(0x5009, '\0');
  this->memory_.SetData8(0x500a, 4);
  this->memory_.SetData8(0x500b, 8);
  this->memory_.SetMemory(0x500c, std::vector<uint8_t>{0x81, 0x03});

  const DwarfCie* cie = this->section_->GetCie(0x5000);
  ASSERT_TRUE(cie != nullptr);
  EXPECT_EQ(3U, cie->version);
  EXPECT_EQ(DW_EH_PE_sdata4, cie->fde_address_encoding);
  EXPECT_EQ(DW_EH_PE_omit, cie->lsda_encoding);
  EXPECT_EQ(0U, cie->segment_size);
  EXPECT_EQ(1U, cie->augmentation_string.size());
  EXPECT_EQ('\0', cie->augmentation_string[0]);
  EXPECT_EQ(0U, cie->personality_handler);
  EXPECT_EQ(0x500eU, cie->cfa_instructions_offset);
  EXPECT_EQ(0x5104U, cie->cfa_instructions_end);
  EXPECT_EQ(4U, cie->code_alignment_factor);
  EXPECT_EQ(8, cie->data_alignment_factor);
  EXPECT_EQ(0x181U, cie->return_address_register);
}

TYPED_TEST_P(DwarfSectionImplTest, GetCie_version_4) {
  this->memory_.SetData32(0x5000, 0x100);
  this->memory_.SetData32(0x5004, 0xffffffff);
  this->memory_.SetData8(0x5008, 0x4);
  this->memory_.SetData8(0x5009, '\0');
  this->memory_.SetData8(0x500a, 4);
  this->memory_.SetData8(0x500b, 0x13);
  this->memory_.SetData8(0x500c, 4);
  this->memory_.SetData8(0x500d, 8);
  this->memory_.SetMemory(0x500e, std::vector<uint8_t>{0x81, 0x03});

  const DwarfCie* cie = this->section_->GetCie(0x5000);
  ASSERT_TRUE(cie != nullptr);
  EXPECT_EQ(4U, cie->version);
  EXPECT_EQ(DW_EH_PE_sdata4, cie->fde_address_encoding);
  EXPECT_EQ(DW_EH_PE_omit, cie->lsda_encoding);
  EXPECT_EQ(0x13U, cie->segment_size);
  EXPECT_EQ(1U, cie->augmentation_string.size());
  EXPECT_EQ('\0', cie->augmentation_string[0]);
  EXPECT_EQ(0U, cie->personality_handler);
  EXPECT_EQ(0x5010U, cie->cfa_instructions_offset);
  EXPECT_EQ(0x5104U, cie->cfa_instructions_end);
  EXPECT_EQ(4U, cie->code_alignment_factor);
  EXPECT_EQ(8, cie->data_alignment_factor);
  EXPECT_EQ(0x181U, cie->return_address_register);
}

TYPED_TEST_P(DwarfSectionImplTest, GetFdeFromOffset_fail_should_not_cache) {
  ASSERT_TRUE(this->section_->GetFdeFromOffset(0x4000) == nullptr);
  EXPECT_EQ(DWARF_ERROR_MEMORY_INVALID, this->section_->LastErrorCode());
  EXPECT_EQ(0x4000U, this->section_->LastErrorAddress());
  this->section_->TestClearError();
  ASSERT_TRUE(this->section_->GetFdeFromOffset(0x4000) == nullptr);
  EXPECT_EQ(DWARF_ERROR_MEMORY_INVALID, this->section_->LastErrorCode());
  EXPECT_EQ(0x4000U, this->section_->LastErrorAddress());
}

TYPED_TEST_P(DwarfSectionImplTest, GetFdeFromOffset_32_no_augment) {
  this->memory_.SetData32(0x4000, 0x20);
  this->memory_.SetData32(0x4004, 0x8000);
  this->memory_.SetData32(0x4008, 0x5000);
  this->memory_.SetData32(0x400c, 0x100);

  EXPECT_CALL(*this->section_, GetCieOffsetFromFde32(0x8000)).WillOnce(::testing::Return(0x8000));
  DwarfCie cie{};
  cie.fde_address_encoding = DW_EH_PE_udata4;
  this->section_->TestSetCachedCieEntry(0x8000, cie);
  EXPECT_CALL(*this->section_, AdjustPcFromFde(0x5000)).WillOnce(::testing::Return(0x5000));

  const DwarfFde* fde = this->section_->GetFdeFromOffset(0x4000);
  ASSERT_TRUE(fde != nullptr);
  ASSERT_TRUE(fde->cie != nullptr);
  EXPECT_EQ(0x4010U, fde->cfa_instructions_offset);
  EXPECT_EQ(0x4024U, fde->cfa_instructions_end);
  EXPECT_EQ(0x5000U, fde->pc_start);
  EXPECT_EQ(0x5100U, fde->pc_end);
  EXPECT_EQ(0x8000U, fde->cie_offset);
  EXPECT_EQ(0U, fde->lsda_address);
}

TYPED_TEST_P(DwarfSectionImplTest, GetFdeFromOffset_32_no_augment_non_zero_segment_size) {
  this->memory_.SetData32(0x4000, 0x30);
  this->memory_.SetData32(0x4004, 0x8000);
  this->memory_.SetData32(0x4018, 0x5000);
  this->memory_.SetData32(0x401c, 0x100);

  EXPECT_CALL(*this->section_, GetCieOffsetFromFde32(0x8000)).WillOnce(::testing::Return(0x8000));
  DwarfCie cie{};
  cie.fde_address_encoding = DW_EH_PE_udata4;
  cie.segment_size = 0x10;
  this->section_->TestSetCachedCieEntry(0x8000, cie);
  EXPECT_CALL(*this->section_, AdjustPcFromFde(0x5000)).WillOnce(::testing::Return(0x5000));

  const DwarfFde* fde = this->section_->GetFdeFromOffset(0x4000);
  ASSERT_TRUE(fde != nullptr);
  ASSERT_TRUE(fde->cie != nullptr);
  EXPECT_EQ(0x4020U, fde->cfa_instructions_offset);
  EXPECT_EQ(0x4034U, fde->cfa_instructions_end);
  EXPECT_EQ(0x5000U, fde->pc_start);
  EXPECT_EQ(0x5100U, fde->pc_end);
  EXPECT_EQ(0x8000U, fde->cie_offset);
  EXPECT_EQ(0U, fde->lsda_address);
}

TYPED_TEST_P(DwarfSectionImplTest, GetFdeFromOffset_32_augment) {
  this->memory_.SetData32(0x4000, 0x100);
  this->memory_.SetData32(0x4004, 0x8000);
  this->memory_.SetData32(0x4008, 0x5000);
  this->memory_.SetData32(0x400c, 0x100);
  this->memory_.SetMemory(0x4010, std::vector<uint8_t>{0x82, 0x01});
  this->memory_.SetData16(0x4012, 0x1234);

  EXPECT_CALL(*this->section_, GetCieOffsetFromFde32(0x8000)).WillOnce(::testing::Return(0x8000));
  DwarfCie cie{};
  cie.fde_address_encoding = DW_EH_PE_udata4;
  cie.augmentation_string.push_back('z');
  cie.lsda_encoding = DW_EH_PE_udata2;
  this->section_->TestSetCachedCieEntry(0x8000, cie);
  EXPECT_CALL(*this->section_, AdjustPcFromFde(0x5000)).WillOnce(::testing::Return(0x5000));

  const DwarfFde* fde = this->section_->GetFdeFromOffset(0x4000);
  ASSERT_TRUE(fde != nullptr);
  ASSERT_TRUE(fde->cie != nullptr);
  EXPECT_EQ(0x4094U, fde->cfa_instructions_offset);
  EXPECT_EQ(0x4104U, fde->cfa_instructions_end);
  EXPECT_EQ(0x5000U, fde->pc_start);
  EXPECT_EQ(0x5100U, fde->pc_end);
  EXPECT_EQ(0x8000U, fde->cie_offset);
  EXPECT_EQ(0x1234U, fde->lsda_address);
}

TYPED_TEST_P(DwarfSectionImplTest, GetFdeFromOffset_64_no_augment) {
  this->memory_.SetData32(0x4000, 0xffffffff);
  this->memory_.SetData64(0x4004, 0x100);
  this->memory_.SetData64(0x400c, 0x12345678);
  this->memory_.SetData32(0x4014, 0x5000);
  this->memory_.SetData32(0x4018, 0x100);

  EXPECT_CALL(*this->section_, GetCieOffsetFromFde64(0x12345678))
      .WillOnce(::testing::Return(0x12345678));
  DwarfCie cie{};
  cie.fde_address_encoding = DW_EH_PE_udata4;
  this->section_->TestSetCachedCieEntry(0x12345678, cie);
  EXPECT_CALL(*this->section_, AdjustPcFromFde(0x5000)).WillOnce(::testing::Return(0x5000));

  const DwarfFde* fde = this->section_->GetFdeFromOffset(0x4000);
  ASSERT_TRUE(fde != nullptr);
  ASSERT_TRUE(fde->cie != nullptr);
  EXPECT_EQ(0x401cU, fde->cfa_instructions_offset);
  EXPECT_EQ(0x410cU, fde->cfa_instructions_end);
  EXPECT_EQ(0x5000U, fde->pc_start);
  EXPECT_EQ(0x5100U, fde->pc_end);
  EXPECT_EQ(0x12345678U, fde->cie_offset);
  EXPECT_EQ(0U, fde->lsda_address);
}

TYPED_TEST_P(DwarfSectionImplTest, GetFdeFromOffset_cached) {
  DwarfCie cie{};
  cie.fde_address_encoding = DW_EH_PE_udata4;
  cie.augmentation_string.push_back('z');
  cie.lsda_encoding = DW_EH_PE_udata2;

  DwarfFde fde_cached{};
  fde_cached.cfa_instructions_offset = 0x1000;
  fde_cached.cfa_instructions_end = 0x1100;
  fde_cached.pc_start = 0x9000;
  fde_cached.pc_end = 0x9400;
  fde_cached.cie_offset = 0x30000;
  fde_cached.cie = &cie;
  this->section_->TestSetCachedFdeEntry(0x6000, fde_cached);

  const DwarfFde* fde = this->section_->GetFdeFromOffset(0x6000);
  ASSERT_TRUE(fde != nullptr);
  ASSERT_EQ(&cie, fde->cie);
  EXPECT_EQ(0x1000U, fde->cfa_instructions_offset);
  EXPECT_EQ(0x1100U, fde->cfa_instructions_end);
  EXPECT_EQ(0x9000U, fde->pc_start);
  EXPECT_EQ(0x9400U, fde->pc_end);
  EXPECT_EQ(0x30000U, fde->cie_offset);
}

TYPED_TEST_P(DwarfSectionImplTest, GetCfaLocationInfo_cie_not_cached) {
  DwarfCie cie{};
  cie.cfa_instructions_offset = 0x3000;
  cie.cfa_instructions_end = 0x3002;
  DwarfFde fde{};
  fde.cie = &cie;
  fde.cie_offset = 0x8000;
  fde.cfa_instructions_offset = 0x6000;
  fde.cfa_instructions_end = 0x6002;

  this->memory_.SetMemory(0x3000, std::vector<uint8_t>{0x09, 0x02, 0x01});
  this->memory_.SetMemory(0x6000, std::vector<uint8_t>{0x09, 0x04, 0x03});

  dwarf_loc_regs_t loc_regs;
  ASSERT_TRUE(this->section_->GetCfaLocationInfo(0x100, &fde, &loc_regs));
  ASSERT_EQ(2U, loc_regs.size());

  auto entry = loc_regs.find(2);
  ASSERT_NE(entry, loc_regs.end());
  ASSERT_EQ(DWARF_LOCATION_REGISTER, entry->second.type);
  ASSERT_EQ(1U, entry->second.values[0]);

  entry = loc_regs.find(4);
  ASSERT_NE(entry, loc_regs.end());
  ASSERT_EQ(DWARF_LOCATION_REGISTER, entry->second.type);
  ASSERT_EQ(3U, entry->second.values[0]);
}

TYPED_TEST_P(DwarfSectionImplTest, GetCfaLocationInfo_cie_cached) {
  DwarfCie cie{};
  cie.cfa_instructions_offset = 0x3000;
  cie.cfa_instructions_end = 0x3002;
  DwarfFde fde{};
  fde.cie = &cie;
  fde.cie_offset = 0x8000;
  fde.cfa_instructions_offset = 0x6000;
  fde.cfa_instructions_end = 0x6002;

  dwarf_loc_regs_t cie_loc_regs;
  cie_loc_regs[6] = DwarfLocation{DWARF_LOCATION_REGISTER, {4, 0}};
  this->section_->TestSetCachedCieLocRegs(0x8000, cie_loc_regs);
  this->memory_.SetMemory(0x6000, std::vector<uint8_t>{0x09, 0x04, 0x03});

  dwarf_loc_regs_t loc_regs;
  ASSERT_TRUE(this->section_->GetCfaLocationInfo(0x100, &fde, &loc_regs));
  ASSERT_EQ(2U, loc_regs.size());

  auto entry = loc_regs.find(6);
  ASSERT_NE(entry, loc_regs.end());
  ASSERT_EQ(DWARF_LOCATION_REGISTER, entry->second.type);
  ASSERT_EQ(4U, entry->second.values[0]);

  entry = loc_regs.find(4);
  ASSERT_NE(entry, loc_regs.end());
  ASSERT_EQ(DWARF_LOCATION_REGISTER, entry->second.type);
  ASSERT_EQ(3U, entry->second.values[0]);
}

TYPED_TEST_P(DwarfSectionImplTest, Log) {
  DwarfCie cie{};
  cie.cfa_instructions_offset = 0x5000;
  cie.cfa_instructions_end = 0x5001;
  DwarfFde fde{};
  fde.cie = &cie;
  fde.cfa_instructions_offset = 0x6000;
  fde.cfa_instructions_end = 0x6001;

  this->memory_.SetMemory(0x5000, std::vector<uint8_t>{0x00});
  this->memory_.SetMemory(0x6000, std::vector<uint8_t>{0xc2});
  ASSERT_TRUE(this->section_->Log(2, 0x1000, 0x1000, &fde));

  ASSERT_EQ(
      "4 unwind     DW_CFA_nop\n"
      "4 unwind     Raw Data: 0x00\n"
      "4 unwind     DW_CFA_restore register(2)\n"
      "4 unwind     Raw Data: 0xc2\n",
      GetFakeLogPrint());
  ASSERT_EQ("", GetFakeLogBuf());
}

REGISTER_TYPED_TEST_CASE_P(
    DwarfSectionImplTest, Eval_cfa_expr_eval_fail, Eval_cfa_expr_no_stack,
    Eval_cfa_expr_is_register, Eval_cfa_expr, Eval_cfa_val_expr, Eval_bad_regs, Eval_no_cfa,
    Eval_cfa_bad, Eval_cfa_register_prev, Eval_cfa_register_from_value, Eval_double_indirection,
    Eval_register_reference_chain, Eval_dex_pc, Eval_invalid_register, Eval_different_reg_locations,
    Eval_return_address_undefined, Eval_pc_zero, Eval_return_address, Eval_ignore_large_reg_loc,
    Eval_reg_expr, Eval_reg_val_expr, GetCie_fail_should_not_cache, GetCie_32_version_check,
    GetCie_negative_data_alignment_factor, GetCie_64_no_augment, GetCie_augment, GetCie_version_3,
    GetCie_version_4, GetFdeFromOffset_fail_should_not_cache, GetFdeFromOffset_32_no_augment,
    GetFdeFromOffset_32_no_augment_non_zero_segment_size, GetFdeFromOffset_32_augment,
    GetFdeFromOffset_64_no_augment, GetFdeFromOffset_cached, GetCfaLocationInfo_cie_not_cached,
    GetCfaLocationInfo_cie_cached, Log);

typedef ::testing::Types<uint32_t, uint64_t> DwarfSectionImplTestTypes;
INSTANTIATE_TYPED_TEST_CASE_P(, DwarfSectionImplTest, DwarfSectionImplTestTypes);

}  // namespace unwindstack
