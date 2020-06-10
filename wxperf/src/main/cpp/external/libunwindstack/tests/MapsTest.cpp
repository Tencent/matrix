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

#include <inttypes.h>
#include <sys/mman.h>

#include <android-base/file.h>
#include <android-base/stringprintf.h>
#include <gtest/gtest.h>

#include <unwindstack/Maps.h>

namespace unwindstack {

static void VerifyLine(std::string line, MapInfo* info) {
  BufferMaps maps(line.c_str());

  if (info == nullptr) {
    ASSERT_FALSE(maps.Parse()) << "Failed on: " + line;
  } else {
    ASSERT_TRUE(maps.Parse()) << "Failed on: " + line;
    MapInfo* element = maps.Get(0);
    ASSERT_TRUE(element != nullptr) << "Failed on: " + line;
    info->start = element->start;
    info->end = element->end;
    info->offset = element->offset;
    info->flags = element->flags;
    info->name = element->name;
    info->elf_offset = element->elf_offset;
  }
}

TEST(MapsTest, map_add) {
  Maps maps;

  maps.Add(0x1000, 0x2000, 0, PROT_READ, "fake_map", 0);
  maps.Add(0x3000, 0x4000, 0x10, 0, "", 0x1234);
  maps.Add(0x5000, 0x6000, 1, 2, "fake_map2", static_cast<uint64_t>(-1));

  ASSERT_EQ(3U, maps.Total());
  MapInfo* info = maps.Get(0);
  ASSERT_EQ(0x1000U, info->start);
  ASSERT_EQ(0x2000U, info->end);
  ASSERT_EQ(0U, info->offset);
  ASSERT_EQ(PROT_READ, info->flags);
  ASSERT_EQ("fake_map", info->name);
  ASSERT_EQ(0U, info->elf_offset);
  ASSERT_EQ(0U, info->load_bias.load());
}

TEST(MapsTest, map_move) {
  Maps maps;

  maps.Add(0x1000, 0x2000, 0, PROT_READ, "fake_map", 0);
  maps.Add(0x3000, 0x4000, 0x10, 0, "", 0x1234);
  maps.Add(0x5000, 0x6000, 1, 2, "fake_map2", static_cast<uint64_t>(-1));

  Maps maps2 = std::move(maps);

  ASSERT_EQ(3U, maps2.Total());
  MapInfo* info = maps2.Get(0);
  ASSERT_EQ(0x1000U, info->start);
  ASSERT_EQ(0x2000U, info->end);
  ASSERT_EQ(0U, info->offset);
  ASSERT_EQ(PROT_READ, info->flags);
  ASSERT_EQ("fake_map", info->name);
  ASSERT_EQ(0U, info->elf_offset);
  ASSERT_EQ(0U, info->load_bias.load());
}

TEST(MapsTest, verify_parse_line) {
  MapInfo info(nullptr, 0, 0, 0, 0, "");

  VerifyLine("01-02 rwxp 03 04:05 06\n", &info);
  EXPECT_EQ(1U, info.start);
  EXPECT_EQ(2U, info.end);
  EXPECT_EQ(PROT_READ | PROT_WRITE | PROT_EXEC, info.flags);
  EXPECT_EQ(3U, info.offset);
  EXPECT_EQ("", info.name);

  VerifyLine("0a-0b ---s 0c 0d:0e 06 /fake/name\n", &info);
  EXPECT_EQ(0xaU, info.start);
  EXPECT_EQ(0xbU, info.end);
  EXPECT_EQ(0U, info.flags);
  EXPECT_EQ(0xcU, info.offset);
  EXPECT_EQ("/fake/name", info.name);

  VerifyLine("01-02   rwxp   03    04:05    06    /fake/name/again\n", &info);
  EXPECT_EQ(1U, info.start);
  EXPECT_EQ(2U, info.end);
  EXPECT_EQ(PROT_READ | PROT_WRITE | PROT_EXEC, info.flags);
  EXPECT_EQ(3U, info.offset);
  EXPECT_EQ("/fake/name/again", info.name);

  VerifyLine("-00 rwxp 00 00:00 0\n", nullptr);
  VerifyLine("00- rwxp 00 00:00 0\n", nullptr);
  VerifyLine("00-00 rwxp 00 :00 0\n", nullptr);
  VerifyLine("00-00 rwxp 00 00:00 \n", nullptr);
  VerifyLine("x-00 rwxp 00 00:00 0\n", nullptr);
  VerifyLine("00 -00 rwxp 00 00:00 0\n", nullptr);
  VerifyLine("00-x rwxp 00 00:00 0\n", nullptr);
  VerifyLine("00-x rwxp 00 00:00 0\n", nullptr);
  VerifyLine("00-00x rwxp 00 00:00 0\n", nullptr);
  VerifyLine("00-00 rwxp0 00 00:00 0\n", nullptr);
  VerifyLine("00-00 rwxp0 00 00:00 0\n", nullptr);
  VerifyLine("00-00 rwp 00 00:00 0\n", nullptr);
  VerifyLine("00-00 rwxp 0000:00 0\n", nullptr);
  VerifyLine("00-00 rwxp 00 00 :00 0\n", nullptr);
  VerifyLine("00-00 rwxp 00 00: 00 0\n", nullptr);
  VerifyLine("00-00 rwxp 00 00:000\n", nullptr);
  VerifyLine("00-00 rwxp 00 00:00 0/fake\n", nullptr);
  VerifyLine("00-00 xxxx 00 00:00 0 /fake\n", nullptr);
  VerifyLine("00-00 ywxp 00 00:00 0 /fake\n", nullptr);
  VerifyLine("00-00 ryxp 00 00:00 0 /fake\n", nullptr);
  VerifyLine("00-00 rwyp 00 00:00 0 /fake\n", nullptr);
  VerifyLine("00-00 rwx- 00 00:00 0 /fake\n", nullptr);
  VerifyLine("0\n", nullptr);
  VerifyLine("00\n", nullptr);
  VerifyLine("00-\n", nullptr);
  VerifyLine("00-0\n", nullptr);
  VerifyLine("00-00\n", nullptr);
  VerifyLine("00-00 \n", nullptr);
  VerifyLine("00-00 -\n", nullptr);
  VerifyLine("00-00 r\n", nullptr);
  VerifyLine("00-00 --\n", nullptr);
  VerifyLine("00-00 rw\n", nullptr);
  VerifyLine("00-00 ---\n", nullptr);
  VerifyLine("00-00 rwx\n", nullptr);
  VerifyLine("00-00 ---s\n", nullptr);
  VerifyLine("00-00 ---p\n", nullptr);
  VerifyLine("00-00 ---s 0\n", nullptr);
  VerifyLine("00-00 ---p 0 \n", nullptr);
  VerifyLine("00-00 ---p 0 0\n", nullptr);
  VerifyLine("00-00 ---p 0 0:\n", nullptr);
  VerifyLine("00-00 ---p 0 0:0\n", nullptr);
  VerifyLine("00-00 ---p 0 0:0 \n", nullptr);

  // Line to verify that the parser will detect a completely malformed line
  // properly.
  VerifyLine("7ffff7dda000-7ffff7dfd7ffff7ff3000-7ffff7ff4000 ---p 0000f000 fc:02 44171565\n",
             nullptr);
}

TEST(MapsTest, verify_large_values) {
  MapInfo info(nullptr, 0, 0, 0, 0, "");
#if defined(__LP64__)
  VerifyLine("fabcdef012345678-f12345678abcdef8 rwxp f0b0d0f010305070 00:00 0\n", &info);
  EXPECT_EQ(0xfabcdef012345678UL, info.start);
  EXPECT_EQ(0xf12345678abcdef8UL, info.end);
  EXPECT_EQ(PROT_READ | PROT_WRITE | PROT_EXEC, info.flags);
  EXPECT_EQ(0xf0b0d0f010305070UL, info.offset);
#else
  VerifyLine("f2345678-fabcdef8 rwxp f0305070 00:00 0\n", &info);
  EXPECT_EQ(0xf2345678UL, info.start);
  EXPECT_EQ(0xfabcdef8UL, info.end);
  EXPECT_EQ(PROT_READ | PROT_WRITE | PROT_EXEC, info.flags);
  EXPECT_EQ(0xf0305070UL, info.offset);
#endif
}

TEST(MapsTest, parse_permissions) {
  BufferMaps maps(
      "1000-2000 ---s 00000000 00:00 0\n"
      "2000-3000 r--s 00000000 00:00 0\n"
      "3000-4000 -w-s 00000000 00:00 0\n"
      "4000-5000 --xp 00000000 00:00 0\n"
      "5000-6000 rwxp 00000000 00:00 0\n");

  ASSERT_TRUE(maps.Parse());
  ASSERT_EQ(5U, maps.Total());

  MapInfo* info = maps.Get(0);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(PROT_NONE, info->flags);
  EXPECT_EQ(0x1000U, info->start);
  EXPECT_EQ(0x2000U, info->end);
  EXPECT_EQ(0U, info->offset);
  EXPECT_EQ("", info->name);

  info = maps.Get(1);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(PROT_READ, info->flags);
  EXPECT_EQ(0x2000U, info->start);
  EXPECT_EQ(0x3000U, info->end);
  EXPECT_EQ(0U, info->offset);
  EXPECT_EQ("", info->name);

  info = maps.Get(2);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(PROT_WRITE, info->flags);
  EXPECT_EQ(0x3000U, info->start);
  EXPECT_EQ(0x4000U, info->end);
  EXPECT_EQ(0U, info->offset);
  EXPECT_EQ("", info->name);

  info = maps.Get(3);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(PROT_EXEC, info->flags);
  EXPECT_EQ(0x4000U, info->start);
  EXPECT_EQ(0x5000U, info->end);
  EXPECT_EQ(0U, info->offset);
  EXPECT_EQ("", info->name);

  info = maps.Get(4);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(PROT_READ | PROT_WRITE | PROT_EXEC, info->flags);
  EXPECT_EQ(0x5000U, info->start);
  EXPECT_EQ(0x6000U, info->end);
  EXPECT_EQ(0U, info->offset);
  EXPECT_EQ("", info->name);

  ASSERT_TRUE(maps.Get(5) == nullptr);
}

TEST(MapsTest, parse_name) {
  BufferMaps maps(
      "7b29b000-7b29e000 rw-p 00000000 00:00 0\n"
      "7b29e000-7b29f000 rw-p 00000000 00:00 0 /system/lib/fake.so\n"
      "7b29f000-7b2a0000 rw-p 00000000 00:00 0");

  ASSERT_TRUE(maps.Parse());
  ASSERT_EQ(3U, maps.Total());

  MapInfo* info = maps.Get(0);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ("", info->name);
  EXPECT_EQ(0x7b29b000U, info->start);
  EXPECT_EQ(0x7b29e000U, info->end);
  EXPECT_EQ(0U, info->offset);
  EXPECT_EQ(PROT_READ | PROT_WRITE, info->flags);

  info = maps.Get(1);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ("/system/lib/fake.so", info->name);
  EXPECT_EQ(0x7b29e000U, info->start);
  EXPECT_EQ(0x7b29f000U, info->end);
  EXPECT_EQ(0U, info->offset);
  EXPECT_EQ(PROT_READ | PROT_WRITE, info->flags);

  info = maps.Get(2);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ("", info->name);
  EXPECT_EQ(0x7b29f000U, info->start);
  EXPECT_EQ(0x7b2a0000U, info->end);
  EXPECT_EQ(0U, info->offset);
  EXPECT_EQ(PROT_READ | PROT_WRITE, info->flags);

  ASSERT_TRUE(maps.Get(3) == nullptr);
}

TEST(MapsTest, parse_offset) {
  BufferMaps maps(
      "a000-e000 rw-p 00000000 00:00 0 /system/lib/fake.so\n"
      "e000-f000 rw-p 00a12345 00:00 0 /system/lib/fake.so\n");

  ASSERT_TRUE(maps.Parse());
  ASSERT_EQ(2U, maps.Total());

  MapInfo* info = maps.Get(0);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(0U, info->offset);
  EXPECT_EQ(0xa000U, info->start);
  EXPECT_EQ(0xe000U, info->end);
  EXPECT_EQ(PROT_READ | PROT_WRITE, info->flags);
  EXPECT_EQ("/system/lib/fake.so", info->name);

  info = maps.Get(1);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(0xa12345U, info->offset);
  EXPECT_EQ(0xe000U, info->start);
  EXPECT_EQ(0xf000U, info->end);
  EXPECT_EQ(PROT_READ | PROT_WRITE, info->flags);
  EXPECT_EQ("/system/lib/fake.so", info->name);

  ASSERT_TRUE(maps.Get(2) == nullptr);
}

TEST(MapsTest, iterate) {
  BufferMaps maps(
      "a000-e000 rw-p 00000000 00:00 0 /system/lib/fake.so\n"
      "e000-f000 rw-p 00a12345 00:00 0 /system/lib/fake.so\n");

  ASSERT_TRUE(maps.Parse());
  ASSERT_EQ(2U, maps.Total());

  Maps::iterator it = maps.begin();
  EXPECT_EQ(0xa000U, (*it)->start);
  EXPECT_EQ(0xe000U, (*it)->end);
  ++it;
  EXPECT_EQ(0xe000U, (*it)->start);
  EXPECT_EQ(0xf000U, (*it)->end);
  ++it;
  EXPECT_EQ(maps.end(), it);
}

TEST(MapsTest, const_iterate) {
  BufferMaps maps(
      "a000-e000 rw-p 00000000 00:00 0 /system/lib/fake.so\n"
      "e000-f000 rw-p 00a12345 00:00 0 /system/lib/fake.so\n");

  ASSERT_TRUE(maps.Parse());
  ASSERT_EQ(2U, maps.Total());

  Maps::const_iterator it = maps.begin();
  EXPECT_EQ(0xa000U, (*it)->start);
  EXPECT_EQ(0xe000U, (*it)->end);
  ++it;
  EXPECT_EQ(0xe000U, (*it)->start);
  EXPECT_EQ(0xf000U, (*it)->end);
  ++it;
  EXPECT_EQ(maps.end(), it);
}

TEST(MapsTest, device) {
  BufferMaps maps(
      "a000-e000 rw-p 00000000 00:00 0 /dev/\n"
      "f000-f100 rw-p 00000000 00:00 0 /dev/does_not_exist\n"
      "f100-f200 rw-p 00000000 00:00 0 /dev/ashmem/does_not_exist\n"
      "f200-f300 rw-p 00000000 00:00 0 /devsomething/does_not_exist\n");

  ASSERT_TRUE(maps.Parse());
  ASSERT_EQ(4U, maps.Total());

  MapInfo* info = maps.Get(0);
  ASSERT_TRUE(info != nullptr);
  EXPECT_TRUE(info->flags & 0x8000);
  EXPECT_EQ("/dev/", info->name);

  info = maps.Get(1);
  EXPECT_TRUE(info->flags & 0x8000);
  EXPECT_EQ("/dev/does_not_exist", info->name);

  info = maps.Get(2);
  EXPECT_FALSE(info->flags & 0x8000);
  EXPECT_EQ("/dev/ashmem/does_not_exist", info->name);

  info = maps.Get(3);
  EXPECT_FALSE(info->flags & 0x8000);
  EXPECT_EQ("/devsomething/does_not_exist", info->name);
}

TEST(MapsTest, file_smoke) {
  TemporaryFile tf;
  ASSERT_TRUE(tf.fd != -1);

  ASSERT_TRUE(
      android::base::WriteStringToFile("7b29b000-7b29e000 r-xp a0000000 00:00 0   /fake.so\n"
                                       "7b2b0000-7b2e0000 r-xp b0000000 00:00 0   /fake2.so\n"
                                       "7b2e0000-7b2f0000 r-xp c0000000 00:00 0   /fake3.so\n",
                                       tf.path, 0660, getuid(), getgid()));

  FileMaps maps(tf.path);

  ASSERT_TRUE(maps.Parse());
  ASSERT_EQ(3U, maps.Total());

  MapInfo* info = maps.Get(0);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(0x7b29b000U, info->start);
  EXPECT_EQ(0x7b29e000U, info->end);
  EXPECT_EQ(0xa0000000U, info->offset);
  EXPECT_EQ(PROT_READ | PROT_EXEC, info->flags);
  EXPECT_EQ("/fake.so", info->name);

  info = maps.Get(1);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(0x7b2b0000U, info->start);
  EXPECT_EQ(0x7b2e0000U, info->end);
  EXPECT_EQ(0xb0000000U, info->offset);
  EXPECT_EQ(PROT_READ | PROT_EXEC, info->flags);
  EXPECT_EQ("/fake2.so", info->name);

  info = maps.Get(2);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(0x7b2e0000U, info->start);
  EXPECT_EQ(0x7b2f0000U, info->end);
  EXPECT_EQ(0xc0000000U, info->offset);
  EXPECT_EQ(PROT_READ | PROT_EXEC, info->flags);
  EXPECT_EQ("/fake3.so", info->name);

  ASSERT_TRUE(maps.Get(3) == nullptr);
}

TEST(MapsTest, file_no_map_name) {
  TemporaryFile tf;
  ASSERT_TRUE(tf.fd != -1);

  ASSERT_TRUE(
      android::base::WriteStringToFile("7b29b000-7b29e000 r-xp a0000000 00:00 0\n"
                                       "7b2b0000-7b2e0000 r-xp b0000000 00:00 0   /fake2.so\n"
                                       "7b2e0000-7b2f0000 r-xp c0000000 00:00 0 \n",
                                       tf.path, 0660, getuid(), getgid()));

  FileMaps maps(tf.path);

  ASSERT_TRUE(maps.Parse());
  ASSERT_EQ(3U, maps.Total());

  MapInfo* info = maps.Get(0);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(0x7b29b000U, info->start);
  EXPECT_EQ(0x7b29e000U, info->end);
  EXPECT_EQ(0xa0000000U, info->offset);
  EXPECT_EQ(PROT_READ | PROT_EXEC, info->flags);
  EXPECT_EQ("", info->name);

  info = maps.Get(1);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(0x7b2b0000U, info->start);
  EXPECT_EQ(0x7b2e0000U, info->end);
  EXPECT_EQ(0xb0000000U, info->offset);
  EXPECT_EQ(PROT_READ | PROT_EXEC, info->flags);
  EXPECT_EQ("/fake2.so", info->name);

  info = maps.Get(2);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(0x7b2e0000U, info->start);
  EXPECT_EQ(0x7b2f0000U, info->end);
  EXPECT_EQ(0xc0000000U, info->offset);
  EXPECT_EQ(PROT_READ | PROT_EXEC, info->flags);
  EXPECT_EQ("", info->name);

  ASSERT_TRUE(maps.Get(3) == nullptr);
}

// Verify that a file that crosses a buffer is parsed correctly.
static std::string CreateEntry(size_t index) {
  return android::base::StringPrintf("%08zx-%08zx rwxp 0000 00:00 0\n", index * 4096,
                                     (index + 1) * 4096);
}

TEST(MapsTest, file_buffer_cross) {
  constexpr size_t kBufferSize = 2048;
  TemporaryFile tf;
  ASSERT_TRUE(tf.fd != -1);

  // Compute how many to add in the first buffer.
  size_t entry_len = CreateEntry(0).size();
  size_t index;
  std::string file_data;
  for (index = 0; index < kBufferSize / entry_len; index++) {
    file_data += CreateEntry(index);
  }
  // Add a long name to make sure that the first buffer does not contain a
  // complete line.
  // Remove the last newline.
  size_t extra = 0;
  size_t leftover = kBufferSize % entry_len;
  size_t overlap1_index = 0;
  std::string overlap1_name;
  if (leftover == 0) {
    // Exact match, add a long name to cross over the value.
    overlap1_name = "/fake/name/is/long/on/purpose";
    file_data.erase(file_data.size() - 1);
    file_data += ' ' + overlap1_name + '\n';
    extra = entry_len + overlap1_name.size() + 1;
    overlap1_index = index;
  }

  // Compute how many need to go in to hit the buffer boundary exactly.
  size_t bytes_left_in_buffer = kBufferSize - extra;
  size_t entries_to_add = bytes_left_in_buffer / entry_len + index;
  for (; index < entries_to_add; index++) {
    file_data += CreateEntry(index);
  }

  // Now figure out how many bytes to add to get exactly to the buffer boundary.
  leftover = bytes_left_in_buffer % entry_len;
  std::string overlap2_name;
  size_t overlap2_index = 0;
  if (leftover != 0) {
    file_data.erase(file_data.size() - 1);
    file_data += ' ';
    overlap2_name = std::string(leftover - 1, 'x');
    file_data += overlap2_name + '\n';
    overlap2_index = index - 1;
  }

  // Now add a few entries on the next page.
  for (size_t start = index; index < start + 10; index++) {
    file_data += CreateEntry(index);
  }

  ASSERT_TRUE(android::base::WriteStringToFile(file_data, tf.path, 0660, getuid(), getgid()));

  FileMaps maps(tf.path);
  ASSERT_TRUE(maps.Parse());
  EXPECT_EQ(index, maps.Total());
  // Verify all of the maps.
  for (size_t i = 0; i < index; i++) {
    MapInfo* info = maps.Get(i);
    ASSERT_TRUE(info != nullptr) << "Failed verifying index " + std::to_string(i);
    EXPECT_EQ(i * 4096, info->start) << "Failed verifying index " + std::to_string(i);
    EXPECT_EQ((i + 1) * 4096, info->end) << "Failed verifying index " + std::to_string(i);
    EXPECT_EQ(0U, info->offset) << "Failed verifying index " + std::to_string(i);
    if (overlap1_index != 0 && i == overlap1_index) {
      EXPECT_EQ(overlap1_name, info->name) << "Failed verifying overlap1 name " + std::to_string(i);
    } else if (overlap2_index != 0 && i == overlap2_index) {
      EXPECT_EQ(overlap2_name, info->name) << "Failed verifying overlap2 name " + std::to_string(i);
    } else {
      EXPECT_EQ("", info->name) << "Failed verifying index " + std::to_string(i);
    }
  }
}

TEST(MapsTest, file_should_fail) {
  TemporaryFile tf;
  ASSERT_TRUE(tf.fd != -1);

  ASSERT_TRUE(android::base::WriteStringToFile(
      "7ffff7dda000-7ffff7dfd7ffff7ff3000-7ffff7ff4000 ---p 0000f000 fc:02 44171565\n", tf.path,
      0660, getuid(), getgid()));

  FileMaps maps(tf.path);

  ASSERT_FALSE(maps.Parse());
}

// Create a maps file that is extremely large.
TEST(MapsTest, large_file) {
  TemporaryFile tf;
  ASSERT_TRUE(tf.fd != -1);

  std::string file_data;
  uint64_t start = 0x700000;
  for (size_t i = 0; i < 5000; i++) {
    file_data +=
        android::base::StringPrintf("%" PRIx64 "-%" PRIx64 " r-xp 1000 00:0 0 /fake%zu.so\n",
                                    start + i * 4096, start + (i + 1) * 4096, i);
  }

  ASSERT_TRUE(android::base::WriteStringToFile(file_data, tf.path, 0660, getuid(), getgid()));

  FileMaps maps(tf.path);

  ASSERT_TRUE(maps.Parse());
  ASSERT_EQ(5000U, maps.Total());
  for (size_t i = 0; i < 5000; i++) {
    MapInfo* info = maps.Get(i);
    EXPECT_EQ(start + i * 4096, info->start) << "Failed at map " + std::to_string(i);
    EXPECT_EQ(start + (i + 1) * 4096, info->end) << "Failed at map " + std::to_string(i);
    std::string name = "/fake" + std::to_string(i) + ".so";
    EXPECT_EQ(name, info->name) << "Failed at map " + std::to_string(i);
  }
}

TEST(MapsTest, find) {
  BufferMaps maps(
      "1000-2000 r--p 00000010 00:00 0 /system/lib/fake1.so\n"
      "3000-4000 -w-p 00000020 00:00 0 /system/lib/fake2.so\n"
      "6000-8000 --xp 00000030 00:00 0 /system/lib/fake3.so\n"
      "a000-b000 rw-p 00000040 00:00 0 /system/lib/fake4.so\n"
      "e000-f000 rwxp 00000050 00:00 0 /system/lib/fake5.so\n");
  ASSERT_TRUE(maps.Parse());
  ASSERT_EQ(5U, maps.Total());

  EXPECT_TRUE(maps.Find(0x500) == nullptr);
  EXPECT_TRUE(maps.Find(0x2000) == nullptr);
  EXPECT_TRUE(maps.Find(0x5010) == nullptr);
  EXPECT_TRUE(maps.Find(0x9a00) == nullptr);
  EXPECT_TRUE(maps.Find(0xf000) == nullptr);
  EXPECT_TRUE(maps.Find(0xf010) == nullptr);

  MapInfo* info = maps.Find(0x1000);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(0x1000U, info->start);
  EXPECT_EQ(0x2000U, info->end);
  EXPECT_EQ(0x10U, info->offset);
  EXPECT_EQ(PROT_READ, info->flags);
  EXPECT_EQ("/system/lib/fake1.so", info->name);

  info = maps.Find(0x3020);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(0x3000U, info->start);
  EXPECT_EQ(0x4000U, info->end);
  EXPECT_EQ(0x20U, info->offset);
  EXPECT_EQ(PROT_WRITE, info->flags);
  EXPECT_EQ("/system/lib/fake2.so", info->name);

  info = maps.Find(0x6020);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(0x6000U, info->start);
  EXPECT_EQ(0x8000U, info->end);
  EXPECT_EQ(0x30U, info->offset);
  EXPECT_EQ(PROT_EXEC, info->flags);
  EXPECT_EQ("/system/lib/fake3.so", info->name);

  info = maps.Find(0xafff);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(0xa000U, info->start);
  EXPECT_EQ(0xb000U, info->end);
  EXPECT_EQ(0x40U, info->offset);
  EXPECT_EQ(PROT_READ | PROT_WRITE, info->flags);
  EXPECT_EQ("/system/lib/fake4.so", info->name);

  info = maps.Find(0xe500);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(0xe000U, info->start);
  EXPECT_EQ(0xf000U, info->end);
  EXPECT_EQ(0x50U, info->offset);
  EXPECT_EQ(PROT_READ | PROT_WRITE | PROT_EXEC, info->flags);
  EXPECT_EQ("/system/lib/fake5.so", info->name);
}

}  // namespace unwindstack
