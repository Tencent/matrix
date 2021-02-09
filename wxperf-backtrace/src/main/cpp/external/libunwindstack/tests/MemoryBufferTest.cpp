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

#include <vector>

#include <gtest/gtest.h>

#include <unwindstack/Memory.h>

#include "LogFake.h"

namespace unwindstack {

class MemoryBufferTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ResetLogs();
    memory_.reset(new MemoryBuffer);
  }
  std::unique_ptr<MemoryBuffer> memory_;
};

TEST_F(MemoryBufferTest, empty) {
  ASSERT_EQ(0U, memory_->Size());
  std::vector<uint8_t> buffer(1024);
  ASSERT_FALSE(memory_->ReadFully(0, buffer.data(), 1));
  ASSERT_EQ(nullptr, memory_->GetPtr(0));
  ASSERT_EQ(nullptr, memory_->GetPtr(1));
}

TEST_F(MemoryBufferTest, write_read) {
  memory_->Resize(256);
  ASSERT_EQ(256U, memory_->Size());
  ASSERT_TRUE(memory_->GetPtr(0) != nullptr);
  ASSERT_TRUE(memory_->GetPtr(1) != nullptr);
  ASSERT_TRUE(memory_->GetPtr(255) != nullptr);
  ASSERT_TRUE(memory_->GetPtr(256) == nullptr);

  uint8_t* data = memory_->GetPtr(0);
  for (size_t i = 0; i < memory_->Size(); i++) {
    data[i] = i;
  }

  std::vector<uint8_t> buffer(memory_->Size());
  ASSERT_TRUE(memory_->ReadFully(0, buffer.data(), buffer.size()));
  for (size_t i = 0; i < buffer.size(); i++) {
    ASSERT_EQ(i, buffer[i]) << "Failed at byte " << i;
  }
}

TEST_F(MemoryBufferTest, read_failures) {
  memory_->Resize(100);
  std::vector<uint8_t> buffer(200);
  ASSERT_FALSE(memory_->ReadFully(0, buffer.data(), 101));
  ASSERT_FALSE(memory_->ReadFully(100, buffer.data(), 1));
  ASSERT_FALSE(memory_->ReadFully(101, buffer.data(), 2));
  ASSERT_FALSE(memory_->ReadFully(99, buffer.data(), 2));
  ASSERT_TRUE(memory_->ReadFully(99, buffer.data(), 1));
}

TEST_F(MemoryBufferTest, read_failure_overflow) {
  memory_->Resize(100);
  std::vector<uint8_t> buffer(200);

  ASSERT_FALSE(memory_->ReadFully(UINT64_MAX - 100, buffer.data(), 200));
}

TEST_F(MemoryBufferTest, Read) {
  memory_->Resize(256);
  ASSERT_EQ(256U, memory_->Size());
  ASSERT_TRUE(memory_->GetPtr(0) != nullptr);
  ASSERT_TRUE(memory_->GetPtr(1) != nullptr);
  ASSERT_TRUE(memory_->GetPtr(255) != nullptr);
  ASSERT_TRUE(memory_->GetPtr(256) == nullptr);

  uint8_t* data = memory_->GetPtr(0);
  for (size_t i = 0; i < memory_->Size(); i++) {
    data[i] = i;
  }

  std::vector<uint8_t> buffer(memory_->Size());
  ASSERT_EQ(128U, memory_->Read(128, buffer.data(), buffer.size()));
  for (size_t i = 0; i < 128; i++) {
    ASSERT_EQ(128 + i, buffer[i]) << "Failed at byte " << i;
  }
}

}  // namespace unwindstack
