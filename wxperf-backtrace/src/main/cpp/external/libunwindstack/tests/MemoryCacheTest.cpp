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

class MemoryCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    memory_ = new MemoryFake;
    memory_cache_.reset(new MemoryCache(memory_));

    memory_->SetMemoryBlock(0x8000, 4096, 0xab);
    memory_->SetMemoryBlock(0x9000, 4096, 0xde);
    memory_->SetMemoryBlock(0xa000, 3000, 0x50);
  }

  MemoryFake* memory_;
  std::unique_ptr<MemoryCache> memory_cache_;

  constexpr static size_t kMaxCachedSize = 64;
};

TEST_F(MemoryCacheTest, cached_read) {
  for (size_t i = 1; i <= kMaxCachedSize; i++) {
    std::vector<uint8_t> buffer(i);
    ASSERT_TRUE(memory_cache_->ReadFully(0x8000 + i, buffer.data(), i))
        << "Read failed at size " << i;
    ASSERT_EQ(std::vector<uint8_t>(i, 0xab), buffer) << "Failed at size " << i;
  }

  // Verify the cached data is used.
  memory_->SetMemoryBlock(0x8000, 4096, 0xff);
  for (size_t i = 1; i <= kMaxCachedSize; i++) {
    std::vector<uint8_t> buffer(i);
    ASSERT_TRUE(memory_cache_->ReadFully(0x8000 + i, buffer.data(), i))
        << "Read failed at size " << i;
    ASSERT_EQ(std::vector<uint8_t>(i, 0xab), buffer) << "Failed at size " << i;
  }
}

TEST_F(MemoryCacheTest, no_cached_read_after_clear) {
  for (size_t i = 1; i <= kMaxCachedSize; i++) {
    std::vector<uint8_t> buffer(i);
    ASSERT_TRUE(memory_cache_->ReadFully(0x8000 + i, buffer.data(), i))
        << "Read failed at size " << i;
    ASSERT_EQ(std::vector<uint8_t>(i, 0xab), buffer) << "Failed at size " << i;
  }

  // Verify the cached data is not used after a reset.
  memory_cache_->Clear();
  memory_->SetMemoryBlock(0x8000, 4096, 0xff);
  for (size_t i = 1; i <= kMaxCachedSize; i++) {
    std::vector<uint8_t> buffer(i);
    ASSERT_TRUE(memory_cache_->ReadFully(0x8000 + i, buffer.data(), i))
        << "Read failed at size " << i;
    ASSERT_EQ(std::vector<uint8_t>(i, 0xff), buffer) << "Failed at size " << i;
  }
}

TEST_F(MemoryCacheTest, cached_read_across_caches) {
  std::vector<uint8_t> expect(16, 0xab);
  expect.resize(32, 0xde);

  std::vector<uint8_t> buffer(32);
  ASSERT_TRUE(memory_cache_->ReadFully(0x8ff0, buffer.data(), 32));
  ASSERT_EQ(expect, buffer);

  // Verify the cached data is used.
  memory_->SetMemoryBlock(0x8000, 4096, 0xff);
  memory_->SetMemoryBlock(0x9000, 4096, 0xff);
  ASSERT_TRUE(memory_cache_->ReadFully(0x8ff0, buffer.data(), 32));
  ASSERT_EQ(expect, buffer);
}

TEST_F(MemoryCacheTest, no_cache_read) {
  for (size_t i = kMaxCachedSize + 1; i < 2 * kMaxCachedSize; i++) {
    std::vector<uint8_t> buffer(i);
    ASSERT_TRUE(memory_cache_->ReadFully(0x8000 + i, buffer.data(), i))
        << "Read failed at size " << i;
    ASSERT_EQ(std::vector<uint8_t>(i, 0xab), buffer) << "Failed at size " << i;
  }

  // Verify the cached data is not used.
  memory_->SetMemoryBlock(0x8000, 4096, 0xff);
  for (size_t i = kMaxCachedSize + 1; i < 2 * kMaxCachedSize; i++) {
    std::vector<uint8_t> buffer(i);
    ASSERT_TRUE(memory_cache_->ReadFully(0x8000 + i, buffer.data(), i))
        << "Read failed at size " << i;
    ASSERT_EQ(std::vector<uint8_t>(i, 0xff), buffer) << "Failed at size " << i;
  }
}

TEST_F(MemoryCacheTest, read_for_cache_fail) {
  std::vector<uint8_t> buffer(kMaxCachedSize);
  ASSERT_TRUE(memory_cache_->ReadFully(0xa010, buffer.data(), kMaxCachedSize));
  ASSERT_EQ(std::vector<uint8_t>(kMaxCachedSize, 0x50), buffer);

  // Verify the cached data is not used.
  memory_->SetMemoryBlock(0xa000, 3000, 0xff);
  ASSERT_TRUE(memory_cache_->ReadFully(0xa010, buffer.data(), kMaxCachedSize));
  ASSERT_EQ(std::vector<uint8_t>(kMaxCachedSize, 0xff), buffer);
}

TEST_F(MemoryCacheTest, read_for_cache_fail_cross) {
  std::vector<uint8_t> expect(16, 0xde);
  expect.resize(32, 0x50);

  std::vector<uint8_t> buffer(32);
  ASSERT_TRUE(memory_cache_->ReadFully(0x9ff0, buffer.data(), 32));
  ASSERT_EQ(expect, buffer);

  // Verify the cached data is not used for the second half but for the first.
  memory_->SetMemoryBlock(0xa000, 3000, 0xff);
  ASSERT_TRUE(memory_cache_->ReadFully(0x9ff0, buffer.data(), 32));
  expect.resize(16);
  expect.resize(32, 0xff);
  ASSERT_EQ(expect, buffer);
}

}  // namespace unwindstack
