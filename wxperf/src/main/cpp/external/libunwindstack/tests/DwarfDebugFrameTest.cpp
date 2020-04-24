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

#include <vector>

#include <gtest/gtest.h>

#include <unwindstack/DwarfError.h>

#include "DwarfDebugFrame.h"
#include "DwarfEncoding.h"

#include "LogFake.h"
#include "MemoryFake.h"

namespace unwindstack {

template <typename TypeParam>
class DwarfDebugFrameTest : public ::testing::Test {
 protected:
  void SetUp() override {
    memory_.Clear();
    debug_frame_ = new DwarfDebugFrame<TypeParam>(&memory_);
    ResetLogs();
  }

  void TearDown() override { delete debug_frame_; }

  MemoryFake memory_;
  DwarfDebugFrame<TypeParam>* debug_frame_ = nullptr;
};
TYPED_TEST_CASE_P(DwarfDebugFrameTest);

// NOTE: All test class variables need to be referenced as this->.

static void SetCie32(MemoryFake* memory, uint64_t offset, uint32_t length,
                     std::vector<uint8_t> data) {
  memory->SetData32(offset, length);
  offset += 4;
  // Indicates this is a cie.
  memory->SetData32(offset, 0xffffffff);
  offset += 4;
  memory->SetMemory(offset, data);
}

static void SetCie64(MemoryFake* memory, uint64_t offset, uint64_t length,
                     std::vector<uint8_t> data) {
  memory->SetData32(offset, 0xffffffff);
  offset += 4;
  memory->SetData64(offset, length);
  offset += 8;
  // Indicates this is a cie.
  memory->SetData64(offset, 0xffffffffffffffffUL);
  offset += 8;
  memory->SetMemory(offset, data);
}

static void SetFde32(MemoryFake* memory, uint64_t offset, uint32_t length, uint64_t cie_offset,
                     uint32_t pc_start, uint32_t pc_length, uint64_t segment_length = 0,
                     std::vector<uint8_t>* data = nullptr) {
  memory->SetData32(offset, length);
  offset += 4;
  memory->SetData32(offset, cie_offset);
  offset += 4 + segment_length;
  memory->SetData32(offset, pc_start);
  offset += 4;
  memory->SetData32(offset, pc_length);
  if (data != nullptr) {
    offset += 4;
    memory->SetMemory(offset, *data);
  }
}

static void SetFde64(MemoryFake* memory, uint64_t offset, uint64_t length, uint64_t cie_offset,
                     uint64_t pc_start, uint64_t pc_length, uint64_t segment_length = 0,
                     std::vector<uint8_t>* data = nullptr) {
  memory->SetData32(offset, 0xffffffff);
  offset += 4;
  memory->SetData64(offset, length);
  offset += 8;
  memory->SetData64(offset, cie_offset);
  offset += 8 + segment_length;
  memory->SetData64(offset, pc_start);
  offset += 8;
  memory->SetData64(offset, pc_length);
  if (data != nullptr) {
    offset += 8;
    memory->SetMemory(offset, *data);
  }
}

static void SetFourFdes32(MemoryFake* memory) {
  SetCie32(memory, 0x5000, 0xfc, std::vector<uint8_t>{1, '\0', 0, 0, 1});

  // FDE 32 information.
  SetFde32(memory, 0x5100, 0xfc, 0, 0x1500, 0x200);
  SetFde32(memory, 0x5200, 0xfc, 0, 0x2500, 0x300);

  // CIE 32 information.
  SetCie32(memory, 0x5300, 0xfc, std::vector<uint8_t>{1, '\0', 0, 0, 1});

  // FDE 32 information.
  SetFde32(memory, 0x5400, 0xfc, 0x300, 0x3500, 0x400);
  SetFde32(memory, 0x5500, 0xfc, 0x300, 0x4500, 0x500);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetFdes32) {
  SetFourFdes32(&this->memory_);
  ASSERT_TRUE(this->debug_frame_->Init(0x5000, 0x600, 0));

  std::vector<const DwarfFde*> fdes;
  this->debug_frame_->GetFdes(&fdes);

  ASSERT_EQ(4U, fdes.size());

  EXPECT_EQ(0x5000U, fdes[0]->cie_offset);
  EXPECT_EQ(0x5110U, fdes[0]->cfa_instructions_offset);
  EXPECT_EQ(0x5200U, fdes[0]->cfa_instructions_end);
  EXPECT_EQ(0x1500U, fdes[0]->pc_start);
  EXPECT_EQ(0x1700U, fdes[0]->pc_end);
  EXPECT_EQ(0U, fdes[0]->lsda_address);
  EXPECT_TRUE(fdes[0]->cie != nullptr);

  EXPECT_EQ(0x5000U, fdes[1]->cie_offset);
  EXPECT_EQ(0x5210U, fdes[1]->cfa_instructions_offset);
  EXPECT_EQ(0x5300U, fdes[1]->cfa_instructions_end);
  EXPECT_EQ(0x2500U, fdes[1]->pc_start);
  EXPECT_EQ(0x2800U, fdes[1]->pc_end);
  EXPECT_EQ(0U, fdes[1]->lsda_address);
  EXPECT_TRUE(fdes[1]->cie != nullptr);

  EXPECT_EQ(0x5300U, fdes[2]->cie_offset);
  EXPECT_EQ(0x5410U, fdes[2]->cfa_instructions_offset);
  EXPECT_EQ(0x5500U, fdes[2]->cfa_instructions_end);
  EXPECT_EQ(0x3500U, fdes[2]->pc_start);
  EXPECT_EQ(0x3900U, fdes[2]->pc_end);
  EXPECT_EQ(0U, fdes[2]->lsda_address);
  EXPECT_TRUE(fdes[2]->cie != nullptr);

  EXPECT_EQ(0x5300U, fdes[3]->cie_offset);
  EXPECT_EQ(0x5510U, fdes[3]->cfa_instructions_offset);
  EXPECT_EQ(0x5600U, fdes[3]->cfa_instructions_end);
  EXPECT_EQ(0x4500U, fdes[3]->pc_start);
  EXPECT_EQ(0x4a00U, fdes[3]->pc_end);
  EXPECT_EQ(0U, fdes[3]->lsda_address);
  EXPECT_TRUE(fdes[3]->cie != nullptr);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetFdes32_after_GetFdeFromPc) {
  SetFourFdes32(&this->memory_);
  ASSERT_TRUE(this->debug_frame_->Init(0x5000, 0x600, 0));

  const DwarfFde* fde = this->debug_frame_->GetFdeFromPc(0x3600);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x3500U, fde->pc_start);
  EXPECT_EQ(0x3900U, fde->pc_end);

