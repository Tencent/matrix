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

#include <deque>
#include <vector>

#include <gtest/gtest.h>

#include <unwindstack/Log.h>

#include "ArmExidx.h"

#include "LogFake.h"
#include "MemoryFake.h"

namespace unwindstack {

class ArmExidxExtractTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ResetLogs();
    elf_memory_.Clear();
    exidx_ = new ArmExidx(nullptr, &elf_memory_, nullptr);
    data_ = exidx_->data();
    data_->clear();
  }

  void TearDown() override {
    delete exidx_;
  }

  ArmExidx* exidx_ = nullptr;
  std::deque<uint8_t>* data_;
  MemoryFake elf_memory_;
};

TEST_F(ArmExidxExtractTest, bad_alignment) {
  ASSERT_FALSE(exidx_->ExtractEntryData(0x1001));
  ASSERT_EQ(ARM_STATUS_INVALID_ALIGNMENT, exidx_->status());
  ASSERT_TRUE(data_->empty());
}

TEST_F(ArmExidxExtractTest, cant_unwind) {
  elf_memory_.SetData32(0x1000, 0x7fff2340);
  elf_memory_.SetData32(0x1004, 1);
  ASSERT_FALSE(exidx_->ExtractEntryData(0x1000));
  ASSERT_EQ(ARM_STATUS_NO_UNWIND, exidx_->status());
  ASSERT_TRUE(data_->empty());
}

TEST_F(ArmExidxExtractTest, compact) {
  elf_memory_.SetData32(0x4000, 0x7ffa3000);
  elf_memory_.SetData32(0x4004, 0x80a8b0b0);
  ASSERT_TRUE(exidx_->ExtractEntryData(0x4000));
  ASSERT_EQ(3U, data_->size());
  ASSERT_EQ(0xa8, data_->at(0));
  ASSERT_EQ(0xb0, data_->at(1));
  ASSERT_EQ(0xb0, data_->at(2));

  // Missing finish gets added.
  elf_memory_.Clear();
  elf_memory_.SetData32(0x534, 0x7ffa3000);
  elf_memory_.SetData32(0x538, 0x80a1a2a3);
  ASSERT_TRUE(exidx_->ExtractEntryData(0x534));
  ASSERT_EQ(4U, data_->size());
  ASSERT_EQ(0xa1, data_->at(0));
  ASSERT_EQ(0xa2, data_->at(1));
  ASSERT_EQ(0xa3, data_->at(2));
  ASSERT_EQ(0xb0, data_->at(3));
}

TEST_F(ArmExidxExtractTest, compact_non_zero_personality) {
  elf_memory_.SetData32(0x4000, 0x7ffa3000);

  uint32_t compact_value = 0x80a8b0b0;
  for (size_t i = 1; i < 16; i++) {
    elf_memory_.SetData32(0x4004, compact_value | (i << 24));
    ASSERT_FALSE(exidx_->ExtractEntryData(0x4000));
    ASSERT_EQ(ARM_STATUS_INVALID_PERSONALITY, exidx_->status());
  }
}

