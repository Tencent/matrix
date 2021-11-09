/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <unwindstack/Memory.h>

#include "MemoryFake.h"

namespace unwindstack {

class MemoryRangesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    MemoryFake* memory = new MemoryFake;
    process_memory_.reset(memory);
    memory->SetMemoryBlock(1000, 5000, 0x15);
    memory->SetMemoryBlock(6000, 12000, 0x26);
    memory->SetMemoryBlock(14000, 20000, 0x37);
    memory->SetMemoryBlock(20000, 22000, 0x48);

    ranges_.reset(new MemoryRanges);
    ranges_->Insert(new MemoryRange(process_memory_, 15000, 100, 4000));
    ranges_->Insert(new MemoryRange(process_memory_, 10000, 2000, 2000));
    ranges_->Insert(new MemoryRange(process_memory_, 3000, 1000, 0));
    ranges_->Insert(new MemoryRange(process_memory_, 19000, 1000, 6000));
    ranges_->Insert(new MemoryRange(process_memory_, 20000, 1000, 7000));
  }

  std::shared_ptr<Memory> process_memory_;
  std::unique_ptr<MemoryRanges> ranges_;
};

TEST_F(MemoryRangesTest, read) {
  std::vector<uint8_t> dst(2000);
  size_t bytes = ranges_->Read(0, dst.data(), dst.size());
  ASSERT_EQ(1000UL, bytes);
  for (size_t i = 0; i < bytes; i++) {
    ASSERT_EQ(0x15U, dst[i]) << "Failed at byte " << i;
  }

  bytes = ranges_->Read(2000, dst.data(), dst.size());
  ASSERT_EQ(2000UL, bytes);
  for (size_t i = 0; i < bytes; i++) {
    ASSERT_EQ(0x26U, dst[i]) << "Failed at byte " << i;
  }

  bytes = ranges_->Read(4000, dst.data(), dst.size());
  ASSERT_EQ(100UL, bytes);
  for (size_t i = 0; i < bytes; i++) {
    ASSERT_EQ(0x37U, dst[i]) << "Failed at byte " << i;
  }
}

TEST_F(MemoryRangesTest, read_fail) {
  std::vector<uint8_t> dst(4096);
  ASSERT_EQ(0UL, ranges_->Read(1000, dst.data(), dst.size()));
  ASSERT_EQ(0UL, ranges_->Read(5000, dst.data(), dst.size()));
  ASSERT_EQ(0UL, ranges_->Read(8000, dst.data(), dst.size()));
}

TEST_F(MemoryRangesTest, read_across_ranges) {
  // The MemoryRanges object does not support reading across a range,
  // so this will only read in the first range.
  std::vector<uint8_t> dst(4096);
  size_t bytes = ranges_->Read(6000, dst.data(), dst.size());
  ASSERT_EQ(1000UL, bytes);
  for (size_t i = 0; i < bytes; i++) {
    ASSERT_EQ(0x37U, dst[i]) << "Failed at byte " << i;
  }
}

}  // namespace unwindstack