  std::vector<const DwarfFde*> fdes;
  this->debug_frame_->GetFdes(&fdes);
  ASSERT_EQ(4U, fdes.size());

  // Verify that they got added in the correct order.
  EXPECT_EQ(0x1500U, fdes[0]->pc_start);
  EXPECT_EQ(0x1700U, fdes[0]->pc_end);
  EXPECT_EQ(0x2500U, fdes[1]->pc_start);
  EXPECT_EQ(0x2800U, fdes[1]->pc_end);
  EXPECT_EQ(0x3500U, fdes[2]->pc_start);
  EXPECT_EQ(0x3900U, fdes[2]->pc_end);
  EXPECT_EQ(0x4500U, fdes[3]->pc_start);
  EXPECT_EQ(0x4a00U, fdes[3]->pc_end);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetFdes32_not_in_section) {
  SetFourFdes32(&this->memory_);
  ASSERT_TRUE(this->debug_frame_->Init(0x5000, 0x500, 0));

  std::vector<const DwarfFde*> fdes;
  this->debug_frame_->GetFdes(&fdes);

  ASSERT_EQ(3U, fdes.size());
}

TYPED_TEST_P(DwarfDebugFrameTest, GetFdeFromPc32) {
  SetFourFdes32(&this->memory_);
  ASSERT_TRUE(this->debug_frame_->Init(0x5000, 0x600, 0));

  const DwarfFde* fde = this->debug_frame_->GetFdeFromPc(0x1600);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x1500U, fde->pc_start);

  fde = this->debug_frame_->GetFdeFromPc(0x2600);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x2500U, fde->pc_start);

  fde = this->debug_frame_->GetFdeFromPc(0x3600);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x3500U, fde->pc_start);

  fde = this->debug_frame_->GetFdeFromPc(0x4600);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x4500U, fde->pc_start);

  fde = this->debug_frame_->GetFdeFromPc(0);
  ASSERT_TRUE(fde == nullptr);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetFdeFromPc32_reverse) {
  SetFourFdes32(&this->memory_);
  ASSERT_TRUE(this->debug_frame_->Init(0x5000, 0x600, 0));

  const DwarfFde* fde = this->debug_frame_->GetFdeFromPc(0x4600);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x4500U, fde->pc_start);

  fde = this->debug_frame_->GetFdeFromPc(0x3600);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x3500U, fde->pc_start);

  fde = this->debug_frame_->GetFdeFromPc(0x2600);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x2500U, fde->pc_start);

  fde = this->debug_frame_->GetFdeFromPc(0x1600);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x1500U, fde->pc_start);

  fde = this->debug_frame_->GetFdeFromPc(0);
  ASSERT_TRUE(fde == nullptr);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetFdeFromPc32_not_in_section) {
  SetFourFdes32(&this->memory_);
  ASSERT_TRUE(this->debug_frame_->Init(0x5000, 0x500, 0));

  const DwarfFde* fde = this->debug_frame_->GetFdeFromPc(0x4600);
  ASSERT_TRUE(fde == nullptr);
}

