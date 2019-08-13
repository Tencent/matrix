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

#include <unwindstack/DwarfSection.h>

#include "MemoryFake.h"

namespace unwindstack {

class MockDwarfSection : public DwarfSection {
 public:
  MockDwarfSection(Memory* memory) : DwarfSection(memory) {}
  virtual ~MockDwarfSection() = default;

  MOCK_METHOD4(Log, bool(uint8_t, uint64_t, uint64_t, const DwarfFde*));

  MOCK_METHOD5(Eval, bool(const DwarfCie*, Memory*, const dwarf_loc_regs_t&, Regs*, bool*));

  MOCK_METHOD3(GetCfaLocationInfo, bool(uint64_t, const DwarfFde*, dwarf_loc_regs_t*));

  MOCK_METHOD2(Init, bool(uint64_t, uint64_t));

  MOCK_METHOD2(GetFdeOffsetFromPc, bool(uint64_t, uint64_t*));

  MOCK_METHOD1(GetFdeFromOffset, const DwarfFde*(uint64_t));

  MOCK_METHOD1(GetFdeFromIndex, const DwarfFde*(size_t));

  MOCK_METHOD1(IsCie32, bool(uint32_t));

  MOCK_METHOD1(IsCie64, bool(uint64_t));

  MOCK_METHOD1(GetCieOffsetFromFde32, uint64_t(uint32_t));

  MOCK_METHOD1(GetCieOffsetFromFde64, uint64_t(uint64_t));

