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

#include <elf.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <vector>

#include <android-base/file.h>
#include <gtest/gtest.h>

#include <unwindstack/Elf.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>

#include "ElfTestUtils.h"
#include "MemoryFake.h"

namespace unwindstack {

class MapInfoCreateMemoryTest : public ::testing::Test {
 protected:
  template <typename Ehdr, typename Shdr>
  static void InitElf(int fd, uint64_t file_offset, uint64_t sh_offset, uint8_t class_type) {
    std::vector<uint8_t> buffer(20000);
    memset(buffer.data(), 0, buffer.size());

    Ehdr ehdr;
    memset(&ehdr, 0, sizeof(ehdr));
    memcpy(ehdr.e_ident, ELFMAG, SELFMAG);
    ehdr.e_ident[EI_CLASS] = class_type;
    ehdr.e_shoff = sh_offset;
    ehdr.e_shentsize = sizeof(Shdr) + 100;
    ehdr.e_shnum = 4;
    memcpy(&buffer[file_offset], &ehdr, sizeof(ehdr));

    ASSERT_TRUE(android::base::WriteFully(fd, buffer.data(), buffer.size()));
  }

  static void SetUpTestCase() {
    std::vector<uint8_t> buffer(12288, 0);
    memcpy(buffer.data(), ELFMAG, SELFMAG);
    buffer[EI_CLASS] = ELFCLASS32;
    ASSERT_TRUE(android::base::WriteFully(elf_.fd, buffer.data(), 1024));

    memset(buffer.data(), 0, buffer.size());
    memcpy(&buffer[0x1000], ELFMAG, SELFMAG);
    buffer[0x1000 + EI_CLASS] = ELFCLASS64;
    buffer[0x2000] = 0xff;
    ASSERT_TRUE(android::base::WriteFully(elf_at_1000_.fd, buffer.data(), buffer.size()));

    InitElf<Elf32_Ehdr, Elf32_Shdr>(elf32_at_map_.fd, 0x1000, 0x2000, ELFCLASS32);
    InitElf<Elf64_Ehdr, Elf64_Shdr>(elf64_at_map_.fd, 0x2000, 0x3000, ELFCLASS64);
  }

  void SetUp() override {
    memory_ = new MemoryFake;
    process_memory_.reset(memory_);
  }

  MemoryFake* memory_;
  std::shared_ptr<Memory> process_memory_;

  static TemporaryFile elf_;

  static TemporaryFile elf_at_1000_;