static void SetFourFdes64(MemoryFake* memory) {
  // CIE 64 information.
  SetCie64(memory, 0x5000, 0xf4, std::vector<uint8_t>{1, '\0', 0, 0, 1});

  // FDE 64 information.
  SetFde64(memory, 0x5100, 0xf4, 0, 0x1500, 0x200);
  SetFde64(memory, 0x5200, 0xf4, 0, 0x2500, 0x300);

  // CIE 64 information.
  SetCie64(memory, 0x5300, 0xf4, std::vector<uint8_t>{1, '\0', 0, 0, 1});

  // FDE 64 information.
  SetFde64(memory, 0x5400, 0xf4, 0x300, 0x3500, 0x400);
  SetFde64(memory, 0x5500, 0xf4, 0x300, 0x4500, 0x500);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetFdes64) {
  SetFourFdes64(&this->memory_);
  ASSERT_TRUE(this->debug_frame_->Init(0x5000, 0x600, 0));

  std::vector<const DwarfFde*> fdes;
  this->debug_frame_->GetFdes(&fdes);

  ASSERT_EQ(4U, fdes.size());

  EXPECT_EQ(0x5000U, fdes[0]->cie_offset);
  EXPECT_EQ(0x5124U, fdes[0]->cfa_instructions_offset);
  EXPECT_EQ(0x5200U, fdes[0]->cfa_instructions_end);
  EXPECT_EQ(0x1500U, fdes[0]->pc_start);
  EXPECT_EQ(0x1700U, fdes[0]->pc_end);
  EXPECT_EQ(0U, fdes[0]->lsda_address);
  EXPECT_TRUE(fdes[0]->cie != nullptr);

  EXPECT_EQ(0x5000U, fdes[1]->cie_offset);
  EXPECT_EQ(0x5224U, fdes[1]->cfa_instructions_offset);
  EXPECT_EQ(0x5300U, fdes[1]->cfa_instructions_end);
  EXPECT_EQ(0x2500U, fdes[1]->pc_start);
  EXPECT_EQ(0x2800U, fdes[1]->pc_end);
  EXPECT_EQ(0U, fdes[1]->lsda_address);
  EXPECT_TRUE(fdes[1]->cie != nullptr);

  EXPECT_EQ(0x5300U, fdes[2]->cie_offset);
  EXPECT_EQ(0x5424U, fdes[2]->cfa_instructions_offset);
  EXPECT_EQ(0x5500U, fdes[2]->cfa_instructions_end);
  EXPECT_EQ(0x3500U, fdes[2]->pc_start);
  EXPECT_EQ(0x3900U, fdes[2]->pc_end);
  EXPECT_EQ(0U, fdes[2]->lsda_address);
  EXPECT_TRUE(fdes[2]->cie != nullptr);

  EXPECT_EQ(0x5300U, fdes[3]->cie_offset);
  EXPECT_EQ(0x5524U, fdes[3]->cfa_instructions_offset);
  EXPECT_EQ(0x5600U, fdes[3]->cfa_instructions_end);
  EXPECT_EQ(0x4500U, fdes[3]->pc_start);
  EXPECT_EQ(0x4a00U, fdes[3]->pc_end);
  EXPECT_EQ(0U, fdes[3]->lsda_address);
  EXPECT_TRUE(fdes[3]->cie != nullptr);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetFdes64_after_GetFdeFromPc) {
  SetFourFdes64(&this->memory_);
  ASSERT_TRUE(this->debug_frame_->Init(0x5000, 0x600, 0));

  const DwarfFde* fde = this->debug_frame_->GetFdeFromPc(0x2600);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x2500U, fde->pc_start);
  EXPECT_EQ(0x2800U, fde->pc_end);

  std::vector<const DwarfFde*> fdes;
  this->debug_frame_->GetFdes(&fdes);
  ASSERT_EQ(4U, fdes.size());

  // Verify that they got added in the correct order.
  EXPECT_EQ(0x1500U, fdes[0]->pc_start);
  EXPECT_EQ(0x1700U, fdes[0]->pc_end);
  EXPECT_EQ(0x2500U, fdes[1]->pc_start);
  EXPECT_EQ(0x2800U, fdes[1]->pc_end);
  EXPECT_EQ(0x3500U, fdes[2]->pc_start);
  EXPECT_EQ(0x3900U, fdes[2]->pc_end);
  EXPECT_EQ(0x4500U, fdes[3]->pc_start);
  EXPECT_EQ(0x4a00U, fdes[3]->pc_end);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetFdes64_not_in_section) {
  SetFourFdes64(&this->memory_);
  ASSERT_TRUE(this->debug_frame_->Init(0x5000, 0x500, 0));

  std::vector<const DwarfFde*> fdes;
  this->debug_frame_->GetFdes(&fdes);

  ASSERT_EQ(3U, fdes.size());
}

