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

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <unwindstack/Memory.h>

#include "MemoryFake.h"

namespace unwindstack {

TEST(MemoryTest, read32) {
  MemoryFakeAlwaysReadZero memory;

  uint32_t data = 0xffffffff;
  ASSERT_TRUE(memory.Read32(0, &data));
  ASSERT_EQ(0U, data);
}

TEST(MemoryTest, read64) {
  MemoryFakeAlwaysReadZero memory;

  uint64_t data = 0xffffffffffffffffULL;
  ASSERT_TRUE(memory.Read64(0, &data));
  ASSERT_EQ(0U, data);
}

struct FakeStruct {
  int one;
  bool two;
  uint32_t three;
  uint64_t four;
};

TEST(MemoryTest, read_string) {
  std::string name("string_in_memory");

  MemoryFake memory;

  memory.SetMemory(100, name.c_str(), name.size() + 1);

  std::string dst_name;
  ASSERT_TRUE(memory.ReadString(100, &dst_name));
  ASSERT_EQ("string_in_memory", dst_name);

  ASSERT_TRUE(memory.ReadString(107, &dst_name));
  ASSERT_EQ("in_memory", dst_name);

  // Set size greater than string.
  ASSERT_TRUE(memory.ReadString(107, &dst_name, 10));
  ASSERT_EQ("in_memory", dst_name);

  ASSERT_FALSE(memory.ReadString(107, &dst_name, 9));
}

TEST(MemoryTest, read_string_error) {
  std::string name("short");

  MemoryFake memory;

  // Save everything except the terminating '\0'.
  memory.SetMemory(0, name.c_str(), name.size());

  std::string dst_name;
  // Read from a non-existant address.
  ASSERT_FALSE(memory.ReadString(100, &dst_name));

  // This should fail because there is no terminating '\0'.
  ASSERT_FALSE(memory.ReadString(0, &dst_name));

  // This should pass because there is a terminating '\0'.
  memory.SetData8(name.size(), '\0');
  ASSERT_TRUE(memory.ReadString(0, &dst_name));
  ASSERT_EQ("short", dst_name);
}

}  // namespace unwindstack
