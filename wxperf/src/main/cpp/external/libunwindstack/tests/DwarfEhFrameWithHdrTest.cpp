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

#include "DwarfEhFrameWithHdr.h"
#include "DwarfEncoding.h"

#include "LogFake.h"
#include "MemoryFake.h"

namespace unwindstack {

template <typename TypeParam>
class MockDwarfEhFrameWithHdr : public DwarfEhFrameWithHdr<TypeParam> {
 public:
  MockDwarfEhFrameWithHdr(Memory* memory) : DwarfEhFrameWithHdr<TypeParam>(memory) {}
  ~MockDwarfEhFrameWithHdr() = default;

  void TestSetTableEncoding(uint8_t encoding) { this->table_encoding_ = encoding; }
  void TestSetEntriesOffset(uint64_t offset) { this->entries_offset_ = offset; }
  void TestSetEntriesEnd(uint64_t end) { this->entries_end_ = end; }
  void TestSetEntriesDataOffset(uint64_t offset) { this->entries_data_offset_ = offset; }
  void TestSetCurEntriesOffset(uint64_t offset) { this->cur_entries_offset_ = offset; }
  void TestSetTableEntrySize(size_t size) { this->table_entry_size_ = size; }

  void TestSetFdeCount(uint64_t count) { this->fde_count_ = count; }
  void TestSetFdeInfo(uint64_t index, const typename DwarfEhFrameWithHdr<TypeParam>::FdeInfo& info) {
    this->fde_info_[index] = info;
  }

  uint8_t TestGetVersion() { return this->version_; }
  uint8_t TestGetPtrEncoding() { return this->ptr_encoding_; }
  uint64_t TestGetPtrOffset() { return this->ptr_offset_; }
  uint8_t TestGetTableEncoding() { return this->table_encoding_; }
  uint64_t TestGetTableEntrySize() { return this->table_entry_size_; }
  uint64_t TestGetFdeCount() { return this->fde_count_; }
  uint64_t TestGetEntriesOffset() { return this->entries_offset_; }
  uint64_t TestGetEntriesEnd() { return this->entries_end_; }
  uint64_t TestGetEntriesDataOffset() { return this->entries_data_offset_; }
  uint64_t TestGetCurEntriesOffset() { return this->cur_entries_offset_; }
};

template <typename TypeParam>
class DwarfEhFrameWithHdrTest : public ::testing::Test {
 protected:
  void SetUp() override {
    memory_.Clear();
    eh_frame_ = new MockDwarfEhFrameWithHdr<TypeParam>(&memory_);
    ResetLogs();
  }

  void TearDown() override { delete eh_frame_; }