TYPED_TEST_P(DwarfDebugFrameTest, GetFdeFromPc64) {
  SetFourFdes64(&this->memory_);
  ASSERT_TRUE(this->debug_frame_->Init(0x5000, 0x600, 0));

  const DwarfFde* fde = this->debug_frame_->GetFdeFromPc(0x1600);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x1500U, fde->pc_start);

  fde = this->debug_frame_->GetFdeFromPc(0x2600);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x2500U, fde->pc_start);

  fde = this->debug_frame_->GetFdeFromPc(0x3600);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x3500U, fde->pc_start);

  fde = this->debug_frame_->GetFdeFromPc(0x4600);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x4500U, fde->pc_start);

  fde = this->debug_frame_->GetFdeFromPc(0);
  ASSERT_TRUE(fde == nullptr);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetFdeFromPc64_reverse) {
  SetFourFdes64(&this->memory_);
  ASSERT_TRUE(this->debug_frame_->Init(0x5000, 0x600, 0));

  const DwarfFde* fde = this->debug_frame_->GetFdeFromPc(0x4600);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x4500U, fde->pc_start);

  fde = this->debug_frame_->GetFdeFromPc(0x3600);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x3500U, fde->pc_start);

  fde = this->debug_frame_->GetFdeFromPc(0x2600);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x2500U, fde->pc_start);

  fde = this->debug_frame_->GetFdeFromPc(0x1600);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x1500U, fde->pc_start);

  fde = this->debug_frame_->GetFdeFromPc(0);
  ASSERT_TRUE(fde == nullptr);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetFdeFromPc64_not_in_section) {
  SetFourFdes64(&this->memory_);
  ASSERT_TRUE(this->debug_frame_->Init(0x5000, 0x500, 0));

  const DwarfFde* fde = this->debug_frame_->GetFdeFromPc(0x4600);
  ASSERT_TRUE(fde == nullptr);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetCieFde32) {
  SetCie32(&this->memory_, 0xf000, 0x100, std::vector<uint8_t>{1, '\0', 4, 8, 0x20});
  SetFde32(&this->memory_, 0x14000, 0x20, 0xf000, 0x9000, 0x100);

  const DwarfFde* fde = this->debug_frame_->GetFdeFromOffset(0x14000);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x14010U, fde->cfa_instructions_offset);
  EXPECT_EQ(0x14024U, fde->cfa_instructions_end);
  EXPECT_EQ(0x9000U, fde->pc_start);
  EXPECT_EQ(0x9100U, fde->pc_end);
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

TYPED_TEST_P(DwarfDebugFrameTest, GetCieFde64) {
  SetCie64(&this->memory_, 0x6000, 0x100, std::vector<uint8_t>{1, '\0', 4, 8, 0x20});
  SetFde64(&this->memory_, 0x8000, 0x200, 0x6000, 0x5000, 0x300);

  const DwarfFde* fde = this->debug_frame_->GetFdeFromOffset(0x8000);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x8024U, fde->cfa_instructions_offset);
  EXPECT_EQ(0x820cU, fde->cfa_instructions_end);
  EXPECT_EQ(0x5000U, fde->pc_start);
  EXPECT_EQ(0x5300U, fde->pc_end);
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