TEST_F(ArmExidxExtractTest, second_read_compact_personality_1_2) {
  elf_memory_.SetData32(0x5000, 0x1234);
  elf_memory_.SetData32(0x5004, 0x00001230);
  elf_memory_.SetData32(0x6234, 0x8100f3b0);
  ASSERT_TRUE(exidx_->ExtractEntryData(0x5000));
  ASSERT_EQ(2U, data_->size());
  ASSERT_EQ(0xf3, data_->at(0));
  ASSERT_EQ(0xb0, data_->at(1));

  elf_memory_.Clear();
  elf_memory_.SetData32(0x5000, 0x1234);
  elf_memory_.SetData32(0x5004, 0x00001230);
  elf_memory_.SetData32(0x6234, 0x8200f3f4);
  ASSERT_TRUE(exidx_->ExtractEntryData(0x5000));
  ASSERT_EQ(3U, data_->size());
  ASSERT_EQ(0xf3, data_->at(0));
  ASSERT_EQ(0xf4, data_->at(1));
  ASSERT_EQ(0xb0, data_->at(2));

  elf_memory_.Clear();
  elf_memory_.SetData32(0x5000, 0x1234);
  elf_memory_.SetData32(0x5004, 0x00001230);
  elf_memory_.SetData32(0x6234, 0x8201f3f4);
  elf_memory_.SetData32(0x6238, 0x102030b0);
  ASSERT_TRUE(exidx_->ExtractEntryData(0x5000));
  ASSERT_EQ(6U, data_->size());
  ASSERT_EQ(0xf3, data_->at(0));
  ASSERT_EQ(0xf4, data_->at(1));
  ASSERT_EQ(0x10, data_->at(2));
  ASSERT_EQ(0x20, data_->at(3));
  ASSERT_EQ(0x30, data_->at(4));
  ASSERT_EQ(0xb0, data_->at(5));

  elf_memory_.Clear();
  elf_memory_.SetData32(0x5000, 0x1234);
  elf_memory_.SetData32(0x5004, 0x00001230);
  elf_memory_.SetData32(0x6234, 0x8103f3f4);
  elf_memory_.SetData32(0x6238, 0x10203040);
  elf_memory_.SetData32(0x623c, 0x50607080);
  elf_memory_.SetData32(0x6240, 0x90a0c0d0);
  ASSERT_TRUE(exidx_->ExtractEntryData(0x5000));
  ASSERT_EQ(15U, data_->size());
  ASSERT_EQ(0xf3, data_->at(0));
  ASSERT_EQ(0xf4, data_->at(1));
  ASSERT_EQ(0x10, data_->at(2));
  ASSERT_EQ(0x20, data_->at(3));
  ASSERT_EQ(0x30, data_->at(4));
  ASSERT_EQ(0x40, data_->at(5));
  ASSERT_EQ(0x50, data_->at(6));
  ASSERT_EQ(0x60, data_->at(7));
  ASSERT_EQ(0x70, data_->at(8));
  ASSERT_EQ(0x80, data_->at(9));
  ASSERT_EQ(0x90, data_->at(10));
  ASSERT_EQ(0xa0, data_->at(11));
  ASSERT_EQ(0xc0, data_->at(12));
  ASSERT_EQ(0xd0, data_->at(13));
  ASSERT_EQ(0xb0, data_->at(14));
}

TEST_F(ArmExidxExtractTest, second_read_compact_personality_illegal) {
  elf_memory_.SetData32(0x5000, 0x7ffa1e48);
  elf_memory_.SetData32(0x5004, 0x1230);
  elf_memory_.SetData32(0x6234, 0x832132b0);
  ASSERT_FALSE(exidx_->ExtractEntryData(0x5000));
  ASSERT_EQ(ARM_STATUS_INVALID_PERSONALITY, exidx_->status());

  elf_memory_.Clear();
  elf_memory_.SetData32(0x5000, 0x7ffa1e48);
  elf_memory_.SetData32(0x5004, 0x1230);
  elf_memory_.SetData32(0x6234, 0x842132b0);
  ASSERT_FALSE(exidx_->ExtractEntryData(0x5000));
  ASSERT_EQ(ARM_STATUS_INVALID_PERSONALITY, exidx_->status());
}

TEST_F(ArmExidxExtractTest, second_read_offset_is_negative) {
  elf_memory_.SetData32(0x5000, 0x7ffa1e48);
  elf_memory_.SetData32(0x5004, 0x7fffb1e0);
  elf_memory_.SetData32(0x1e4, 0x842132b0);
  ASSERT_FALSE(exidx_->ExtractEntryData(0x5000));
  ASSERT_EQ(ARM_STATUS_INVALID_PERSONALITY, exidx_->status());
}

