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

#include <string>
#include <vector>

#include <android-base/test_utils.h>
#include <android-base/file.h>
#include <gtest/gtest.h>

#include <unwindstack/Memory.h>

namespace unwindstack {

class MemoryFileTest : public ::testing::Test {
 protected:
  void SetUp() override {
    tf_ = new TemporaryFile;
  }

  void TearDown() override {
    delete tf_;
  }

  void WriteTestData() {
    ASSERT_TRUE(android::base::WriteStringToFd("0123456789abcdefghijklmnopqrstuvxyz", tf_->fd));
  }

  MemoryFileAtOffset memory_;

  TemporaryFile* tf_ = nullptr;
};

TEST_F(MemoryFileTest, init_offset_0) {
  WriteTestData();

  ASSERT_TRUE(memory_.Init(tf_->path, 0));
  std::vector<char> buffer(11);
  ASSERT_TRUE(memory_.ReadFully(0, buffer.data(), 10));
  buffer[10] = '\0';
  ASSERT_STREQ("0123456789", buffer.data());
}

TEST_F(MemoryFileTest, init_offset_non_zero) {
  WriteTestData();

  ASSERT_TRUE(memory_.Init(tf_->path, 10));
  std::vector<char> buffer(11);
  ASSERT_TRUE(memory_.ReadFully(0, buffer.data(), 10));
  buffer[10] = '\0';
  ASSERT_STREQ("abcdefghij", buffer.data());
}

TEST_F(MemoryFileTest, init_offset_non_zero_larger_than_pagesize) {
  size_t pagesize = getpagesize();
  std::string large_string;
  for (size_t i = 0; i < pagesize; i++) {
    large_string += '1';
  }
  large_string += "012345678901234abcdefgh";
  ASSERT_TRUE(android::base::WriteStringToFd(large_string, tf_->fd));

  ASSERT_TRUE(memory_.Init(tf_->path, pagesize + 15));
  std::vector<char> buffer(9);
  ASSERT_TRUE(memory_.ReadFully(0, buffer.data(), 8));
  buffer[8] = '\0';
  ASSERT_STREQ("abcdefgh", buffer.data());
}

TEST_F(MemoryFileTest, init_offset_pagesize_aligned) {
  size_t pagesize = getpagesize();
  std::string data;
  for (size_t i = 0; i < 2 * pagesize; i++) {
    data += static_cast<char>((i / pagesize) + '0');
    data += static_cast<char>((i % 10) + '0');
  }
  ASSERT_TRUE(android::base::WriteStringToFd(data, tf_->fd));

  ASSERT_TRUE(memory_.Init(tf_->path, 2 * pagesize));
  std::vector<char> buffer(11);
  ASSERT_TRUE(memory_.ReadFully(0, buffer.data(), 10));
  buffer[10] = '\0';
  std::string expected_str;
  for (size_t i = 0; i < 5; i++) {
    expected_str += '1';
    expected_str += static_cast<char>(((i + pagesize) % 10) + '0');
  }
  ASSERT_STREQ(expected_str.c_str(), buffer.data());
}

TEST_F(MemoryFileTest, init_offset_pagesize_aligned_plus_extra) {
  size_t pagesize = getpagesize();
  std::string data;
  for (size_t i = 0; i < 2 * pagesize; i++) {
    data += static_cast<char>((i / pagesize) + '0');
    data += static_cast<char>((i % 10) + '0');
  }
  ASSERT_TRUE(android::base::WriteStringToFd(data, tf_->fd));

  ASSERT_TRUE(memory_.Init(tf_->path, 2 * pagesize + 10));
  std::vector<char> buffer(11);
  ASSERT_TRUE(memory_.ReadFully(0, buffer.data(), 10));
  buffer[10] = '\0';
  std::string expected_str;
  for (size_t i = 0; i < 5; i++) {
    expected_str += '1';
    expected_str += static_cast<char>(((i + pagesize + 5) % 10) + '0');
  }
  ASSERT_STREQ(expected_str.c_str(), buffer.data());
}

TEST_F(MemoryFileTest, init_offset_greater_than_filesize) {
  size_t pagesize = getpagesize();
  std::string data;
  uint64_t file_size = 2 * pagesize + pagesize / 2;
  for (size_t i = 0; i < file_size; i++) {
    data += static_cast<char>((i / pagesize) + '0');
  }
  ASSERT_TRUE(android::base::WriteStringToFd(data, tf_->fd));

  // Check offset > file size fails and aligned_offset > file size.
  ASSERT_FALSE(memory_.Init(tf_->path, file_size + 2 * pagesize));
  // Check offset == filesize fails.
  ASSERT_FALSE(memory_.Init(tf_->path, file_size));
  // Check aligned_offset < filesize, but offset > filesize fails.
  ASSERT_FALSE(memory_.Init(tf_->path, 2 * pagesize + pagesize / 2 + pagesize / 4));
}

TEST_F(MemoryFileTest, read_error) {
  std::string data;
  for (size_t i = 0; i < 5000; i++) {
    data += static_cast<char>((i % 10) + '0');
  }
  ASSERT_TRUE(android::base::WriteStringToFd(data, tf_->fd));

  std::vector<char> buffer(100);

  // Read before init.
  ASSERT_FALSE(memory_.ReadFully(0, buffer.data(), 10));

  ASSERT_TRUE(memory_.Init(tf_->path, 0));

  ASSERT_FALSE(memory_.ReadFully(10000, buffer.data(), 10));
  ASSERT_FALSE(memory_.ReadFully(5000, buffer.data(), 10));
  ASSERT_FALSE(memory_.ReadFully(4990, buffer.data(), 11));
  ASSERT_TRUE(memory_.ReadFully(4990, buffer.data(), 10));
  ASSERT_FALSE(memory_.ReadFully(4999, buffer.data(), 2));
  ASSERT_TRUE(memory_.ReadFully(4999, buffer.data(), 1));

  // Check that overflow fails properly.
  ASSERT_FALSE(memory_.ReadFully(UINT64_MAX - 100, buffer.data(), 200));
}

TEST_F(MemoryFileTest, read_past_file_within_mapping) {
  size_t pagesize = getpagesize();

  ASSERT_TRUE(pagesize > 100);
  std::vector<uint8_t> buffer(pagesize - 100);
  for (size_t i = 0; i < pagesize - 100; i++) {
    buffer[i] = static_cast<uint8_t>((i % 0x5e) + 0x20);
  }
  ASSERT_TRUE(android::base::WriteFully(tf_->fd, buffer.data(), buffer.size()));

  ASSERT_TRUE(memory_.Init(tf_->path, 0));

  for (size_t i = 0; i < 100; i++) {
    uint8_t value;
    ASSERT_FALSE(memory_.ReadFully(buffer.size() + i, &value, 1))
        << "Should have failed at value " << i;
  }
}

TEST_F(MemoryFileTest, map_partial_offset_aligned) {
  size_t pagesize = getpagesize();
  std::vector<uint8_t> buffer(pagesize * 10);
  for (size_t i = 0; i < pagesize * 10; i++) {
    buffer[i] = i / pagesize + 1;
  }
  ASSERT_TRUE(android::base::WriteFully(tf_->fd, buffer.data(), buffer.size()));

  // Map in only two pages of the data, and after the first page.
  ASSERT_TRUE(memory_.Init(tf_->path, pagesize, pagesize * 2));

  std::vector<uint8_t> read_buffer(pagesize * 2);
  // Make sure that reading after mapped data is a failure.
  ASSERT_FALSE(memory_.ReadFully(pagesize * 2, read_buffer.data(), 1));
  ASSERT_TRUE(memory_.ReadFully(0, read_buffer.data(), pagesize * 2));
  for (size_t i = 0; i < pagesize; i++) {
    ASSERT_EQ(2, read_buffer[i]) << "Failed at byte " << i;
  }
  for (size_t i = pagesize; i < pagesize * 2; i++) {
    ASSERT_EQ(3, read_buffer[i]) << "Failed at byte " << i;
  }
}

TEST_F(MemoryFileTest, map_partial_offset_unaligned) {
  size_t pagesize = getpagesize();
  ASSERT_TRUE(pagesize > 0x100);
  std::vector<uint8_t> buffer(pagesize * 10);
  for (size_t i = 0; i < buffer.size(); i++) {
    buffer[i] = i / pagesize + 1;
  }
  ASSERT_TRUE(android::base::WriteFully(tf_->fd, buffer.data(), buffer.size()));

  // Map in only two pages of the data, and after the first page.
  ASSERT_TRUE(memory_.Init(tf_->path, pagesize + 0x100, pagesize * 2));

  std::vector<uint8_t> read_buffer(pagesize * 2);
  // Make sure that reading after mapped data is a failure.
  ASSERT_FALSE(memory_.ReadFully(pagesize * 2, read_buffer.data(), 1));
  ASSERT_TRUE(memory_.ReadFully(0, read_buffer.data(), pagesize * 2));
  for (size_t i = 0; i < pagesize - 0x100; i++) {
    ASSERT_EQ(2, read_buffer[i]) << "Failed at byte " << i;
  }
  for (size_t i = pagesize - 0x100; i < 2 * pagesize - 0x100; i++) {
    ASSERT_EQ(3, read_buffer[i]) << "Failed at byte " << i;
  }
  for (size_t i = 2 * pagesize - 0x100; i < pagesize * 2; i++) {
    ASSERT_EQ(4, read_buffer[i]) << "Failed at byte " << i;
  }
}

TEST_F(MemoryFileTest, map_overflow) {
  size_t pagesize = getpagesize();
  ASSERT_TRUE(pagesize > 0x100);
  std::vector<uint8_t> buffer(pagesize * 10);
  for (size_t i = 0; i < buffer.size(); i++) {
    buffer[i] = i / pagesize + 1;
  }
  ASSERT_TRUE(android::base::WriteFully(tf_->fd, buffer.data(), buffer.size()));

  // Map in only two pages of the data, and after the first page.
  ASSERT_TRUE(memory_.Init(tf_->path, pagesize + 0x100, UINT64_MAX));

  std::vector<uint8_t> read_buffer(pagesize * 10);
  ASSERT_FALSE(memory_.ReadFully(pagesize * 9 - 0x100 + 1, read_buffer.data(), 1));
  ASSERT_TRUE(memory_.ReadFully(0, read_buffer.data(), pagesize * 9 - 0x100));
}

TEST_F(MemoryFileTest, init_reinit) {
  size_t pagesize = getpagesize();
  std::vector<uint8_t> buffer(pagesize * 2);
  for (size_t i = 0; i < buffer.size(); i++) {
    buffer[i] = i / pagesize + 1;
  }
  ASSERT_TRUE(android::base::WriteFully(tf_->fd, buffer.data(), buffer.size()));

  ASSERT_TRUE(memory_.Init(tf_->path, 0));
  std::vector<uint8_t> read_buffer(buffer.size());
  ASSERT_TRUE(memory_.ReadFully(0, read_buffer.data(), pagesize));
  for (size_t i = 0; i < pagesize; i++) {
    ASSERT_EQ(1, read_buffer[i]) << "Failed at byte " << i;
  }

  // Now reinit.
  ASSERT_TRUE(memory_.Init(tf_->path, pagesize));
  ASSERT_TRUE(memory_.ReadFully(0, read_buffer.data(), pagesize));
  for (size_t i = 0; i < pagesize; i++) {
    ASSERT_EQ(2, read_buffer[i]) << "Failed at byte " << i;
  }
}

}  // namespace unwindstack