static void VerifyCieVersion(const DwarfCie* cie, uint8_t version, uint8_t segment_size,
                             uint8_t fde_encoding, uint64_t return_address, uint64_t start_offset,
                             uint64_t end_offset) {
  EXPECT_EQ(version, cie->version);
  EXPECT_EQ(fde_encoding, cie->fde_address_encoding);
  EXPECT_EQ(DW_EH_PE_omit, cie->lsda_encoding);
  EXPECT_EQ(segment_size, cie->segment_size);
  EXPECT_EQ(1U, cie->augmentation_string.size());
  EXPECT_EQ('\0', cie->augmentation_string[0]);
  EXPECT_EQ(0U, cie->personality_handler);
  EXPECT_EQ(4U, cie->code_alignment_factor);
  EXPECT_EQ(8, cie->data_alignment_factor);
  EXPECT_EQ(return_address, cie->return_address_register);
  EXPECT_EQ(0x5000U + start_offset, cie->cfa_instructions_offset);
  EXPECT_EQ(0x5000U + end_offset, cie->cfa_instructions_end);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetCieFromOffset32_cie_cached) {
  SetCie32(&this->memory_, 0x5000, 0x100, std::vector<uint8_t>{1, '\0', 4, 8, 0x20});
  const DwarfCie* cie = this->debug_frame_->GetCieFromOffset(0x5000);
  EXPECT_EQ(DWARF_ERROR_NONE, this->debug_frame_->LastErrorCode());
  ASSERT_TRUE(cie != nullptr);
  VerifyCieVersion(cie, 1, 0, DW_EH_PE_sdata4, 0x20, 0xd, 0x104);

  std::vector<uint8_t> zero(0x100, 0);
  this->memory_.SetMemory(0x5000, zero);
  cie = this->debug_frame_->GetCieFromOffset(0x5000);
  EXPECT_EQ(DWARF_ERROR_NONE, this->debug_frame_->LastErrorCode());
  ASSERT_TRUE(cie != nullptr);
  VerifyCieVersion(cie, 1, 0, DW_EH_PE_sdata4, 0x20, 0xd, 0x104);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetCieFromOffset64_cie_cached) {
  SetCie64(&this->memory_, 0x5000, 0x100, std::vector<uint8_t>{1, '\0', 4, 8, 0x20});
  const DwarfCie* cie = this->debug_frame_->GetCieFromOffset(0x5000);
  EXPECT_EQ(DWARF_ERROR_NONE, this->debug_frame_->LastErrorCode());
  ASSERT_TRUE(cie != nullptr);
  VerifyCieVersion(cie, 1, 0, DW_EH_PE_sdata8, 0x20, 0x19, 0x10c);

  std::vector<uint8_t> zero(0x100, 0);
  this->memory_.SetMemory(0x5000, zero);
  cie = this->debug_frame_->GetCieFromOffset(0x5000);
  EXPECT_EQ(DWARF_ERROR_NONE, this->debug_frame_->LastErrorCode());
  ASSERT_TRUE(cie != nullptr);
  VerifyCieVersion(cie, 1, 0, DW_EH_PE_sdata8, 0x20, 0x19, 0x10c);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetCieFromOffset32_version1) {
  SetCie32(&this->memory_, 0x5000, 0x100, std::vector<uint8_t>{1, '\0', 4, 8, 0x20});
  const DwarfCie* cie = this->debug_frame_->GetCieFromOffset(0x5000);
  EXPECT_EQ(DWARF_ERROR_NONE, this->debug_frame_->LastErrorCode());
  ASSERT_TRUE(cie != nullptr);
  VerifyCieVersion(cie, 1, 0, DW_EH_PE_sdata4, 0x20, 0xd, 0x104);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetCieFromOffset64_version1) {
  SetCie64(&this->memory_, 0x5000, 0x100, std::vector<uint8_t>{1, '\0', 4, 8, 0x20});
  const DwarfCie* cie = this->debug_frame_->GetCieFromOffset(0x5000);
  EXPECT_EQ(DWARF_ERROR_NONE, this->debug_frame_->LastErrorCode());
  ASSERT_TRUE(cie != nullptr);
  VerifyCieVersion(cie, 1, 0, DW_EH_PE_sdata8, 0x20, 0x19, 0x10c);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetCieFromOffset32_version3) {
  SetCie32(&this->memory_, 0x5000, 0x100, std::vector<uint8_t>{3, '\0', 4, 8, 0x81, 3});
  const DwarfCie* cie = this->debug_frame_->GetCieFromOffset(0x5000);
  EXPECT_EQ(DWARF_ERROR_NONE, this->debug_frame_->LastErrorCode());
  ASSERT_TRUE(cie != nullptr);
  VerifyCieVersion(cie, 3, 0, DW_EH_PE_sdata4, 0x181, 0xe, 0x104);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetCieFromOffset64_version3) {
  SetCie64(&this->memory_, 0x5000, 0x100, std::vector<uint8_t>{3, '\0', 4, 8, 0x81, 3});
  const DwarfCie* cie = this->debug_frame_->GetCieFromOffset(0x5000);
  EXPECT_EQ(DWARF_ERROR_NONE, this->debug_frame_->LastErrorCode());
  ASSERT_TRUE(cie != nullptr);
  VerifyCieVersion(cie, 3, 0, DW_EH_PE_sdata8, 0x181, 0x1a, 0x10c);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetCieFromOffset32_version4) {
  SetCie32(&this->memory_, 0x5000, 0x100, std::vector<uint8_t>{4, '\0', 0, 10, 4, 8, 0x81, 3});
  const DwarfCie* cie = this->debug_frame_->GetCieFromOffset(0x5000);
  EXPECT_EQ(DWARF_ERROR_NONE, this->debug_frame_->LastErrorCode());
  ASSERT_TRUE(cie != nullptr);
  VerifyCieVersion(cie, 4, 10, DW_EH_PE_sdata4, 0x181, 0x10, 0x104);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetCieFromOffset64_version4) {
  SetCie64(&this->memory_, 0x5000, 0x100, std::vector<uint8_t>{4, '\0', 0, 10, 4, 8, 0x81, 3});
  const DwarfCie* cie = this->debug_frame_->GetCieFromOffset(0x5000);
  EXPECT_EQ(DWARF_ERROR_NONE, this->debug_frame_->LastErrorCode());
  ASSERT_TRUE(cie != nullptr);
  VerifyCieVersion(cie, 4, 10, DW_EH_PE_sdata8, 0x181, 0x1c, 0x10c);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetCieFromOffset32_version5) {
  SetCie32(&this->memory_, 0x5000, 0x100, std::vector<uint8_t>{5, '\0', 0, 10, 4, 8, 0x81, 3});
  const DwarfCie* cie = this->debug_frame_->GetCieFromOffset(0x5000);
  EXPECT_EQ(DWARF_ERROR_NONE, this->debug_frame_->LastErrorCode());
  ASSERT_TRUE(cie != nullptr);
  VerifyCieVersion(cie, 5, 10, DW_EH_PE_sdata4, 0x181, 0x10, 0x104);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetCieFromOffset64_version5) {
  SetCie64(&this->memory_, 0x5000, 0x100, std::vector<uint8_t>{5, '\0', 0, 10, 4, 8, 0x81, 3});
  const DwarfCie* cie = this->debug_frame_->GetCieFromOffset(0x5000);
  EXPECT_EQ(DWARF_ERROR_NONE, this->debug_frame_->LastErrorCode());
  ASSERT_TRUE(cie != nullptr);
  VerifyCieVersion(cie, 5, 10, DW_EH_PE_sdata8, 0x181, 0x1c, 0x10c);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetCieFromOffset_version_invalid) {
  SetCie32(&this->memory_, 0x5000, 0x100, std::vector<uint8_t>{0, '\0', 1, 2, 3, 4, 5, 6, 7});
  ASSERT_TRUE(this->debug_frame_->GetCieFromOffset(0x5000) == nullptr);
  EXPECT_EQ(DWARF_ERROR_UNSUPPORTED_VERSION, this->debug_frame_->LastErrorCode());
  SetCie64(&this->memory_, 0x6000, 0x100, std::vector<uint8_t>{0, '\0', 1, 2, 3, 4, 5, 6, 7});
  ASSERT_TRUE(this->debug_frame_->GetCieFromOffset(0x6000) == nullptr);
  EXPECT_EQ(DWARF_ERROR_UNSUPPORTED_VERSION, this->debug_frame_->LastErrorCode());

  SetCie32(&this->memory_, 0x7000, 0x100, std::vector<uint8_t>{6, '\0', 1, 2, 3, 4, 5, 6, 7});
  ASSERT_TRUE(this->debug_frame_->GetCieFromOffset(0x7000) == nullptr);
  EXPECT_EQ(DWARF_ERROR_UNSUPPORTED_VERSION, this->debug_frame_->LastErrorCode());
  SetCie64(&this->memory_, 0x8000, 0x100, std::vector<uint8_t>{6, '\0', 1, 2, 3, 4, 5, 6, 7});
  ASSERT_TRUE(this->debug_frame_->GetCieFromOffset(0x8000) == nullptr);
  EXPECT_EQ(DWARF_ERROR_UNSUPPORTED_VERSION, this->debug_frame_->LastErrorCode());
}

