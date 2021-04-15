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

#include <vector>

#include <gtest/gtest.h>

#include <android-base/file.h>
#include <unwindstack/Memory.h>

namespace unwindstack {

class MemoryOfflineTest : public ::testing::Test {
 protected:
  void SetUp() override {
    for (size_t i = 0; i < 1024; ++i) {
      data.push_back(i & 0xff);
    }

    ASSERT_TRUE(android::base::WriteFully(temp_file.fd, &offset, sizeof(offset)));
    ASSERT_TRUE(android::base::WriteFully(temp_file.fd, data.data(), data.size()));

    memory = std::make_unique<MemoryOffline>();
    ASSERT_TRUE(memory != nullptr);

    ASSERT_TRUE(memory->Init(temp_file.path, 0));
  }

  TemporaryFile temp_file;
  uint64_t offset = 4096;
  std::vector<char> data;
  std::unique_ptr<MemoryOffline> memory;
};

TEST_F(MemoryOfflineTest, read_boundaries) {
  char buf = '\0';
  ASSERT_EQ(0U, memory->Read(offset - 1, &buf, 1));
  ASSERT_EQ(0U, memory->Read(offset + data.size(), &buf, 1));
  ASSERT_EQ(1U, memory->Read(offset, &buf, 1));
  ASSERT_EQ(buf, data.front());
  ASSERT_EQ(1U, memory->Read(offset + data.size() - 1, &buf, 1));
  ASSERT_EQ(buf, data.back());
}

TEST_F(MemoryOfflineTest, read_values) {
  std::vector<char> buf;
  buf.resize(2 * data.size());
  ASSERT_EQ(data.size(), memory->Read(offset, buf.data(), buf.size()));
  buf.resize(data.size());
  ASSERT_EQ(buf, data);
}

}  // namespace unwindstack