  MOCK_METHOD1(AdjustPcFromFde, uint64_t(uint64_t));
};

class DwarfSectionTest : public ::testing::Test {
 protected:
  MemoryFake memory_;
};

TEST_F(DwarfSectionTest, GetFdeOffsetFromPc_fail_from_pc) {
  MockDwarfSection mock_section(&memory_);

  EXPECT_CALL(mock_section, GetFdeOffsetFromPc(0x1000, ::testing::_))
      .WillOnce(::testing::Return(false));

  // Verify nullptr when GetFdeOffsetFromPc fails.
  ASSERT_TRUE(mock_section.GetFdeFromPc(0x1000) == nullptr);
}

TEST_F(DwarfSectionTest, GetFdeOffsetFromPc_fail_fde_pc_end) {
  MockDwarfSection mock_section(&memory_);

  DwarfFde fde{};
  fde.pc_end = 0x500;

  EXPECT_CALL(mock_section, GetFdeOffsetFromPc(0x1000, ::testing::_))
      .WillOnce(::testing::Return(true));
  EXPECT_CALL(mock_section, GetFdeFromOffset(::testing::_)).WillOnce(::testing::Return(&fde));

  // Verify nullptr when GetFdeOffsetFromPc fails.
  ASSERT_TRUE(mock_section.GetFdeFromPc(0x1000) == nullptr);
}

TEST_F(DwarfSectionTest, GetFdeOffsetFromPc_pass) {
  MockDwarfSection mock_section(&memory_);

  DwarfFde fde{};
  fde.pc_end = 0x2000;

  EXPECT_CALL(mock_section, GetFdeOffsetFromPc(0x1000, ::testing::_))
      .WillOnce(::testing::Return(true));
  EXPECT_CALL(mock_section, GetFdeFromOffset(::testing::_)).WillOnce(::testing::Return(&fde));

  // Verify nullptr when GetFdeOffsetFromPc fails.
  ASSERT_EQ(&fde, mock_section.GetFdeFromPc(0x1000));
}

TEST_F(DwarfSectionTest, Step_fail_fde) {
  MockDwarfSection mock_section(&memory_);

  EXPECT_CALL(mock_section, GetFdeOffsetFromPc(0x1000, ::testing::_))
      .WillOnce(::testing::Return(false));

  bool finished;
  ASSERT_FALSE(mock_section.Step(0x1000, nullptr, nullptr, &finished));
}

TEST_F(DwarfSectionTest, Step_fail_cie_null) {
  MockDwarfSection mock_section(&memory_);

  DwarfFde fde{};
  fde.pc_end = 0x2000;
  fde.cie = nullptr;

  EXPECT_CALL(mock_section, GetFdeOffsetFromPc(0x1000, ::testing::_))
      .WillOnce(::testing::Return(true));
  EXPECT_CALL(mock_section, GetFdeFromOffset(::testing::_)).WillOnce(::testing::Return(&fde));

  bool finished;
  ASSERT_FALSE(mock_section.Step(0x1000, nullptr, nullptr, &finished));
}

TEST_F(DwarfSectionTest, Step_fail_cfa_location) {
  MockDwarfSection mock_section(&memory_);

  DwarfCie cie{};
  DwarfFde fde{};
  fde.pc_end = 0x2000;
  fde.cie = &cie;

  EXPECT_CALL(mock_section, GetFdeOffsetFromPc(0x1000, ::testing::_))
      .WillOnce(::testing::Return(true));
  EXPECT_CALL(mock_section, GetFdeFromOffset(::testing::_)).WillOnce(::testing::Return(&fde));

  EXPECT_CALL(mock_section, GetCfaLocationInfo(0x1000, &fde, ::testing::_))
      .WillOnce(::testing::Return(false));

  bool finished;
  ASSERT_FALSE(mock_section.Step(0x1000, nullptr, nullptr, &finished));
}

TEST_F(DwarfSectionTest, Step_pass) {
  MockDwarfSection mock_section(&memory_);

  DwarfCie cie{};
  DwarfFde fde{};
  fde.pc_end = 0x2000;
  fde.cie = &cie;

  EXPECT_CALL(mock_section, GetFdeOffsetFromPc(0x1000, ::testing::_))
      .WillOnce(::testing::Return(true));
  EXPECT_CALL(mock_section, GetFdeFromOffset(::testing::_)).WillOnce(::testing::Return(&fde));

  EXPECT_CALL(mock_section, GetCfaLocationInfo(0x1000, &fde, ::testing::_))
      .WillOnce(::testing::Return(true));

  MemoryFake process;
  EXPECT_CALL(mock_section, Eval(&cie, &process, ::testing::_, nullptr, ::testing::_))
      .WillOnce(::testing::Return(true));

  bool finished;
  ASSERT_TRUE(mock_section.Step(0x1000, nullptr, &process, &finished));
}

static bool MockGetCfaLocationInfo(::testing::Unused, const DwarfFde* fde,
                                   dwarf_loc_regs_t* loc_regs) {
  loc_regs->pc_start = fde->pc_start;
  loc_regs->pc_end = fde->pc_end;
  return true;
}

TEST_F(DwarfSectionTest, Step_cache) {
  MockDwarfSection mock_section(&memory_);

  DwarfCie cie{};
  DwarfFde fde{};
  fde.pc_start = 0x500;
  fde.pc_end = 0x2000;
  fde.cie = &cie;

  EXPECT_CALL(mock_section, GetFdeOffsetFromPc(0x1000, ::testing::_))
      .WillOnce(::testing::Return(true));
  EXPECT_CALL(mock_section, GetFdeFromOffset(::testing::_)).WillOnce(::testing::Return(&fde));

  EXPECT_CALL(mock_section, GetCfaLocationInfo(0x1000, &fde, ::testing::_))
      .WillOnce(::testing::Invoke(MockGetCfaLocationInfo));

  MemoryFake process;
  EXPECT_CALL(mock_section, Eval(&cie, &process, ::testing::_, nullptr, ::testing::_))
      .WillRepeatedly(::testing::Return(true));

  bool finished;
  ASSERT_TRUE(mock_section.Step(0x1000, nullptr, &process, &finished));
  ASSERT_TRUE(mock_section.Step(0x1000, nullptr, &process, &finished));
  ASSERT_TRUE(mock_section.Step(0x1500, nullptr, &process, &finished));
}

TEST_F(DwarfSectionTest, Step_cache_not_in_pc) {
  MockDwarfSection mock_section(&memory_);

  DwarfCie cie{};
  DwarfFde fde0{};
  fde0.pc_start = 0x1000;
  fde0.pc_end = 0x2000;
  fde0.cie = &cie;
  EXPECT_CALL(mock_section, GetFdeOffsetFromPc(0x1000, ::testing::_))
      .WillOnce(::testing::Return(true));
  EXPECT_CALL(mock_section, GetFdeFromOffset(::testing::_)).WillOnce(::testing::Return(&fde0));
  EXPECT_CALL(mock_section, GetCfaLocationInfo(0x1000, &fde0, ::testing::_))
      .WillOnce(::testing::Invoke(MockGetCfaLocationInfo));

  MemoryFake process;
  EXPECT_CALL(mock_section, Eval(&cie, &process, ::testing::_, nullptr, ::testing::_))
      .WillRepeatedly(::testing::Return(true));

  bool finished;
  ASSERT_TRUE(mock_section.Step(0x1000, nullptr, &process, &finished));

  DwarfFde fde1{};
  fde1.pc_start = 0x500;
  fde1.pc_end = 0x800;
  fde1.cie = &cie;
  EXPECT_CALL(mock_section, GetFdeOffsetFromPc(0x600, ::testing::_))
      .WillOnce(::testing::Return(true));
  EXPECT_CALL(mock_section, GetFdeFromOffset(::testing::_)).WillOnce(::testing::Return(&fde1));
  EXPECT_CALL(mock_section, GetCfaLocationInfo(0x600, &fde1, ::testing::_))
      .WillOnce(::testing::Invoke(MockGetCfaLocationInfo));

  ASSERT_TRUE(mock_section.Step(0x600, nullptr, &process, &finished));
  ASSERT_TRUE(mock_section.Step(0x700, nullptr, &process, &finished));
}

}  // namespace unwindstack