  static TemporaryFile elf32_at_map_;
  static TemporaryFile elf64_at_map_;
};
TemporaryFile MapInfoCreateMemoryTest::elf_;
TemporaryFile MapInfoCreateMemoryTest::elf_at_1000_;
TemporaryFile MapInfoCreateMemoryTest::elf32_at_map_;
TemporaryFile MapInfoCreateMemoryTest::elf64_at_map_;

TEST_F(MapInfoCreateMemoryTest, end_le_start) {
  MapInfo info(nullptr, 0x100, 0x100, 0, 0, elf_.path);

  std::unique_ptr<Memory> memory(info.CreateMemory(process_memory_));
  ASSERT_TRUE(memory.get() == nullptr);

  info.end = 0xff;
  memory.reset(info.CreateMemory(process_memory_));
  ASSERT_TRUE(memory.get() == nullptr);

  // Make sure this test is valid.
  info.end = 0x101;
  memory.reset(info.CreateMemory(process_memory_));
  ASSERT_TRUE(memory.get() != nullptr);
  EXPECT_FALSE(info.memory_backed_elf);
}

// Verify that if the offset is non-zero but there is no elf at the offset,
// that the full file is used.
TEST_F(MapInfoCreateMemoryTest, file_backed_non_zero_offset_full_file) {
  MapInfo info(nullptr, 0x100, 0x200, 0x100, 0, elf_.path);

  std::unique_ptr<Memory> memory(info.CreateMemory(process_memory_));
  ASSERT_TRUE(memory.get() != nullptr);
  EXPECT_FALSE(info.memory_backed_elf);
  ASSERT_EQ(0x100U, info.elf_offset);
  EXPECT_EQ(0x100U, info.elf_start_offset);

  // Read the entire file.
  std::vector<uint8_t> buffer(1024);
  ASSERT_TRUE(memory->ReadFully(0, buffer.data(), 1024));
  ASSERT_TRUE(memcmp(buffer.data(), ELFMAG, SELFMAG) == 0);
  ASSERT_EQ(ELFCLASS32, buffer[EI_CLASS]);
  for (size_t i = EI_CLASS + 1; i < buffer.size(); i++) {
    ASSERT_EQ(0, buffer[i]) << "Failed at byte " << i;
  }

  ASSERT_FALSE(memory->ReadFully(1024, buffer.data(), 1));

  // Now verify the elf start offset is set correctly based on the previous
  // info.
  MapInfo prev_info(nullptr, 0, 0x100, 0x10, 0, "");
  info.prev_map = &prev_info;

  // No preconditions met, change each one until it should set the elf start
  // offset to zero.
  info.elf_offset = 0;
  info.elf_start_offset = 0;
  info.memory_backed_elf = false;
  memory.reset(info.CreateMemory(process_memory_));
  ASSERT_TRUE(memory.get() != nullptr);
  EXPECT_FALSE(info.memory_backed_elf);
  ASSERT_EQ(0x100U, info.elf_offset);
  EXPECT_EQ(0x100U, info.elf_start_offset);

  prev_info.offset = 0;
  info.elf_offset = 0;
  info.elf_start_offset = 0;
  info.memory_backed_elf = false;
  memory.reset(info.CreateMemory(process_memory_));
  ASSERT_TRUE(memory.get() != nullptr);
  EXPECT_FALSE(info.memory_backed_elf);
  ASSERT_EQ(0x100U, info.elf_offset);
  EXPECT_EQ(0x100U, info.elf_start_offset);

  prev_info.flags = PROT_READ;
  info.elf_offset = 0;
  info.elf_start_offset = 0;
  info.memory_backed_elf = false;
  memory.reset(info.CreateMemory(process_memory_));
  ASSERT_TRUE(memory.get() != nullptr);
  EXPECT_FALSE(info.memory_backed_elf);
  ASSERT_EQ(0x100U, info.elf_offset);
  EXPECT_EQ(0x100U, info.elf_start_offset);

  prev_info.name = info.name;
  info.elf_offset = 0;
  info.elf_start_offset = 0;
  info.memory_backed_elf = false;
  memory.reset(info.CreateMemory(process_memory_));
  ASSERT_TRUE(memory.get() != nullptr);
  EXPECT_FALSE(info.memory_backed_elf);
  ASSERT_EQ(0x100U, info.elf_offset);
  EXPECT_EQ(0U, info.elf_start_offset);
}

// Verify that if the offset is non-zero and there is an elf at that
// offset, that only part of the file is used.
TEST_F(MapInfoCreateMemoryTest, file_backed_non_zero_offset_partial_file) {
  MapInfo info(nullptr, 0x100, 0x200, 0x1000, 0, elf_at_1000_.path);

  std::unique_ptr<Memory> memory(info.CreateMemory(process_memory_));
  ASSERT_TRUE(memory.get() != nullptr);
  EXPECT_FALSE(info.memory_backed_elf);
  ASSERT_EQ(0U, info.elf_offset);
  EXPECT_EQ(0x1000U, info.elf_start_offset);

  // Read the valid part of the file.
  std::vector<uint8_t> buffer(0x100);
  ASSERT_TRUE(memory->ReadFully(0, buffer.data(), 0x100));
  ASSERT_TRUE(memcmp(buffer.data(), ELFMAG, SELFMAG) == 0);
  ASSERT_EQ(ELFCLASS64, buffer[EI_CLASS]);
  for (size_t i = EI_CLASS + 1; i < buffer.size(); i++) {
    ASSERT_EQ(0, buffer[i]) << "Failed at byte " << i;
  }

  ASSERT_FALSE(memory->ReadFully(0x100, buffer.data(), 1));
}

// Verify that if the offset is non-zero and there is an elf at that
// offset, that only part of the file is used. Further verify that if the
// embedded elf is bigger than the initial map, the new object is larger
// than the original map size. Do this for a 32 bit elf and a 64 bit elf.
TEST_F(MapInfoCreateMemoryTest, file_backed_non_zero_offset_partial_file_whole_elf32) {
  MapInfo info(nullptr, 0x5000, 0x6000, 0x1000, 0, elf32_at_map_.path);

  std::unique_ptr<Memory> memory(info.CreateMemory(process_memory_));
  ASSERT_TRUE(memory.get() != nullptr);
  EXPECT_FALSE(info.memory_backed_elf);
  ASSERT_EQ(0U, info.elf_offset);
  EXPECT_EQ(0x1000U, info.elf_start_offset);

  // Verify the memory is a valid elf.
  uint8_t e_ident[SELFMAG + 1];
  ASSERT_TRUE(memory->ReadFully(0, e_ident, SELFMAG));
  ASSERT_EQ(0, memcmp(e_ident, ELFMAG, SELFMAG));

  // Read past the end of what would normally be the size of the map.
  ASSERT_TRUE(memory->ReadFully(0x1000, e_ident, 1));
}

TEST_F(MapInfoCreateMemoryTest, file_backed_non_zero_offset_partial_file_whole_elf64) {
  MapInfo info(nullptr, 0x7000, 0x8000, 0x2000, 0, elf64_at_map_.path);

  std::unique_ptr<Memory> memory(info.CreateMemory(process_memory_));
  ASSERT_TRUE(memory.get() != nullptr);
  EXPECT_FALSE(info.memory_backed_elf);
  ASSERT_EQ(0U, info.elf_offset);
  EXPECT_EQ(0x2000U, info.elf_start_offset);

  // Verify the memory is a valid elf.
  uint8_t e_ident[SELFMAG + 1];
  ASSERT_TRUE(memory->ReadFully(0, e_ident, SELFMAG));
  ASSERT_EQ(0, memcmp(e_ident, ELFMAG, SELFMAG));

  // Read past the end of what would normally be the size of the map.
  ASSERT_TRUE(memory->ReadFully(0x1000, e_ident, 1));
}

// Verify that device file names will never result in Memory object creation.
TEST_F(MapInfoCreateMemoryTest, check_device_maps) {
  // Set up some memory so that a valid local memory object would
  // be returned if the file mapping fails, but the device check is incorrect.
  std::vector<uint8_t> buffer(1024);
  uint64_t start = reinterpret_cast<uint64_t>(buffer.data());
  MapInfo info(nullptr, start, start + buffer.size(), 0, 0x8000, "/dev/something");

  std::unique_ptr<Memory> memory(info.CreateMemory(process_memory_));
  ASSERT_TRUE(memory.get() == nullptr);
}

TEST_F(MapInfoCreateMemoryTest, process_memory) {
  MapInfo info(nullptr, 0x2000, 0x3000, 0, PROT_READ, "");

  Elf32_Ehdr ehdr = {};
  TestInitEhdr<Elf32_Ehdr>(&ehdr, ELFCLASS32, EM_ARM);
  std::vector<uint8_t> buffer(1024);
  memcpy(buffer.data(), &ehdr, sizeof(ehdr));

  // Verify that the the process_memory object is used, so seed it
  // with memory.
  for (size_t i = sizeof(ehdr); i < buffer.size(); i++) {
    buffer[i] = i % 256;
  }
  memory_->SetMemory(info.start, buffer.data(), buffer.size());

  std::unique_ptr<Memory> memory(info.CreateMemory(process_memory_));
  ASSERT_TRUE(memory.get() != nullptr);
  EXPECT_TRUE(info.memory_backed_elf);

  memset(buffer.data(), 0, buffer.size());
  ASSERT_TRUE(memory->ReadFully(0, buffer.data(), buffer.size()));
  ASSERT_EQ(0, memcmp(&ehdr, buffer.data(), sizeof(ehdr)));
  for (size_t i = sizeof(ehdr); i < buffer.size(); i++) {
    ASSERT_EQ(i % 256, buffer[i]) << "Failed at byte " << i;
  }

  // Try to read outside of the map size.
  ASSERT_FALSE(memory->ReadFully(buffer.size(), buffer.data(), 1));
}

TEST_F(MapInfoCreateMemoryTest, valid_rosegment_zero_offset) {
  Maps maps;
  maps.Add(0x500, 0x600, 0, PROT_READ, "something_else", 0);
  maps.Add(0x1000, 0x2600, 0, PROT_READ, "/only/in/memory.so", 0);
  maps.Add(0x3000, 0x5000, 0x4000, PROT_READ | PROT_EXEC, "/only/in/memory.so", 0);

  Elf32_Ehdr ehdr = {};
  TestInitEhdr<Elf32_Ehdr>(&ehdr, ELFCLASS32, EM_ARM);
  memory_->SetMemory(0x1000, &ehdr, sizeof(ehdr));
  memory_->SetMemoryBlock(0x1000 + sizeof(ehdr), 0x1600 - sizeof(ehdr), 0xab);

  // Set the memory in the r-x map.
  memory_->SetMemoryBlock(0x3000, 0x2000, 0x5d);

  MapInfo* map_info = maps.Find(0x3000);
  ASSERT_TRUE(map_info != nullptr);

  std::unique_ptr<Memory> mem(map_info->CreateMemory(process_memory_));
  ASSERT_TRUE(mem.get() != nullptr);
  EXPECT_TRUE(map_info->memory_backed_elf);
  EXPECT_EQ(0x4000UL, map_info->elf_offset);
  EXPECT_EQ(0x4000UL, map_info->offset);
  EXPECT_EQ(0U, map_info->elf_start_offset);

  // Verify that reading values from this memory works properly.
  std::vector<uint8_t> buffer(0x4000);
  size_t bytes = mem->Read(0, buffer.data(), buffer.size());
  ASSERT_EQ(0x1600UL, bytes);
  ASSERT_EQ(0, memcmp(&ehdr, buffer.data(), sizeof(ehdr)));
  for (size_t i = sizeof(ehdr); i < bytes; i++) {
    ASSERT_EQ(0xab, buffer[i]) << "Failed at byte " << i;
  }

  bytes = mem->Read(0x4000, buffer.data(), buffer.size());
  ASSERT_EQ(0x2000UL, bytes);
  for (size_t i = 0; i < bytes; i++) {
    ASSERT_EQ(0x5d, buffer[i]) << "Failed at byte " << i;
  }
}

TEST_F(MapInfoCreateMemoryTest, valid_rosegment_non_zero_offset) {
  Maps maps;
  maps.Add(0x500, 0x600, 0, PROT_READ, "something_else", 0);
  maps.Add(0x1000, 0x2000, 0, PROT_READ, "/only/in/memory.apk", 0);
  maps.Add(0x2000, 0x3000, 0x1000, PROT_READ | PROT_EXEC, "/only/in/memory.apk", 0);
  maps.Add(0x3000, 0x4000, 0xa000, PROT_READ, "/only/in/memory.apk", 0);
  maps.Add(0x4000, 0x5000, 0xb000, PROT_READ | PROT_EXEC, "/only/in/memory.apk", 0);

  Elf32_Ehdr ehdr = {};
  TestInitEhdr<Elf32_Ehdr>(&ehdr, ELFCLASS32, EM_ARM);

  // Setup an elf at offset 0x1000 in memory.
  memory_->SetMemory(0x1000, &ehdr, sizeof(ehdr));
  memory_->SetMemoryBlock(0x1000 + sizeof(ehdr), 0x2000 - sizeof(ehdr), 0x12);
  memory_->SetMemoryBlock(0x2000, 0x1000, 0x23);

  // Setup an elf at offset 0x3000 in memory..
  memory_->SetMemory(0x3000, &ehdr, sizeof(ehdr));
  memory_->SetMemoryBlock(0x3000 + sizeof(ehdr), 0x4000 - sizeof(ehdr), 0x34);
  memory_->SetMemoryBlock(0x4000, 0x1000, 0x43);

  MapInfo* map_info = maps.Find(0x4000);
  ASSERT_TRUE(map_info != nullptr);

  std::unique_ptr<Memory> mem(map_info->CreateMemory(process_memory_));
  ASSERT_TRUE(mem.get() != nullptr);
  EXPECT_TRUE(map_info->memory_backed_elf);
  EXPECT_EQ(0x1000UL, map_info->elf_offset);
  EXPECT_EQ(0xb000UL, map_info->offset);
  EXPECT_EQ(0xa000UL, map_info->elf_start_offset);

  // Verify that reading values from this memory works properly.
  std::vector<uint8_t> buffer(0x4000);
  size_t bytes = mem->Read(0, buffer.data(), buffer.size());
  ASSERT_EQ(0x1000UL, bytes);
  ASSERT_EQ(0, memcmp(&ehdr, buffer.data(), sizeof(ehdr)));
  for (size_t i = sizeof(ehdr); i < bytes; i++) {
    ASSERT_EQ(0x34, buffer[i]) << "Failed at byte " << i;
  }

  bytes = mem->Read(0x1000, buffer.data(), buffer.size());
  ASSERT_EQ(0x1000UL, bytes);
  for (size_t i = 0; i < bytes; i++) {
    ASSERT_EQ(0x43, buffer[i]) << "Failed at byte " << i;
  }
}

TEST_F(MapInfoCreateMemoryTest, rosegment_from_file) {
  Maps maps;
  maps.Add(0x500, 0x600, 0, PROT_READ, "something_else", 0);
  maps.Add(0x1000, 0x2000, 0x1000, PROT_READ, elf_at_1000_.path, 0);
  maps.Add(0x2000, 0x3000, 0x2000, PROT_READ | PROT_EXEC, elf_at_1000_.path, 0);

  MapInfo* map_info = maps.Find(0x2000);
  ASSERT_TRUE(map_info != nullptr);

  // Set up the size
  Elf64_Ehdr ehdr;
  ASSERT_EQ(0x1000, lseek(elf_at_1000_.fd, 0x1000, SEEK_SET));
  ASSERT_TRUE(android::base::ReadFully(elf_at_1000_.fd, &ehdr, sizeof(ehdr)));

  // Will not give the elf memory, because the read-only entry does not
  // extend over the executable segment.
  std::unique_ptr<Memory> memory(map_info->CreateMemory(process_memory_));
  ASSERT_TRUE(memory.get() != nullptr);
  EXPECT_FALSE(map_info->memory_backed_elf);
  std::vector<uint8_t> buffer(0x100);
  EXPECT_EQ(0x2000U, map_info->offset);
  EXPECT_EQ(0U, map_info->elf_offset);
  EXPECT_EQ(0U, map_info->elf_start_offset);
  ASSERT_TRUE(memory->ReadFully(0, buffer.data(), 0x100));
  EXPECT_EQ(0xffU, buffer[0]);

  // Now init the elf data enough so that the file memory object will be used.
  ehdr.e_shoff = 0x4000;
  ehdr.e_shnum = 1;
  ehdr.e_shentsize = 0x100;
  ASSERT_EQ(0x1000, lseek(elf_at_1000_.fd, 0x1000, SEEK_SET));
  ASSERT_TRUE(android::base::WriteFully(elf_at_1000_.fd, &ehdr, sizeof(ehdr)));

  map_info->memory_backed_elf = false;
  memory.reset(map_info->CreateMemory(process_memory_));
  EXPECT_FALSE(map_info->memory_backed_elf);
  EXPECT_EQ(0x2000U, map_info->offset);
  EXPECT_EQ(0x1000U, map_info->elf_offset);
  EXPECT_EQ(0x1000U, map_info->elf_start_offset);
  Elf64_Ehdr ehdr_mem;
  ASSERT_TRUE(memory->ReadFully(0, &ehdr_mem, sizeof(ehdr_mem)));
  EXPECT_TRUE(memcmp(&ehdr, &ehdr_mem, sizeof(ehdr)) == 0);
}

}  // namespace unwindstack
