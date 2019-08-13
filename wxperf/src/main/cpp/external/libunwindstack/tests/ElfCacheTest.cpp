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

#include <elf.h>
#include <unistd.h>

#include <android-base/file.h>
#include <android-base/test_utils.h>

#include <gtest/gtest.h>

#include <unwindstack/Elf.h>
#include <unwindstack/MapInfo.h>

#include "ElfTestUtils.h"
#include "MemoryFake.h"

namespace unwindstack {

class ElfCacheTest : public ::testing::Test {
 protected:
  static void SetUpTestCase() { memory_.reset(new MemoryFake); }

  void SetUp() override { Elf::SetCachingEnabled(true); }

  void TearDown() override { Elf::SetCachingEnabled(false); }

  void WriteElfFile(uint64_t offset, TemporaryFile* tf, uint32_t type) {
    ASSERT_TRUE(type == EM_ARM || type == EM_386 || type == EM_X86_64);
    size_t ehdr_size;
    Elf32_Ehdr ehdr32;
    Elf64_Ehdr ehdr64;
    void* ptr;
    if (type == EM_ARM || type == EM_386) {
      ehdr_size = sizeof(ehdr32);
      ptr = &ehdr32;
      TestInitEhdr(&ehdr32, ELFCLASS32, type);
    } else {
      ehdr_size = sizeof(ehdr64);
      ptr = &ehdr64;
      TestInitEhdr(&ehdr64, ELFCLASS64, type);
    }

    ASSERT_EQ(offset, static_cast<uint64_t>(lseek(tf->fd, offset, SEEK_SET)));
    ASSERT_TRUE(android::base::WriteFully(tf->fd, ptr, ehdr_size));
  }

  void VerifyWithinSameMap(bool cache_enabled);
  void VerifySameMap(bool cache_enabled);
  void VerifyWithinSameMapNeverReadAtZero(bool cache_enabled);