TEST_F(ArmExidxExtractTest, second_read_not_compact) {
  elf_memory_.SetData32(0x5000, 0x1234);
  elf_memory_.SetData32(0x5004, 0x00001230);
  elf_memory_.SetData32(0x6234, 0x1);
  elf_memory_.SetData32(0x6238, 0x001122b0);
  ASSERT_TRUE(exidx_->ExtractEntryData(0x5000));
  ASSERT_EQ(3U, data_->size());
  ASSERT_EQ(0x11, data_->at(0));
  ASSERT_EQ(0x22, data_->at(1));
  ASSERT_EQ(0xb0, data_->at(2));

  elf_memory_.Clear();
  elf_memory_.SetData32(0x5000, 0x1234);
  elf_memory_.SetData32(0x5004, 0x00001230);
  elf_memory_.SetData32(0x6234, 0x2);
  elf_memory_.SetData32(0x6238, 0x00112233);
  ASSERT_TRUE(exidx_->ExtractEntryData(0x5000));
  ASSERT_EQ(4U, data_->size());
  ASSERT_EQ(0x11, data_->at(0));
  ASSERT_EQ(0x22, data_->at(1));
  ASSERT_EQ(0x33, data_->at(2));
  ASSERT_EQ(0xb0, data_->at(3));

  elf_memory_.Clear();
  elf_memory_.SetData32(0x5000, 0x1234);
  elf_memory_.SetData32(0x5004, 0x00001230);
  elf_memory_.SetData32(0x6234, 0x3);
  elf_memory_.SetData32(0x6238, 0x01112233);
  elf_memory_.SetData32(0x623c, 0x445566b0);
  ASSERT_TRUE(exidx_->ExtractEntryData(0x5000));
  ASSERT_EQ(7U, data_->size());
  ASSERT_EQ(0x11, data_->at(0));
  ASSERT_EQ(0x22, data_->at(1));
  ASSERT_EQ(0x33, data_->at(2));
  ASSERT_EQ(0x44, data_->at(3));
  ASSERT_EQ(0x55, data_->at(4));
  ASSERT_EQ(0x66, data_->at(5));
  ASSERT_EQ(0xb0, data_->at(6));

  elf_memory_.Clear();
  elf_memory_.SetData32(0x5000, 0x1234);
  elf_memory_.SetData32(0x5004, 0x00001230);
  elf_memory_.SetData32(0x6234, 0x3);
  elf_memory_.SetData32(0x6238, 0x05112233);
  elf_memory_.SetData32(0x623c, 0x01020304);
  elf_memory_.SetData32(0x6240, 0x05060708);
  elf_memory_.SetData32(0x6244, 0x090a0b0c);
  elf_memory_.SetData32(0x6248, 0x0d0e0f10);
  elf_memory_.SetData32(0x624c, 0x11121314);
  ASSERT_TRUE(exidx_->ExtractEntryData(0x5000));
  ASSERT_EQ(24U, data_->size());
  ASSERT_EQ(0x11, data_->at(0));
  ASSERT_EQ(0x22, data_->at(1));
  ASSERT_EQ(0x33, data_->at(2));
  ASSERT_EQ(0x01, data_->at(3));
  ASSERT_EQ(0x02, data_->at(4));
  ASSERT_EQ(0x03, data_->at(5));
  ASSERT_EQ(0x04, data_->at(6));
  ASSERT_EQ(0x05, data_->at(7));
  ASSERT_EQ(0x06, data_->at(8));
  ASSERT_EQ(0x07, data_->at(9));
  ASSERT_EQ(0x08, data_->at(10));
  ASSERT_EQ(0x09, data_->at(11));
  ASSERT_EQ(0x0a, data_->at(12));
  ASSERT_EQ(0x0b, data_->at(13));
  ASSERT_EQ(0x0c, data_->at(14));
  ASSERT_EQ(0x0d, data_->at(15));
  ASSERT_EQ(0x0e, data_->at(16));
  ASSERT_EQ(0x0f, data_->at(17));
  ASSERT_EQ(0x10, data_->at(18));
  ASSERT_EQ(0x11, data_->at(19));
  ASSERT_EQ(0x12, data_->at(20));
  ASSERT_EQ(0x13, data_->at(21));
  ASSERT_EQ(0x14, data_->at(22));
  ASSERT_EQ(0xb0, data_->at(23));
}

