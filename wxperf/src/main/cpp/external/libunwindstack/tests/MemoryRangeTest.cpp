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
#include <string.h>

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <unwindstack/Memory.h>

#include "MemoryFake.h"

namespace unwindstack {

TEST(MemoryRangeTest, read) {
  std::vector<uint8_t> src(1024);
  memset(src.data(), 0x4c, 1024);
  MemoryFake* memory_fake = new MemoryFake;
  std::shared_ptr<Memory> process_memory(memory_fake);
  memory_fake->SetMemory(9001, src);

  MemoryRange range(process_memory, 9001, src.size(), 0);

  std::vector<uint8_t> dst(1024);
  ASSERT_TRUE(range.ReadFully(0, dst.data(), src.size()));
  for (size_t i = 0; i < 1024; i++) {
    ASSERT_EQ(0x4cU, dst[i]) << "Failed at byte " << i;
  }
}

TEST(MemoryRangeTest, read_near_limit) {
  std::vector<uint8_t> src(4096);
  memset(src.data(), 0x4c, 4096);
  MemoryFake* memory_fake = new MemoryFake;
  std::shared_ptr<Memory> process_memory(memory_fake);
  memory_fake->SetMemory(1000, src);

  MemoryRange range(process_memory, 1000, 1024, 0);

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

TEST(MemoryRangeTest, read_overflow) {
  std::vector<uint8_t> buffer(100);

  std::shared_ptr<Memory> process_memory(new MemoryFakeAlwaysReadZero);
  std::unique_ptr<MemoryRange> overflow(new MemoryRange(process_memory, 100, 200, 0));
  ASSERT_FALSE(overflow->ReadFully(UINT64_MAX - 10, buffer.data(), 100));
}

TEST(MemoryRangeTest, Read) {
  std::vector<uint8_t> src(4096);
  memset(src.data(), 0x4c, 4096);
  MemoryFake* memory_fake = new MemoryFake;
  std::shared_ptr<Memory> process_memory(memory_fake);
  memory_fake->SetMemory(1000, src);

  MemoryRange range(process_memory, 1000, 1024, 0);
  std::vector<uint8_t> dst(1024);
  ASSERT_EQ(4U, range.Read(1020, dst.data(), 1024));
  for (size_t i = 0; i < 4; i++) {
    ASSERT_EQ(0x4cU, dst[i]) << "Failed at byte " << i;
  }
}

}  // namespace unwindstack
