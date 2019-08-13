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

#include "DwarfEhFrame.h"
#include "DwarfEncoding.h"

#include "LogFake.h"
#include "MemoryFake.h"

namespace unwindstack {

template <typename TypeParam>
class MockDwarfEhFrame : public DwarfEhFrame<TypeParam> {
 public:
  MockDwarfEhFrame(Memory* memory) : DwarfEhFrame<TypeParam>(memory) {}
  ~MockDwarfEhFrame() = default;

  void TestSetFdeCount(uint64_t count) { this->fde_count_ = count; }
  void TestSetOffset(uint64_t offset) { this->entries_offset_ = offset; }
  void TestSetEndOffset(uint64_t offset) { this->entries_end_ = offset; }
  void TestPushFdeInfo(const typename DwarfEhFrame<TypeParam>::FdeInfo& info) {
    this->fdes_.push_back(info);
  }

  uint64_t TestGetFdeCount() { return this->fde_count_; }
  uint8_t TestGetOffset() { return this->offset_; }
  uint8_t TestGetEndOffset() { return this->end_offset_; }
  void TestGetFdeInfo(size_t index, typename DwarfEhFrame<TypeParam>::FdeInfo* info) {
    *info = this->fdes_[index];
  }
};

template <typename TypeParam>
class DwarfEhFrameTest : public ::testing::Test {
 protected:
  void SetUp() override {
    memory_.Clear();
    eh_frame_ = new MockDwarfEhFrame<TypeParam>(&memory_);
    ResetLogs();
  }

  void TearDown() override { delete eh_frame_; }