TEST_F(ArmExidxExtractTest, read_failures) {
  ASSERT_FALSE(exidx_->ExtractEntryData(0x5000));
  ASSERT_EQ(ARM_STATUS_READ_FAILED, exidx_->status());
  EXPECT_EQ(0x5004U, exidx_->status_address());

  elf_memory_.SetData32(0x5000, 0x100);
  ASSERT_FALSE(exidx_->ExtractEntryData(0x5000));
  ASSERT_EQ(ARM_STATUS_READ_FAILED, exidx_->status());
  EXPECT_EQ(0x5004U, exidx_->status_address());

  elf_memory_.SetData32(0x5004, 0x100);
  ASSERT_FALSE(exidx_->ExtractEntryData(0x5000));
  ASSERT_EQ(ARM_STATUS_READ_FAILED, exidx_->status());
  EXPECT_EQ(0x5104U, exidx_->status_address());

  elf_memory_.SetData32(0x5104, 0x1);
  ASSERT_FALSE(exidx_->ExtractEntryData(0x5000));
  ASSERT_EQ(ARM_STATUS_READ_FAILED, exidx_->status());
  EXPECT_EQ(0x5108U, exidx_->status_address());

  elf_memory_.SetData32(0x5108, 0x01010203);
  ASSERT_FALSE(exidx_->ExtractEntryData(0x5000));
  ASSERT_EQ(ARM_STATUS_READ_FAILED, exidx_->status());
  EXPECT_EQ(0x510cU, exidx_->status_address());
}

TEST_F(ArmExidxExtractTest, malformed) {
  elf_memory_.SetData32(0x5000, 0x100);
  elf_memory_.SetData32(0x5004, 0x100);
  elf_memory_.SetData32(0x5104, 0x1);
  elf_memory_.SetData32(0x5108, 0x06010203);
  ASSERT_FALSE(exidx_->ExtractEntryData(0x5000));
  ASSERT_EQ(ARM_STATUS_MALFORMED, exidx_->status());

  elf_memory_.Clear();
  elf_memory_.SetData32(0x5000, 0x100);
  elf_memory_.SetData32(0x5004, 0x100);
  elf_memory_.SetData32(0x5104, 0x1);
  elf_memory_.SetData32(0x5108, 0x81060203);
  ASSERT_FALSE(exidx_->ExtractEntryData(0x5000));
  ASSERT_EQ(ARM_STATUS_MALFORMED, exidx_->status());
}

TEST_F(ArmExidxExtractTest, cant_unwind_log) {
  elf_memory_.SetData32(0x1000, 0x7fff2340);
  elf_memory_.SetData32(0x1004, 1);

  exidx_->set_log(ARM_LOG_FULL);
  exidx_->set_log_indent(0);
  exidx_->set_log_skip_execution(false);

  ASSERT_FALSE(exidx_->ExtractEntryData(0x1000));
  ASSERT_EQ(ARM_STATUS_NO_UNWIND, exidx_->status());

  ASSERT_EQ("4 unwind Raw Data: 0x00 0x00 0x00 0x01\n"
            "4 unwind [cantunwind]\n", GetFakeLogPrint());
}

TEST_F(ArmExidxExtractTest, raw_data_compact) {
  elf_memory_.SetData32(0x4000, 0x7ffa3000);
  elf_memory_.SetData32(0x4004, 0x80a8b0b0);

  exidx_->set_log(ARM_LOG_FULL);
  exidx_->set_log_indent(0);
  exidx_->set_log_skip_execution(false);

  ASSERT_TRUE(exidx_->ExtractEntryData(0x4000));
  ASSERT_EQ("4 unwind Raw Data: 0xa8 0xb0 0xb0\n", GetFakeLogPrint());
}

TEST_F(ArmExidxExtractTest, raw_data_non_compact) {
  elf_memory_.SetData32(0x5000, 0x1234);
  elf_memory_.SetData32(0x5004, 0x00001230);
  elf_memory_.SetData32(0x6234, 0x2);
  elf_memory_.SetData32(0x6238, 0x00112233);

  exidx_->set_log(ARM_LOG_FULL);
  exidx_->set_log_indent(0);
  exidx_->set_log_skip_execution(false);

  ASSERT_TRUE(exidx_->ExtractEntryData(0x5000));
  ASSERT_EQ("4 unwind Raw Data: 0x11 0x22 0x33 0xb0\n", GetFakeLogPrint());
}

}  // namespace unwindstack