  MemoryFake memory_;
  MockDwarfEhFrameWithHdr<TypeParam>* eh_frame_ = nullptr;
};
TYPED_TEST_CASE_P(DwarfEhFrameWithHdrTest);

// NOTE: All test class variables need to be referenced as this->.

TYPED_TEST_P(DwarfEhFrameWithHdrTest, Init) {
  this->memory_.SetMemory(
      0x1000, std::vector<uint8_t>{0x1, DW_EH_PE_udata2, DW_EH_PE_udata4, DW_EH_PE_sdata4});
  this->memory_.SetData16(0x1004, 0x500);
  this->memory_.SetData32(0x1006, 126);

  ASSERT_TRUE(this->eh_frame_->Init(0x1000, 0x100));
  EXPECT_EQ(1U, this->eh_frame_->TestGetVersion());
  EXPECT_EQ(DW_EH_PE_udata2, this->eh_frame_->TestGetPtrEncoding());
  EXPECT_EQ(DW_EH_PE_sdata4, this->eh_frame_->TestGetTableEncoding());
  EXPECT_EQ(4U, this->eh_frame_->TestGetTableEntrySize());
  EXPECT_EQ(126U, this->eh_frame_->TestGetFdeCount());
  EXPECT_EQ(0x500U, this->eh_frame_->TestGetPtrOffset());
  EXPECT_EQ(0x100aU, this->eh_frame_->TestGetEntriesOffset());
  EXPECT_EQ(0x1100U, this->eh_frame_->TestGetEntriesEnd());
  EXPECT_EQ(0x1000U, this->eh_frame_->TestGetEntriesDataOffset());
  EXPECT_EQ(0x100aU, this->eh_frame_->TestGetCurEntriesOffset());

  // Verify a zero fde count fails to init.
  this->memory_.SetData32(0x1006, 0);
  ASSERT_FALSE(this->eh_frame_->Init(0x1000, 0x100));
  ASSERT_EQ(DWARF_ERROR_NO_FDES, this->eh_frame_->LastErrorCode());

  // Verify an unexpected version will cause a fail.
  this->memory_.SetData32(0x1006, 126);
  this->memory_.SetData8(0x1000, 0);
  ASSERT_FALSE(this->eh_frame_->Init(0x1000, 0x100));
  ASSERT_EQ(DWARF_ERROR_UNSUPPORTED_VERSION, this->eh_frame_->LastErrorCode());
  this->memory_.SetData8(0x1000, 2);
  ASSERT_FALSE(this->eh_frame_->Init(0x1000, 0x100));
  ASSERT_EQ(DWARF_ERROR_UNSUPPORTED_VERSION, this->eh_frame_->LastErrorCode());
}

TYPED_TEST_P(DwarfEhFrameWithHdrTest, GetFdeInfoFromIndex_expect_cache_fail) {
  this->eh_frame_->TestSetTableEntrySize(0x10);
  this->eh_frame_->TestSetTableEncoding(DW_EH_PE_udata4);
  this->eh_frame_->TestSetEntriesOffset(0x1000);

  ASSERT_TRUE(this->eh_frame_->GetFdeInfoFromIndex(0) == nullptr);
  ASSERT_EQ(DWARF_ERROR_MEMORY_INVALID, this->eh_frame_->LastErrorCode());
  EXPECT_EQ(0x1000U, this->eh_frame_->LastErrorAddress());
  ASSERT_TRUE(this->eh_frame_->GetFdeInfoFromIndex(0) == nullptr);
  ASSERT_EQ(DWARF_ERROR_MEMORY_INVALID, this->eh_frame_->LastErrorCode());
  EXPECT_EQ(0x1000U, this->eh_frame_->LastErrorAddress());
}

TYPED_TEST_P(DwarfEhFrameWithHdrTest, GetFdeInfoFromIndex_read_pcrel) {
  this->eh_frame_->TestSetTableEncoding(DW_EH_PE_pcrel | DW_EH_PE_udata4);
  this->eh_frame_->TestSetEntriesOffset(0x1000);
  this->eh_frame_->TestSetEntriesDataOffset(0x3000);
  this->eh_frame_->TestSetTableEntrySize(0x10);

  this->memory_.SetData32(0x1040, 0x340);
  this->memory_.SetData32(0x1044, 0x500);

  auto info = this->eh_frame_->GetFdeInfoFromIndex(2);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(0x1380U, info->pc);
  EXPECT_EQ(0x1540U, info->offset);
}

TYPED_TEST_P(DwarfEhFrameWithHdrTest, GetFdeInfoFromIndex_read_datarel) {
  this->eh_frame_->TestSetTableEncoding(DW_EH_PE_datarel | DW_EH_PE_udata4);
  this->eh_frame_->TestSetEntriesOffset(0x1000);
  this->eh_frame_->TestSetEntriesDataOffset(0x3000);
  this->eh_frame_->TestSetTableEntrySize(0x10);

  this->memory_.SetData32(0x1040, 0x340);
  this->memory_.SetData32(0x1044, 0x500);

  auto info = this->eh_frame_->GetFdeInfoFromIndex(2);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(0x3340U, info->pc);
  EXPECT_EQ(0x3500U, info->offset);
}

TYPED_TEST_P(DwarfEhFrameWithHdrTest, GetFdeInfoFromIndex_cached) {
  this->eh_frame_->TestSetTableEncoding(DW_EH_PE_udata4);
  this->eh_frame_->TestSetEntriesOffset(0x1000);
  this->eh_frame_->TestSetTableEntrySize(0x10);

  this->memory_.SetData32(0x1040, 0x340);
  this->memory_.SetData32(0x1044, 0x500);

  auto info = this->eh_frame_->GetFdeInfoFromIndex(2);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(0x340U, info->pc);
  EXPECT_EQ(0x500U, info->offset);

  // Clear the memory so that this will fail if it doesn't read cached data.
  this->memory_.Clear();

  info = this->eh_frame_->GetFdeInfoFromIndex(2);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(0x340U, info->pc);
  EXPECT_EQ(0x500U, info->offset);
}

TYPED_TEST_P(DwarfEhFrameWithHdrTest, GetFdeOffsetBinary_verify) {
  this->eh_frame_->TestSetTableEntrySize(0x10);
  this->eh_frame_->TestSetFdeCount(10);

  typename DwarfEhFrameWithHdr<TypeParam>::FdeInfo info;
  for (size_t i = 0; i < 10; i++) {
    info.pc = 0x1000 * (i + 1);
    info.offset = 0x5000 + i * 0x20;
    this->eh_frame_->TestSetFdeInfo(i, info);
  }

  uint64_t fde_offset;
  EXPECT_FALSE(this->eh_frame_->GetFdeOffsetBinary(0x100, &fde_offset, 10));
  // Not an error, just not found.
  ASSERT_EQ(DWARF_ERROR_NONE, this->eh_frame_->LastErrorCode());
  // Even number of elements.
  for (size_t i = 0; i < 10; i++) {
    TypeParam pc = 0x1000 * (i + 1);
    EXPECT_TRUE(this->eh_frame_->GetFdeOffsetBinary(pc, &fde_offset, 10)) << "Failed at index " << i;
    EXPECT_EQ(0x5000 + i * 0x20, fde_offset) << "Failed at index " << i;
    EXPECT_TRUE(this->eh_frame_->GetFdeOffsetBinary(pc + 1, &fde_offset, 10))
        << "Failed at index " << i;
    EXPECT_EQ(0x5000 + i * 0x20, fde_offset) << "Failed at index " << i;
    EXPECT_TRUE(this->eh_frame_->GetFdeOffsetBinary(pc + 0xfff, &fde_offset, 10))
        << "Failed at index " << i;
    EXPECT_EQ(0x5000 + i * 0x20, fde_offset) << "Failed at index " << i;
  }
  // Odd number of elements.
  for (size_t i = 0; i < 9; i++) {
    TypeParam pc = 0x1000 * (i + 1);
    EXPECT_TRUE(this->eh_frame_->GetFdeOffsetBinary(pc, &fde_offset, 9)) << "Failed at index " << i;
    EXPECT_EQ(0x5000 + i * 0x20, fde_offset) << "Failed at index " << i;
    EXPECT_TRUE(this->eh_frame_->GetFdeOffsetBinary(pc + 1, &fde_offset, 9))
        << "Failed at index " << i;
    EXPECT_EQ(0x5000 + i * 0x20, fde_offset) << "Failed at index " << i;
    EXPECT_TRUE(this->eh_frame_->GetFdeOffsetBinary(pc + 0xfff, &fde_offset, 9))
        << "Failed at index " << i;
    EXPECT_EQ(0x5000 + i * 0x20, fde_offset) << "Failed at index " << i;
  }
}

TYPED_TEST_P(DwarfEhFrameWithHdrTest, GetFdeOffsetBinary_index_fail) {
  this->eh_frame_->TestSetTableEntrySize(0x10);
  this->eh_frame_->TestSetFdeCount(10);

  uint64_t fde_offset;
  EXPECT_FALSE(this->eh_frame_->GetFdeOffsetBinary(0x1000, &fde_offset, 10));
}

TYPED_TEST_P(DwarfEhFrameWithHdrTest, GetFdeOffsetSequential) {
  this->eh_frame_->TestSetFdeCount(10);
  this->eh_frame_->TestSetEntriesDataOffset(0x100);
  this->eh_frame_->TestSetEntriesEnd(0x2000);
  this->eh_frame_->TestSetTableEncoding(DW_EH_PE_udata4);

  this->memory_.SetData32(0x1040, 0x340);
  this->memory_.SetData32(0x1044, 0x500);

  this->memory_.SetData32(0x1048, 0x440);
  this->memory_.SetData32(0x104c, 0x600);

  // Verify that if entries is zero, that it fails.
  uint64_t fde_offset;
  ASSERT_FALSE(this->eh_frame_->GetFdeOffsetSequential(0x344, &fde_offset));
  this->eh_frame_->TestSetCurEntriesOffset(0x1040);

  ASSERT_TRUE(this->eh_frame_->GetFdeOffsetSequential(0x344, &fde_offset));
  EXPECT_EQ(0x500U, fde_offset);

  ASSERT_TRUE(this->eh_frame_->GetFdeOffsetSequential(0x444, &fde_offset));
  EXPECT_EQ(0x600U, fde_offset);

  // Expect that the data is cached so no more memory reads will occur.
  this->memory_.Clear();
  ASSERT_TRUE(this->eh_frame_->GetFdeOffsetSequential(0x444, &fde_offset));
  EXPECT_EQ(0x600U, fde_offset);
}

TYPED_TEST_P(DwarfEhFrameWithHdrTest, GetFdeOffsetSequential_last_element) {
  this->eh_frame_->TestSetFdeCount(2);
  this->eh_frame_->TestSetEntriesDataOffset(0x100);
  this->eh_frame_->TestSetEntriesEnd(0x2000);
  this->eh_frame_->TestSetTableEncoding(DW_EH_PE_udata4);
  this->eh_frame_->TestSetCurEntriesOffset(0x1040);

  this->memory_.SetData32(0x1040, 0x340);
  this->memory_.SetData32(0x1044, 0x500);

  this->memory_.SetData32(0x1048, 0x440);
  this->memory_.SetData32(0x104c, 0x600);

  uint64_t fde_offset;
  ASSERT_TRUE(this->eh_frame_->GetFdeOffsetSequential(0x540, &fde_offset));
  EXPECT_EQ(0x600U, fde_offset);
}

TYPED_TEST_P(DwarfEhFrameWithHdrTest, GetFdeOffsetSequential_end_check) {
  this->eh_frame_->TestSetFdeCount(2);
  this->eh_frame_->TestSetEntriesDataOffset(0x100);
  this->eh_frame_->TestSetEntriesEnd(0x1048);
  this->eh_frame_->TestSetTableEncoding(DW_EH_PE_udata4);

  this->memory_.SetData32(0x1040, 0x340);
  this->memory_.SetData32(0x1044, 0x500);

  this->memory_.SetData32(0x1048, 0x440);
  this->memory_.SetData32(0x104c, 0x600);

  uint64_t fde_offset;
  ASSERT_FALSE(this->eh_frame_->GetFdeOffsetSequential(0x540, &fde_offset));
  ASSERT_EQ(DWARF_ERROR_NONE, this->eh_frame_->LastErrorCode());
}

TYPED_TEST_P(DwarfEhFrameWithHdrTest, GetFdeOffsetFromPc_fail_fde_count) {
  this->eh_frame_->TestSetFdeCount(0);

  uint64_t fde_offset;
  ASSERT_FALSE(this->eh_frame_->GetFdeOffsetFromPc(0x100, &fde_offset));
  ASSERT_EQ(DWARF_ERROR_NONE, this->eh_frame_->LastErrorCode());
}

TYPED_TEST_P(DwarfEhFrameWithHdrTest, GetFdeOffsetFromPc_binary_search) {
  this->eh_frame_->TestSetTableEntrySize(16);
  this->eh_frame_->TestSetFdeCount(10);

  typename DwarfEhFrameWithHdr<TypeParam>::FdeInfo info;
  info.pc = 0x550;
  info.offset = 0x10500;
  this->eh_frame_->TestSetFdeInfo(5, info);
  info.pc = 0x750;
  info.offset = 0x10700;
  this->eh_frame_->TestSetFdeInfo(7, info);
  info.pc = 0x850;
  info.offset = 0x10800;
  this->eh_frame_->TestSetFdeInfo(8, info);

  uint64_t fde_offset;
  ASSERT_TRUE(this->eh_frame_->GetFdeOffsetFromPc(0x800, &fde_offset));
  EXPECT_EQ(0x10700U, fde_offset);
}

TYPED_TEST_P(DwarfEhFrameWithHdrTest, GetFdeOffsetFromPc_sequential_search) {
  this->eh_frame_->TestSetFdeCount(10);
  this->eh_frame_->TestSetTableEntrySize(0);

  typename DwarfEhFrameWithHdr<TypeParam>::FdeInfo info;
  info.pc = 0x50;
  info.offset = 0x10000;
  this->eh_frame_->TestSetFdeInfo(0, info);
  info.pc = 0x150;
  info.offset = 0x10100;
  this->eh_frame_->TestSetFdeInfo(1, info);
  info.pc = 0x250;
  info.offset = 0x10200;
  this->eh_frame_->TestSetFdeInfo(2, info);

  uint64_t fde_offset;
  ASSERT_TRUE(this->eh_frame_->GetFdeOffsetFromPc(0x200, &fde_offset));
  EXPECT_EQ(0x10100U, fde_offset);
}

TYPED_TEST_P(DwarfEhFrameWithHdrTest, GetCieFde32) {
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

TYPED_TEST_P(DwarfEhFrameWithHdrTest, GetCieFde64) {
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

TYPED_TEST_P(DwarfEhFrameWithHdrTest, GetFdeFromPc_fde_not_found) {
  this->eh_frame_->TestSetTableEntrySize(16);
  this->eh_frame_->TestSetFdeCount(1);

  typename DwarfEhFrameWithHdr<TypeParam>::FdeInfo info;
  info.pc = 0x550;
  info.offset = 0x10500;
  this->eh_frame_->TestSetFdeInfo(0, info);

  ASSERT_EQ(nullptr, this->eh_frame_->GetFdeFromPc(0x800));
}

REGISTER_TYPED_TEST_CASE_P(DwarfEhFrameWithHdrTest, Init, GetFdeInfoFromIndex_expect_cache_fail,
                           GetFdeInfoFromIndex_read_pcrel, GetFdeInfoFromIndex_read_datarel,
                           GetFdeInfoFromIndex_cached, GetFdeOffsetBinary_verify,
                           GetFdeOffsetBinary_index_fail, GetFdeOffsetSequential,
                           GetFdeOffsetSequential_last_element, GetFdeOffsetSequential_end_check,
                           GetFdeOffsetFromPc_fail_fde_count, GetFdeOffsetFromPc_binary_search,
                           GetFdeOffsetFromPc_sequential_search, GetCieFde32, GetCieFde64,
                           GetFdeFromPc_fde_not_found);

typedef ::testing::Types<uint32_t, uint64_t> DwarfEhFrameWithHdrTestTypes;
INSTANTIATE_TYPED_TEST_CASE_P(, DwarfEhFrameWithHdrTest, DwarfEhFrameWithHdrTestTypes);

}  // namespace unwindstack
