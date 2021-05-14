/*
 * Copyright (C) 2017 The Android Open Source Project
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
#include <vector>

#include <gtest/gtest.h>

#include <unwindstack/Memory.h>

#include "MemoryFake.h"

namespace unwindstack {

class MemoryRangeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    process_memory_.reset();
    memory_fake_ = new MemoryFake;
    process_memory_.reset(memory_fake_);
  }

  std::shared_ptr<Memory> process_memory_;
  MemoryFake* memory_fake_ = nullptr;
};

TEST_F(MemoryRangeTest, read_fully) {
  memory_fake_->SetMemoryBlock(9000, 2048, 0x4c);

  MemoryRange range(process_memory_, 9001, 1024, 0);

  std::vector<uint8_t> dst(1024);
  ASSERT_TRUE(range.ReadFully(0, dst.data(), dst.size()));
  for (size_t i = 0; i < dst.size(); i++) {
    ASSERT_EQ(0x4cU, dst[i]) << "Failed at byte " << i;
  }
}

TEST_F(MemoryRangeTest, read_fully_near_limit) {
  memory_fake_->SetMemoryBlock(0, 8192, 0x4c);

  MemoryRange range(process_memory_, 1000, 1024, 0);

  std::vector<uint8_t> dst(1024);
  ASSERT_TRUE(range.ReadFully(1020, dst.data(), 4));
  for (size_t i = 0; i < 4; i++) {
    ASSERT_EQ(0x4cU, dst[i]) << "Failed at byte " << i;
  }

  // Verify that reads outside of the range will fail.
  ASSERT_FALSE(range.ReadFully(1020, dst.data(), 5));
  ASSERT_FALSE(range.ReadFully(1024, dst.data(), 1));
  ASSERT_FALSE(range.ReadFully(1024, dst.data(), 1024));

  // Verify that reading up to the end works.
  ASSERT_TRUE(range.ReadFully(1020, dst.data(), 4));
}

TEST_F(MemoryRangeTest, read_fully_overflow) {
  std::vector<uint8_t> buffer(100);

  std::shared_ptr<Memory> process_memory(new MemoryFakeAlwaysReadZero);
  std::unique_ptr<MemoryRange> overflow(new MemoryRange(process_memory, 100, 200, 0));
  ASSERT_FALSE(overflow->ReadFully(UINT64_MAX - 10, buffer.data(), 100));
}

TEST_F(MemoryRangeTest, read) {
  memory_fake_->SetMemoryBlock(0, 4096, 0x4c);

  MemoryRange range(process_memory_, 1000, 1024, 0);

  std::vector<uint8_t> dst(1024);
  ASSERT_EQ(4U, range.Read(1020, dst.data(), dst.size()));
  for (size_t i = 0; i < 4; i++) {
    ASSERT_EQ(0x4cU, dst[i]) << "Failed at byte " << i;
  }
}

TEST_F(MemoryRangeTest, read_non_zero_offset) {
  memory_fake_->SetMemoryBlock(1000, 1024, 0x12);

  MemoryRange range(process_memory_, 1000, 1024, 400);

  std::vector<uint8_t> dst(1024);
  ASSERT_EQ(1024U, range.Read(400, dst.data(), dst.size()));
  for (size_t i = 0; i < dst.size(); i++) {
    ASSERT_EQ(0x12U, dst[i]) << "Failed at byte " << i;
  }
}

}  // namespace unwindstack