static void VerifyCieAugment(const DwarfCie* cie, uint64_t inst_offset, uint64_t inst_end) {
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
  EXPECT_EQ(4U, cie->code_alignment_factor);
  EXPECT_EQ(8, cie->data_alignment_factor);
  EXPECT_EQ(0x10U, cie->return_address_register);
  EXPECT_EQ(inst_offset, cie->cfa_instructions_offset);
  EXPECT_EQ(inst_end, cie->cfa_instructions_end);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetCieFromOffset32_augment) {
  SetCie32(&this->memory_, 0x5000, 0x100,
           std::vector<uint8_t>{/* version */ 1,
                                /* augment string */ 'z', 'L', 'P', 'R', '\0',
                                /* code alignment factor */ 4,
                                /* data alignment factor */ 8,
                                /* return address register */ 0x10,
                                /* augment length */ 0xf,
                                /* L data */ DW_EH_PE_textrel | DW_EH_PE_udata2,
                                /* P data */ DW_EH_PE_udata4, 0x78, 0x56, 0x34, 0x12,
                                /* R data */ DW_EH_PE_udata2});

  const DwarfCie* cie = this->debug_frame_->GetCieFromOffset(0x5000);
  ASSERT_TRUE(cie != nullptr);
  VerifyCieAugment(cie, 0x5021, 0x5104);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetCieFromOffset64_augment) {
  SetCie64(&this->memory_, 0x5000, 0x100,
           std::vector<uint8_t>{/* version */ 1,
                                /* augment string */ 'z', 'L', 'P', 'R', '\0',
                                /* code alignment factor */ 4,
                                /* data alignment factor */ 8,
                                /* return address register */ 0x10,
                                /* augment length */ 0xf,
                                /* L data */ DW_EH_PE_textrel | DW_EH_PE_udata2,
                                /* P data */ DW_EH_PE_udata4, 0x78, 0x56, 0x34, 0x12,
                                /* R data */ DW_EH_PE_udata2});

  const DwarfCie* cie = this->debug_frame_->GetCieFromOffset(0x5000);
  ASSERT_TRUE(cie != nullptr);
  VerifyCieAugment(cie, 0x502d, 0x510c);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetFdeFromOffset32_augment) {
  SetCie32(&this->memory_, 0x5000, 0xfc,
           std::vector<uint8_t>{/* version */ 4,
                                /* augment string */ 'z', '\0',
                                /* address size */ 8,
                                /* segment size */ 0x10,
                                /* code alignment factor */ 16,
                                /* data alignment factor */ 32,
                                /* return address register */ 10,
                                /* augment length */ 0x0});

  std::vector<uint8_t> data{/* augment length */ 0x80, 0x3};
  SetFde32(&this->memory_, 0x5200, 0x300, 0x5000, 0x4300, 0x300, 0x10, &data);

  const DwarfFde* fde = this->debug_frame_->GetFdeFromOffset(0x5200);
  ASSERT_TRUE(fde != nullptr);
  ASSERT_TRUE(fde->cie != nullptr);
  EXPECT_EQ(4U, fde->cie->version);
  EXPECT_EQ(0x5000U, fde->cie_offset);
  EXPECT_EQ(0x53a2U, fde->cfa_instructions_offset);
  EXPECT_EQ(0x5504U, fde->cfa_instructions_end);
  EXPECT_EQ(0x4300U, fde->pc_start);
  EXPECT_EQ(0x4600U, fde->pc_end);
  EXPECT_EQ(0U, fde->lsda_address);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetFdeFromOffset64_augment) {
  SetCie64(&this->memory_, 0x5000, 0xfc,
           std::vector<uint8_t>{/* version */ 4,
                                /* augment string */ 'z', '\0',
                                /* address size */ 8,
                                /* segment size */ 0x10,
                                /* code alignment factor */ 16,
                                /* data alignment factor */ 32,
                                /* return address register */ 10,
                                /* augment length */ 0x0});

  std::vector<uint8_t> data{/* augment length */ 0x80, 0x3};
  SetFde64(&this->memory_, 0x5200, 0x300, 0x5000, 0x4300, 0x300, 0x10, &data);

  const DwarfFde* fde = this->debug_frame_->GetFdeFromOffset(0x5200);
  ASSERT_TRUE(fde != nullptr);
  ASSERT_TRUE(fde->cie != nullptr);
  EXPECT_EQ(4U, fde->cie->version);
  EXPECT_EQ(0x5000U, fde->cie_offset);
  EXPECT_EQ(0x53b6U, fde->cfa_instructions_offset);
  EXPECT_EQ(0x550cU, fde->cfa_instructions_end);
  EXPECT_EQ(0x4300U, fde->pc_start);
  EXPECT_EQ(0x4600U, fde->pc_end);
  EXPECT_EQ(0U, fde->lsda_address);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetFdeFromOffset32_lsda_address) {
  SetCie32(&this->memory_, 0x5000, 0xfc,
           std::vector<uint8_t>{/* version */ 1,
                                /* augment string */ 'z', 'L', '\0',
                                /* address size */ 8,
                                /* code alignment factor */ 16,
                                /* data alignment factor */ 32,
                                /* return address register */ 10,
                                /* augment length */ 0x2,
                                /* L data */ DW_EH_PE_udata2});

  std::vector<uint8_t> data{/* augment length */ 0x80, 0x3,
                            /* lsda address */ 0x20, 0x45};
  SetFde32(&this->memory_, 0x5200, 0x300, 0x5000, 0x4300, 0x300, 0, &data);

  const DwarfFde* fde = this->debug_frame_->GetFdeFromOffset(0x5200);
  ASSERT_TRUE(fde != nullptr);
  ASSERT_TRUE(fde->cie != nullptr);
  EXPECT_EQ(1U, fde->cie->version);
  EXPECT_EQ(0x5000U, fde->cie_offset);
  EXPECT_EQ(0x5392U, fde->cfa_instructions_offset);
  EXPECT_EQ(0x5504U, fde->cfa_instructions_end);
  EXPECT_EQ(0x4300U, fde->pc_start);
  EXPECT_EQ(0x4600U, fde->pc_end);
  EXPECT_EQ(0x4520U, fde->lsda_address);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetFdeFromOffset64_lsda_address) {
  SetCie64(&this->memory_, 0x5000, 0xfc,
           std::vector<uint8_t>{/* version */ 1,
                                /* augment string */ 'z', 'L', '\0',
                                /* address size */ 8,
                                /* code alignment factor */ 16,
                                /* data alignment factor */ 32,
                                /* return address register */ 10,
                                /* augment length */ 0x2,
                                /* L data */ DW_EH_PE_udata2});

  std::vector<uint8_t> data{/* augment length */ 0x80, 0x3,
                            /* lsda address */ 0x20, 0x45};
  SetFde64(&this->memory_, 0x5200, 0x300, 0x5000, 0x4300, 0x300, 0, &data);

  const DwarfFde* fde = this->debug_frame_->GetFdeFromOffset(0x5200);
  ASSERT_TRUE(fde != nullptr);
  ASSERT_TRUE(fde->cie != nullptr);
  EXPECT_EQ(1U, fde->cie->version);
  EXPECT_EQ(0x5000U, fde->cie_offset);
  EXPECT_EQ(0x53a6U, fde->cfa_instructions_offset);
  EXPECT_EQ(0x550cU, fde->cfa_instructions_end);
  EXPECT_EQ(0x4300U, fde->pc_start);
  EXPECT_EQ(0x4600U, fde->pc_end);
  EXPECT_EQ(0x4520U, fde->lsda_address);
}