  MemoryFake memory_;
  MockDwarfEhFrame<TypeParam>* eh_frame_ = nullptr;
};
TYPED_TEST_CASE_P(DwarfEhFrameTest);

// NOTE: All test class variables need to be referenced as this->.

TYPED_TEST_P(DwarfEhFrameTest, Init32) {
  // CIE 32 information.
  this->memory_.SetData32(0x5000, 0xfc);
  this->memory_.SetData32(0x5004, 0);
  this->memory_.SetData8(0x5008, 1);
  this->memory_.SetData8(0x5009, '\0');

  // FDE 32 information.
  this->memory_.SetData32(0x5100, 0xfc);
  this->memory_.SetData32(0x5104, 0x104);
  this->memory_.SetData32(0x5108, 0x1500);
  this->memory_.SetData32(0x510c, 0x200);

  this->memory_.SetData32(0x5200, 0xfc);
  this->memory_.SetData32(0x5204, 0x204);
  this->memory_.SetData32(0x5208, 0x2500);
  this->memory_.SetData32(0x520c, 0x300);

  // CIE 32 information.
  this->memory_.SetData32(0x5300, 0xfc);
  this->memory_.SetData32(0x5304, 0);
  this->memory_.SetData8(0x5308, 1);
  this->memory_.SetData8(0x5309, '\0');

  // FDE 32 information.
  this->memory_.SetData32(0x5400, 0xfc);
  this->memory_.SetData32(0x5404, 0x104);
  this->memory_.SetData32(0x5408, 0x3500);
  this->memory_.SetData32(0x540c, 0x400);

  this->memory_.SetData32(0x5500, 0xfc);
  this->memory_.SetData32(0x5504, 0x204);
  this->memory_.SetData32(0x5508, 0x4500);
  this->memory_.SetData32(0x550c, 0x500);

  ASSERT_TRUE(this->eh_frame_->Init(0x5000, 0x600));
  ASSERT_EQ(4U, this->eh_frame_->TestGetFdeCount());

  typename DwarfEhFrame<TypeParam>::FdeInfo info(0, 0, 0);

  this->eh_frame_->TestGetFdeInfo(0, &info);
  EXPECT_EQ(0x5100U, info.offset);
  EXPECT_EQ(0x6608U, info.start);
  EXPECT_EQ(0x6808U, info.end);

  this->eh_frame_->TestGetFdeInfo(1, &info);
  EXPECT_EQ(0x5200U, info.offset);
  EXPECT_EQ(0x7708U, info.start);
  EXPECT_EQ(0x7a08U, info.end);

  this->eh_frame_->TestGetFdeInfo(2, &info);
  EXPECT_EQ(0x5400U, info.offset);
  EXPECT_EQ(0x8908U, info.start);
  EXPECT_EQ(0x8d08U, info.end);

  this->eh_frame_->TestGetFdeInfo(3, &info);
  EXPECT_EQ(0x5500U, info.offset);
  EXPECT_EQ(0x9a08U, info.start);
  EXPECT_EQ(0x9f08U, info.end);
}

TYPED_TEST_P(DwarfEhFrameTest, Init32_fde_not_following_cie) {
  // CIE 32 information.
  this->memory_.SetData32(0x5000, 0xfc);
  this->memory_.SetData32(0x5004, 0);
  this->memory_.SetData8(0x5008, 1);
  this->memory_.SetData8(0x5009, '\0');

  // FDE 32 information.
  this->memory_.SetData32(0x5100, 0xfc);
  this->memory_.SetData32(0x5104, 0x1000);
  this->memory_.SetData32(0x5108, 0x1500);
  this->memory_.SetData32(0x510c, 0x200);

  ASSERT_FALSE(this->eh_frame_->Init(0x5000, 0x600));
  ASSERT_EQ(DWARF_ERROR_ILLEGAL_VALUE, this->eh_frame_->LastErrorCode());
}

TYPED_TEST_P(DwarfEhFrameTest, Init64) {
  // CIE 64 information.
  this->memory_.SetData32(0x5000, 0xffffffff);
  this->memory_.SetData64(0x5004, 0xf4);
  this->memory_.SetData64(0x500c, 0);
  this->memory_.SetData8(0x5014, 1);
  this->memory_.SetData8(0x5015, '\0');

  // FDE 64 information.
  this->memory_.SetData32(0x5100, 0xffffffff);
  this->memory_.SetData64(0x5104, 0xf4);
  this->memory_.SetData64(0x510c, 0x10c);
  this->memory_.SetData64(0x5114, 0x1500);
  this->memory_.SetData64(0x511c, 0x200);

  this->memory_.SetData32(0x5200, 0xffffffff);
  this->memory_.SetData64(0x5204, 0xf4);
  this->memory_.SetData64(0x520c, 0x20c);
  this->memory_.SetData64(0x5214, 0x2500);
  this->memory_.SetData64(0x521c, 0x300);

  // CIE 64 information.
  this->memory_.SetData32(0x5300, 0xffffffff);
  this->memory_.SetData64(0x5304, 0xf4);
  this->memory_.SetData64(0x530c, 0);
  this->memory_.SetData8(0x5314, 1);
  this->memory_.SetData8(0x5315, '\0');

  // FDE 64 information.
  this->memory_.SetData32(0x5400, 0xffffffff);
  this->memory_.SetData64(0x5404, 0xf4);
  this->memory_.SetData64(0x540c, 0x10c);
  this->memory_.SetData64(0x5414, 0x3500);
  this->memory_.SetData64(0x541c, 0x400);

  this->memory_.SetData32(0x5500, 0xffffffff);
  this->memory_.SetData64(0x5504, 0xf4);
  this->memory_.SetData64(0x550c, 0x20c);
  this->memory_.SetData64(0x5514, 0x4500);
  this->memory_.SetData64(0x551c, 0x500);

  ASSERT_TRUE(this->eh_frame_->Init(0x5000, 0x600));
  ASSERT_EQ(4U, this->eh_frame_->TestGetFdeCount());

  typename DwarfEhFrame<TypeParam>::FdeInfo info(0, 0, 0);

  this->eh_frame_->TestGetFdeInfo(0, &info);
  EXPECT_EQ(0x5100U, info.offset);
  EXPECT_EQ(0x6618U, info.start);
  EXPECT_EQ(0x6818U, info.end);

  this->eh_frame_->TestGetFdeInfo(1, &info);
  EXPECT_EQ(0x5200U, info.offset);
  EXPECT_EQ(0x7718U, info.start);
  EXPECT_EQ(0x7a18U, info.end);

  this->eh_frame_->TestGetFdeInfo(2, &info);
  EXPECT_EQ(0x5400U, info.offset);
  EXPECT_EQ(0x8918U, info.start);
  EXPECT_EQ(0x8d18U, info.end);

  this->eh_frame_->TestGetFdeInfo(3, &info);
  EXPECT_EQ(0x5500U, info.offset);
  EXPECT_EQ(0x9a18U, info.start);
  EXPECT_EQ(0x9f18U, info.end);
}

TYPED_TEST_P(DwarfEhFrameTest, Init64_fde_not_following_cie) {
  // CIE 64 information.
  this->memory_.SetData32(0x5000, 0xffffffff);
  this->memory_.SetData64(0x5004, 0xf4);
  this->memory_.SetData64(0x500c, 0);
  this->memory_.SetData8(0x5014, 1);
  this->memory_.SetData8(0x5015, '\0');

  // FDE 64 information.
  this->memory_.SetData32(0x5100, 0xffffffff);
  this->memory_.SetData64(0x5104, 0xf4);
  this->memory_.SetData64(0x510c, 0x1000);
  this->memory_.SetData64(0x5114, 0x1500);
  this->memory_.SetData64(0x511c, 0x200);

  ASSERT_FALSE(this->eh_frame_->Init(0x5000, 0x600));
  ASSERT_EQ(DWARF_ERROR_ILLEGAL_VALUE, this->eh_frame_->LastErrorCode());
}

TYPED_TEST_P(DwarfEhFrameTest, Init_version1) {
  // CIE 32 information.
  this->memory_.SetData32(0x5000, 0xfc);
  this->memory_.SetData32(0x5004, 0);
  this->memory_.SetData8(0x5008, 1);
  // Augment string.
  this->memory_.SetMemory(0x5009, std::vector<uint8_t>{'z', 'R', 'P', 'L', '\0'});
  // Code alignment factor.
  this->memory_.SetMemory(0x500e, std::vector<uint8_t>{0x80, 0x00});
  // Data alignment factor.
  this->memory_.SetMemory(0x5010, std::vector<uint8_t>{0x81, 0x80, 0x80, 0x00});
  // Return address register
  this->memory_.SetData8(0x5014, 0x84);
  // Augmentation length
  this->memory_.SetMemory(0x5015, std::vector<uint8_t>{0x84, 0x00});
  // R data.
  this->memory_.SetData8(0x5017, DW_EH_PE_pcrel | DW_EH_PE_udata2);

  // FDE 32 information.
  this->memory_.SetData32(0x5100, 0xfc);
  this->memory_.SetData32(0x5104, 0x104);
  this->memory_.SetData16(0x5108, 0x1500);
  this->memory_.SetData16(0x510a, 0x200);

  ASSERT_TRUE(this->eh_frame_->Init(0x5000, 0x200));
  ASSERT_EQ(1U, this->eh_frame_->TestGetFdeCount());

  typename DwarfEhFrame<TypeParam>::FdeInfo info(0, 0, 0);
  this->eh_frame_->TestGetFdeInfo(0, &info);
  EXPECT_EQ(0x5100U, info.offset);
  EXPECT_EQ(0x6606U, info.start);
  EXPECT_EQ(0x6806U, info.end);
}

TYPED_TEST_P(DwarfEhFrameTest, Init_version4) {
  // CIE 32 information.
  this->memory_.SetData32(0x5000, 0xfc);
  this->memory_.SetData32(0x5004, 0);
  this->memory_.SetData8(0x5008, 4);
  // Augment string.
  this->memory_.SetMemory(0x5009, std::vector<uint8_t>{'z', 'L', 'P', 'R', '\0'});
  // Address size.
  this->memory_.SetData8(0x500e, 4);
  // Segment size.
  this->memory_.SetData8(0x500f, 0);
  // Code alignment factor.
  this->memory_.SetMemory(0x5010, std::vector<uint8_t>{0x80, 0x00});
  // Data alignment factor.
  this->memory_.SetMemory(0x5012, std::vector<uint8_t>{0x81, 0x80, 0x80, 0x00});
  // Return address register
  this->memory_.SetMemory(0x5016, std::vector<uint8_t>{0x85, 0x10});
  // Augmentation length
  this->memory_.SetMemory(0x5018, std::vector<uint8_t>{0x84, 0x00});
  // L data.
  this->memory_.SetData8(0x501a, 0x10);
  // P data.
  this->memory_.SetData8(0x501b, DW_EH_PE_udata4);
  this->memory_.SetData32(0x501c, 0x100);
  // R data.
  this->memory_.SetData8(0x5020, DW_EH_PE_pcrel | DW_EH_PE_udata2);

  // FDE 32 information.
  this->memory_.SetData32(0x5100, 0xfc);
  this->memory_.SetData32(0x5104, 0x104);
  this->memory_.SetData16(0x5108, 0x1500);
  this->memory_.SetData16(0x510a, 0x200);

  ASSERT_TRUE(this->eh_frame_->Init(0x5000, 0x200));
  ASSERT_EQ(1U, this->eh_frame_->TestGetFdeCount());

  typename DwarfEhFrame<TypeParam>::FdeInfo info(0, 0, 0);
  this->eh_frame_->TestGetFdeInfo(0, &info);
  EXPECT_EQ(0x5100U, info.offset);
  EXPECT_EQ(0x6606U, info.start);
  EXPECT_EQ(0x6806U, info.end);
}

TYPED_TEST_P(DwarfEhFrameTest, GetFdeOffsetFromPc) {
  typename DwarfEhFrame<TypeParam>::FdeInfo info(0, 0, 0);
  for (size_t i = 0; i < 9; i++) {
    info.start = 0x1000 * (i + 1);
    info.end = 0x1000 * (i + 2) - 0x10;
    info.offset = 0x5000 + i * 0x20;
    this->eh_frame_->TestPushFdeInfo(info);
  }

  this->eh_frame_->TestSetFdeCount(0);
  uint64_t fde_offset;
  ASSERT_FALSE(this->eh_frame_->GetFdeOffsetFromPc(0x1000, &fde_offset));
  ASSERT_EQ(DWARF_ERROR_NONE, this->eh_frame_->LastErrorCode());

  this->eh_frame_->TestSetFdeCount(9);
  ASSERT_FALSE(this->eh_frame_->GetFdeOffsetFromPc(0x100, &fde_offset));
  ASSERT_EQ(DWARF_ERROR_NONE, this->eh_frame_->LastErrorCode());
  // Odd number of elements.
  for (size_t i = 0; i < 9; i++) {
    TypeParam pc = 0x1000 * (i + 1);
    ASSERT_TRUE(this->eh_frame_->GetFdeOffsetFromPc(pc, &fde_offset)) << "Failed at index " << i;
    EXPECT_EQ(0x5000 + i * 0x20, fde_offset) << "Failed at index " << i;
    ASSERT_TRUE(this->eh_frame_->GetFdeOffsetFromPc(pc + 1, &fde_offset)) << "Failed at index " << i;
    EXPECT_EQ(0x5000 + i * 0x20, fde_offset) << "Failed at index " << i;
    ASSERT_TRUE(this->eh_frame_->GetFdeOffsetFromPc(pc + 0xeff, &fde_offset))
        << "Failed at index " << i;
    EXPECT_EQ(0x5000 + i * 0x20, fde_offset) << "Failed at index " << i;
    ASSERT_FALSE(this->eh_frame_->GetFdeOffsetFromPc(pc + 0xfff, &fde_offset))
        << "Failed at index " << i;
    ASSERT_EQ(DWARF_ERROR_NONE, this->eh_frame_->LastErrorCode());
  }

  // Even number of elements.
  this->eh_frame_->TestSetFdeCount(10);
  info.start = 0xa000;
  info.end = 0xaff0;
  info.offset = 0x5120;
  this->eh_frame_->TestPushFdeInfo(info);

  for (size_t i = 0; i < 10; i++) {
    TypeParam pc = 0x1000 * (i + 1);
    ASSERT_TRUE(this->eh_frame_->GetFdeOffsetFromPc(pc, &fde_offset)) << "Failed at index " << i;
    EXPECT_EQ(0x5000 + i * 0x20, fde_offset) << "Failed at index " << i;
    ASSERT_TRUE(this->eh_frame_->GetFdeOffsetFromPc(pc + 1, &fde_offset)) << "Failed at index " << i;
    EXPECT_EQ(0x5000 + i * 0x20, fde_offset) << "Failed at index " << i;
    ASSERT_TRUE(this->eh_frame_->GetFdeOffsetFromPc(pc + 0xeff, &fde_offset))
        << "Failed at index " << i;
    EXPECT_EQ(0x5000 + i * 0x20, fde_offset) << "Failed at index " << i;
    ASSERT_FALSE(this->eh_frame_->GetFdeOffsetFromPc(pc + 0xfff, &fde_offset))
        << "Failed at index " << i;
    ASSERT_EQ(DWARF_ERROR_NONE, this->eh_frame_->LastErrorCode());
  }
}

TYPED_TEST_P(DwarfEhFrameTest, GetCieFde32) {
  this->eh_frame_->TestSetOffset(0x4000);

  // CIE 32 information.
  this->memory_.SetData32(0xf000, 0x100);
  this->memory_.SetData32(0xf004, 0);
  this->memory_.SetData8(0xf008, 0x1);
  this->memory_.SetData8(0xf009, '\0');
  this->memory_.SetData8(0xf00a, 4);
  this->memory_.SetData8(0xf00b, 8);
  this->memory_.SetData8(0xf00c, 0x20);

  // FDE 32 information.
  this->memory_.SetData32(0x14000, 0x20);
  this->memory_.SetData32(0x14004, 0x5004);
  this->memory_.SetData32(0x14008, 0x9000);
  this->memory_.SetData32(0x1400c, 0x100);

  const DwarfFde* fde = this->eh_frame_->GetFdeFromOffset(0x14000);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x14010U, fde->cfa_instructions_offset);
  EXPECT_EQ(0x14024U, fde->cfa_instructions_end);
  EXPECT_EQ(0x1d008U, fde->pc_start);
  EXPECT_EQ(0x1d108U, fde->pc_end);
  EXPECT_EQ(0xf000U, fde->cie_offset);
  EXPECT_EQ(0U, fde->lsda_address);