  static std::shared_ptr<Memory> memory_;
};

std::shared_ptr<Memory> ElfCacheTest::memory_;

void ElfCacheTest::VerifySameMap(bool cache_enabled) {
  if (!cache_enabled) {
    Elf::SetCachingEnabled(false);
  }

  TemporaryFile tf;
  ASSERT_TRUE(tf.fd != -1);
  WriteElfFile(0, &tf, EM_ARM);
  close(tf.fd);

  uint64_t start = 0x1000;
  uint64_t end = 0x20000;
  MapInfo info1(start, end, 0, 0x5, tf.path);
  MapInfo info2(start, end, 0, 0x5, tf.path);

  Elf* elf1 = info1.GetElf(memory_, true);
  ASSERT_TRUE(elf1->valid());
  Elf* elf2 = info2.GetElf(memory_, true);
  ASSERT_TRUE(elf2->valid());

  if (cache_enabled) {
    EXPECT_EQ(elf1, elf2);
  } else {
    EXPECT_NE(elf1, elf2);
  }
}

TEST_F(ElfCacheTest, no_caching) {
  VerifySameMap(false);
}

TEST_F(ElfCacheTest, caching_invalid_elf) {
  VerifySameMap(true);
}

void ElfCacheTest::VerifyWithinSameMap(bool cache_enabled) {
  if (!cache_enabled) {
    Elf::SetCachingEnabled(false);
  }

  TemporaryFile tf;
  ASSERT_TRUE(tf.fd != -1);
  WriteElfFile(0, &tf, EM_ARM);
  WriteElfFile(0x100, &tf, EM_386);
  WriteElfFile(0x200, &tf, EM_X86_64);
  lseek(tf.fd, 0x500, SEEK_SET);
  uint8_t value = 0;
  write(tf.fd, &value, 1);
  close(tf.fd);

  uint64_t start = 0x1000;
  uint64_t end = 0x20000;
  // Will have an elf at offset 0 in file.
  MapInfo info0_1(start, end, 0, 0x5, tf.path);
  MapInfo info0_2(start, end, 0, 0x5, tf.path);
  // Will have an elf at offset 0x100 in file.
  MapInfo info100_1(start, end, 0x100, 0x5, tf.path);
  MapInfo info100_2(start, end, 0x100, 0x5, tf.path);
  // Will have an elf at offset 0x200 in file.
  MapInfo info200_1(start, end, 0x200, 0x5, tf.path);
  MapInfo info200_2(start, end, 0x200, 0x5, tf.path);
  // Will have an elf at offset 0 in file.
  MapInfo info300_1(start, end, 0x300, 0x5, tf.path);
  MapInfo info300_2(start, end, 0x300, 0x5, tf.path);

  Elf* elf0_1 = info0_1.GetElf(memory_, true);
  ASSERT_TRUE(elf0_1->valid());
  EXPECT_EQ(ARCH_ARM, elf0_1->arch());
  Elf* elf0_2 = info0_2.GetElf(memory_, true);
  ASSERT_TRUE(elf0_2->valid());
  EXPECT_EQ(ARCH_ARM, elf0_2->arch());
  EXPECT_EQ(0U, info0_1.elf_offset);
  EXPECT_EQ(0U, info0_2.elf_offset);
  if (cache_enabled) {
    EXPECT_EQ(elf0_1, elf0_2);
  } else {
    EXPECT_NE(elf0_1, elf0_2);
  }

  Elf* elf100_1 = info100_1.GetElf(memory_, true);
  ASSERT_TRUE(elf100_1->valid());
  EXPECT_EQ(ARCH_X86, elf100_1->arch());
  Elf* elf100_2 = info100_2.GetElf(memory_, true);
  ASSERT_TRUE(elf100_2->valid());
  EXPECT_EQ(ARCH_X86, elf100_2->arch());
  EXPECT_EQ(0U, info100_1.elf_offset);
  EXPECT_EQ(0U, info100_2.elf_offset);
  if (cache_enabled) {
    EXPECT_EQ(elf100_1, elf100_2);
  } else {
    EXPECT_NE(elf100_1, elf100_2);
  }

  Elf* elf200_1 = info200_1.GetElf(memory_, true);
  ASSERT_TRUE(elf200_1->valid());
  EXPECT_EQ(ARCH_X86_64, elf200_1->arch());
  Elf* elf200_2 = info200_2.GetElf(memory_, true);
  ASSERT_TRUE(elf200_2->valid());
  EXPECT_EQ(ARCH_X86_64, elf200_2->arch());
  EXPECT_EQ(0U, info200_1.elf_offset);
  EXPECT_EQ(0U, info200_2.elf_offset);
  if (cache_enabled) {
    EXPECT_EQ(elf200_1, elf200_2);
  } else {
    EXPECT_NE(elf200_1, elf200_2);
  }

  Elf* elf300_1 = info300_1.GetElf(memory_, true);
  ASSERT_TRUE(elf300_1->valid());
  EXPECT_EQ(ARCH_ARM, elf300_1->arch());
  Elf* elf300_2 = info300_2.GetElf(memory_, true);
  ASSERT_TRUE(elf300_2->valid());
  EXPECT_EQ(ARCH_ARM, elf300_2->arch());
  EXPECT_EQ(0x300U, info300_1.elf_offset);
  EXPECT_EQ(0x300U, info300_2.elf_offset);
  if (cache_enabled) {
    EXPECT_EQ(elf300_1, elf300_2);
    EXPECT_EQ(elf0_1, elf300_1);
  } else {
    EXPECT_NE(elf300_1, elf300_2);
    EXPECT_NE(elf0_1, elf300_1);
  }
}

TEST_F(ElfCacheTest, no_caching_valid_elf_offset_non_zero) {
  VerifyWithinSameMap(false);
}

TEST_F(ElfCacheTest, caching_valid_elf_offset_non_zero) {
  VerifyWithinSameMap(true);
}

// Verify that when reading from multiple non-zero offsets in the same map
// that when cached, all of the elf objects are the same.
void ElfCacheTest::VerifyWithinSameMapNeverReadAtZero(bool cache_enabled) {
  if (!cache_enabled) {
    Elf::SetCachingEnabled(false);
  }

  TemporaryFile tf;
  ASSERT_TRUE(tf.fd != -1);
  WriteElfFile(0, &tf, EM_ARM);
  lseek(tf.fd, 0x500, SEEK_SET);
  uint8_t value = 0;
  write(tf.fd, &value, 1);
  close(tf.fd);

  uint64_t start = 0x1000;
  uint64_t end = 0x20000;
  // Multiple info sections at different offsets will have non-zero elf offsets.
  MapInfo info300_1(start, end, 0x300, 0x5, tf.path);
  MapInfo info300_2(start, end, 0x300, 0x5, tf.path);
  MapInfo info400_1(start, end, 0x400, 0x5, tf.path);
  MapInfo info400_2(start, end, 0x400, 0x5, tf.path);

  Elf* elf300_1 = info300_1.GetElf(memory_, true);
  ASSERT_TRUE(elf300_1->valid());
  EXPECT_EQ(ARCH_ARM, elf300_1->arch());
  Elf* elf300_2 = info300_2.GetElf(memory_, true);
  ASSERT_TRUE(elf300_2->valid());
  EXPECT_EQ(ARCH_ARM, elf300_2->arch());
  EXPECT_EQ(0x300U, info300_1.elf_offset);
  EXPECT_EQ(0x300U, info300_2.elf_offset);
  if (cache_enabled) {
    EXPECT_EQ(elf300_1, elf300_2);
  } else {
    EXPECT_NE(elf300_1, elf300_2);
  }

  Elf* elf400_1 = info400_1.GetElf(memory_, true);
  ASSERT_TRUE(elf400_1->valid());
  EXPECT_EQ(ARCH_ARM, elf400_1->arch());
  Elf* elf400_2 = info400_2.GetElf(memory_, true);
  ASSERT_TRUE(elf400_2->valid());
  EXPECT_EQ(ARCH_ARM, elf400_2->arch());
  EXPECT_EQ(0x400U, info400_1.elf_offset);
  EXPECT_EQ(0x400U, info400_2.elf_offset);
  if (cache_enabled) {
    EXPECT_EQ(elf400_1, elf400_2);
    EXPECT_EQ(elf300_1, elf400_1);
  } else {
    EXPECT_NE(elf400_1, elf400_2);
    EXPECT_NE(elf300_1, elf400_1);
  }
}

TEST_F(ElfCacheTest, no_caching_valid_elf_offset_non_zero_never_read_at_zero) {
  VerifyWithinSameMapNeverReadAtZero(false);
}

TEST_F(ElfCacheTest, caching_valid_elf_offset_non_zero_never_read_at_zero) {
  VerifyWithinSameMapNeverReadAtZero(true);
}

}  // namespace unwindstack