TYPED_TEST_P(DwarfDebugFrameTest, GetFdeFromPc_interleaved) {
  SetCie32(&this->memory_, 0x5000, 0xfc, std::vector<uint8_t>{1, '\0', 0, 0, 1});

  // FDE 0 (0x100 - 0x200)
  SetFde32(&this->memory_, 0x5100, 0xfc, 0, 0x100, 0x100);
  // FDE 1 (0x300 - 0x500)
  SetFde32(&this->memory_, 0x5200, 0xfc, 0, 0x300, 0x200);
  // FDE 2 (0x700 - 0x800)
  SetFde32(&this->memory_, 0x5300, 0xfc, 0, 0x700, 0x100);
  // FDE 3 (0xa00 - 0xb00)
  SetFde32(&this->memory_, 0x5400, 0xfc, 0, 0xa00, 0x100);
  // FDE 4 (0x100 - 0xb00)
  SetFde32(&this->memory_, 0x5500, 0xfc, 0, 0x150, 0xa00);
  // FDE 5 (0x0 - 0x50)
  SetFde32(&this->memory_, 0x5600, 0xfc, 0, 0, 0x50);

  this->debug_frame_->Init(0x5000, 0x700, 0);

  // Force reading all entries so no entries are found.
  const DwarfFde* fde = this->debug_frame_->GetFdeFromPc(0xfffff);
  ASSERT_TRUE(fde == nullptr);

  //   0x0   - 0x50   FDE 5
  fde = this->debug_frame_->GetFdeFromPc(0x10);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0U, fde->pc_start);
  EXPECT_EQ(0x50U, fde->pc_end);

  //   0x100 - 0x200  FDE 0
  fde = this->debug_frame_->GetFdeFromPc(0x170);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x100U, fde->pc_start);
  EXPECT_EQ(0x200U, fde->pc_end);

  //   0x200 - 0x300  FDE 4
  fde = this->debug_frame_->GetFdeFromPc(0x210);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x150U, fde->pc_start);
  EXPECT_EQ(0xb50U, fde->pc_end);

  //   0x300 - 0x500  FDE 1
  fde = this->debug_frame_->GetFdeFromPc(0x310);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x300U, fde->pc_start);
  EXPECT_EQ(0x500U, fde->pc_end);

  //   0x700 - 0x800  FDE 2
  fde = this->debug_frame_->GetFdeFromPc(0x790);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x700U, fde->pc_start);
  EXPECT_EQ(0x800U, fde->pc_end);

  //   0x800 - 0x900  FDE 4
  fde = this->debug_frame_->GetFdeFromPc(0x850);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x150U, fde->pc_start);
  EXPECT_EQ(0xb50U, fde->pc_end);

  //   0xa00 - 0xb00  FDE 3
  fde = this->debug_frame_->GetFdeFromPc(0xa35);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0xa00U, fde->pc_start);
  EXPECT_EQ(0xb00U, fde->pc_end);

  //   0xb00 - 0xb50  FDE 4
  fde = this->debug_frame_->GetFdeFromPc(0xb20);
  ASSERT_TRUE(fde != nullptr);
  EXPECT_EQ(0x150U, fde->pc_start);
  EXPECT_EQ(0xb50U, fde->pc_end);
}