  ASSERT_TRUE(fde->cie != nullptr);
  EXPECT_EQ(1U, fde->cie->version);
  EXPECT_EQ(DW_EH_PE_sdata4, fde->cie->fde_address_encoding);
  EXPECT_EQ(DW_EH_PE_omit, fde->cie->lsda_encoding);
  EXPECT_EQ(0U, fde->cie->segment_size);
  EXPECT_EQ(1U, fde->cie->augmentation_string.size());
  EXPECT_EQ('\0', fde->cie->augmentation_string[0]);
  EXPECT_EQ(0U, fde->cie->personality_handler);
  EXPECT_EQ(0xf00dU, fde->cie->cfa_instructions_offset);
  EXPECT_EQ(0xf104U, fde->cie->cfa_instructions_end);
  EXPECT_EQ(4U, fde->cie->code_alignment_factor);
  EXPECT_EQ(8, fde->cie->data_alignment_factor);
  EXPECT_EQ(0x20U, fde->cie->return_address_register);
}

TYPED_TEST_P(DwarfEhFrameTest, GetCieFde64) {
  this->eh_frame_->TestSetOffset(0x2000);

  // CIE 64 information.
  this->memory_.SetData32(0x6000, 0xffffffff);
  this->memory_.SetData64(0x6004, 0x100);
  this->memory_.SetData64(0x600c, 0);
  this->memory_.SetData8(0x6014, 0x1);
  this->memory_.SetData8(0x6015, '\0');
  this->memory_.SetData8(0x6016, 4);
  this->memory_.SetData8(0x6017, 8);
  this->memory_.SetData8(0x6018, 0x20);

  // FDE 64 information.
  this->memory_.SetData32(0x8000, 0xffffffff);
  this->memory_.SetData64(0x8004, 0x200);
  this->memory_.SetData64(0x800c, 0x200c);
  this->memory_.SetData64(0x8014, 0x5000);
  this->memory_.SetData64(0x801c, 0x300);

  const DwarfFde* fde = this->eh_frame_->GetFdeFromOffset(0x8000);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x8024U, fde->cfa_instructions_offset);
  EXPECT_EQ(0x820cU, fde->cfa_instructions_end);
  EXPECT_EQ(0xd018U, fde->pc_start);
  EXPECT_EQ(0xd318U, fde->pc_end);
  EXPECT_EQ(0x6000U, fde->cie_offset);
  EXPECT_EQ(0U, fde->lsda_address);

  ASSERT_TRUE(fde->cie != nullptr);
  EXPECT_EQ(1U, fde->cie->version);
  EXPECT_EQ(DW_EH_PE_sdata8, fde->cie->fde_address_encoding);
  EXPECT_EQ(DW_EH_PE_omit, fde->cie->lsda_encoding);
  EXPECT_EQ(0U, fde->cie->segment_size);
  EXPECT_EQ(1U, fde->cie->augmentation_string.size());
  EXPECT_EQ('\0', fde->cie->augmentation_string[0]);
  EXPECT_EQ(0U, fde->cie->personality_handler);
  EXPECT_EQ(0x6019U, fde->cie->cfa_instructions_offset);
  EXPECT_EQ(0x610cU, fde->cie->cfa_instructions_end);
  EXPECT_EQ(4U, fde->cie->code_alignment_factor);
  EXPECT_EQ(8, fde->cie->data_alignment_factor);
  EXPECT_EQ(0x20U, fde->cie->return_address_register);
}

REGISTER_TYPED_TEST_CASE_P(DwarfEhFrameTest, Init32, Init32_fde_not_following_cie, Init64,
                           Init64_fde_not_following_cie, Init_version1, Init_version4,
                           GetFdeOffsetFromPc, GetCieFde32, GetCieFde64);

typedef ::testing::Types<uint32_t, uint64_t> DwarfEhFrameTestTypes;
INSTANTIATE_TYPED_TEST_CASE_P(, DwarfEhFrameTest, DwarfEhFrameTestTypes);

}  // namespace unwindstack