REGISTER_TYPED_TEST_CASE_P(
    DwarfDebugFrameTest, GetFdes32, GetFdes32_after_GetFdeFromPc, GetFdes32_not_in_section,
    GetFdeFromPc32, GetFdeFromPc32_reverse, GetFdeFromPc32_not_in_section, GetFdes64,
    GetFdes64_after_GetFdeFromPc, GetFdes64_not_in_section, GetFdeFromPc64, GetFdeFromPc64_reverse,
    GetFdeFromPc64_not_in_section, GetCieFde32, GetCieFde64, GetCieFromOffset32_cie_cached,
    GetCieFromOffset64_cie_cached, GetCieFromOffset32_version1, GetCieFromOffset64_version1,
    GetCieFromOffset32_version3, GetCieFromOffset64_version3, GetCieFromOffset32_version4,
    GetCieFromOffset64_version4, GetCieFromOffset32_version5, GetCieFromOffset64_version5,
    GetCieFromOffset_version_invalid, GetCieFromOffset32_augment, GetCieFromOffset64_augment,
    GetFdeFromOffset32_augment, GetFdeFromOffset64_augment, GetFdeFromOffset32_lsda_address,
    GetFdeFromOffset64_lsda_address, GetFdeFromPc_interleaved);

typedef ::testing::Types<uint32_t, uint64_t> DwarfDebugFrameTestTypes;
INSTANTIATE_TYPED_TEST_CASE_P(, DwarfDebugFrameTest, DwarfDebugFrameTestTypes);

}  // namespace unwindstack
