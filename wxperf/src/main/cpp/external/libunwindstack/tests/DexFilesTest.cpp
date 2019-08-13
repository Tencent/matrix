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
#include <string.h>

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <unwindstack/DexFiles.h>
#include <unwindstack/Elf.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>

#include "DexFileData.h"
#include "ElfFake.h"
#include "MemoryFake.h"

namespace unwindstack {

class DexFilesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    memory_ = new MemoryFake;
    process_memory_.reset(memory_);

    dex_files_.reset(new DexFiles(process_memory_));
    dex_files_->SetArch(ARCH_ARM);

    maps_.reset(
        new BufferMaps("1000-4000 ---s 00000000 00:00 0\n"
                       "4000-6000 r--s 00000000 00:00 0\n"
                       "6000-8000 -w-s 00000000 00:00 0\n"
                       "a000-c000 r-xp 00000000 00:00 0\n"
                       "c000-f000 rwxp 00000000 00:00 0\n"
                       "f000-11000 r-xp 00000000 00:00 0\n"
                       "100000-110000 rw-p 0000000 00:00 0\n"
                       "200000-210000 rw-p 0000000 00:00 0\n"
                       "300000-400000 rw-p 0000000 00:00 0\n"));
    ASSERT_TRUE(maps_->Parse());

    // Global variable in a section that is not readable/executable.
    MapInfo* map_info = maps_->Get(kMapGlobalNonReadableExectable);
    ASSERT_TRUE(map_info != nullptr);
    MemoryFake* memory = new MemoryFake;
    ElfFake* elf = new ElfFake(memory);
    elf->FakeSetValid(true);
    ElfInterfaceFake* interface = new ElfInterfaceFake(memory);
    elf->FakeSetInterface(interface);
    interface->FakeSetGlobalVariable("__dex_debug_descriptor", 0x800);
    map_info->elf.reset(elf);

    // Global variable not set by default.
    map_info = maps_->Get(kMapGlobalSetToZero);
    ASSERT_TRUE(map_info != nullptr);
    memory = new MemoryFake;
    elf = new ElfFake(memory);
    elf->FakeSetValid(true);
    interface = new ElfInterfaceFake(memory);
    elf->FakeSetInterface(interface);
    interface->FakeSetGlobalVariable("__dex_debug_descriptor", 0x800);
    map_info->elf.reset(elf);

    // Global variable set in this map.
    map_info = maps_->Get(kMapGlobal);
    ASSERT_TRUE(map_info != nullptr);
    memory = new MemoryFake;
    elf = new ElfFake(memory);
    elf->FakeSetValid(true);
    interface = new ElfInterfaceFake(memory);
    elf->FakeSetInterface(interface);
    interface->FakeSetGlobalVariable("__dex_debug_descriptor", 0x800);
    map_info->elf.reset(elf);
  }

  void WriteDescriptor32(uint64_t addr, uint32_t head);
  void WriteDescriptor64(uint64_t addr, uint64_t head);
  void WriteEntry32(uint64_t entry_addr, uint32_t next, uint32_t prev, uint32_t dex_file);
  void WriteEntry64(uint64_t entry_addr, uint64_t next, uint64_t prev, uint64_t dex_file);
  void WriteDex(uint64_t dex_file);

  static constexpr size_t kMapGlobalNonReadableExectable = 3;
  static constexpr size_t kMapGlobalSetToZero = 4;
  static constexpr size_t kMapGlobal = 5;
  static constexpr size_t kMapDexFileEntries = 7;
  static constexpr size_t kMapDexFiles = 8;

  std::shared_ptr<Memory> process_memory_;
  MemoryFake* memory_;
  std::unique_ptr<DexFiles> dex_files_;
  std::unique_ptr<BufferMaps> maps_;
};

void DexFilesTest::WriteDescriptor32(uint64_t addr, uint32_t head) {
  //   void* first_entry_
  memory_->SetData32(addr + 12, head);
}

void DexFilesTest::WriteDescriptor64(uint64_t addr, uint64_t head) {
  //   void* first_entry_
  memory_->SetData64(addr + 16, head);
}

void DexFilesTest::WriteEntry32(uint64_t entry_addr, uint32_t next, uint32_t prev,
                                uint32_t dex_file) {
  // Format of the 32 bit DEXFileEntry structure:
  //   uint32_t next
  memory_->SetData32(entry_addr, next);
  //   uint32_t prev
  memory_->SetData32(entry_addr + 4, prev);
  //   uint32_t dex_file
  memory_->SetData32(entry_addr + 8, dex_file);
}

void DexFilesTest::WriteEntry64(uint64_t entry_addr, uint64_t next, uint64_t prev,
                                uint64_t dex_file) {
  // Format of the 64 bit DEXFileEntry structure:
  //   uint64_t next
  memory_->SetData64(entry_addr, next);
  //   uint64_t prev
  memory_->SetData64(entry_addr + 8, prev);
  //   uint64_t dex_file
  memory_->SetData64(entry_addr + 16, dex_file);
}

void DexFilesTest::WriteDex(uint64_t dex_file) {
  memory_->SetMemory(dex_file, kDexData, sizeof(kDexData) * sizeof(uint32_t));
}

TEST_F(DexFilesTest, get_method_information_invalid) {
  std::string method_name = "nothing";
  uint64_t method_offset = 0x124;
  MapInfo* info = maps_->Get(kMapDexFileEntries);

  dex_files_->GetMethodInformation(maps_.get(), info, 0, &method_name, &method_offset);
  EXPECT_EQ("nothing", method_name);
  EXPECT_EQ(0x124U, method_offset);
}

TEST_F(DexFilesTest, get_method_information_32) {
  std::string method_name = "nothing";
  uint64_t method_offset = 0x124;
  MapInfo* info = maps_->Get(kMapDexFiles);

  WriteDescriptor32(0xf800, 0x200000);
  WriteEntry32(0x200000, 0, 0, 0x300000);
  WriteDex(0x300000);

  dex_files_->GetMethodInformation(maps_.get(), info, 0x300100, &method_name, &method_offset);
  EXPECT_EQ("Main.<init>", method_name);
  EXPECT_EQ(0U, method_offset);
}

TEST_F(DexFilesTest, get_method_information_64) {
  std::string method_name = "nothing";
  uint64_t method_offset = 0x124;
  MapInfo* info = maps_->Get(kMapDexFiles);

  dex_files_->SetArch(ARCH_ARM64);
  WriteDescriptor64(0xf800, 0x200000);
  WriteEntry64(0x200000, 0, 0, 0x301000);
  WriteDex(0x301000);

  dex_files_->GetMethodInformation(maps_.get(), info, 0x301102, &method_name, &method_offset);
  EXPECT_EQ("Main.<init>", method_name);
  EXPECT_EQ(2U, method_offset);
}

TEST_F(DexFilesTest, get_method_information_not_first_entry_32) {
  std::string method_name = "nothing";
  uint64_t method_offset = 0x124;
  MapInfo* info = maps_->Get(kMapDexFiles);

  WriteDescriptor32(0xf800, 0x200000);
  WriteEntry32(0x200000, 0x200100, 0, 0x100000);
  WriteEntry32(0x200100, 0, 0x200000, 0x300000);
  WriteDex(0x300000);

  dex_files_->GetMethodInformation(maps_.get(), info, 0x300104, &method_name, &method_offset);
  EXPECT_EQ("Main.<init>", method_name);
  EXPECT_EQ(4U, method_offset);
}

TEST_F(DexFilesTest, get_method_information_not_first_entry_64) {
  std::string method_name = "nothing";
  uint64_t method_offset = 0x124;
  MapInfo* info = maps_->Get(kMapDexFiles);

  dex_files_->SetArch(ARCH_ARM64);
  WriteDescriptor64(0xf800, 0x200000);
  WriteEntry64(0x200000, 0x200100, 0, 0x100000);
  WriteEntry64(0x200100, 0, 0x200000, 0x300000);
  WriteDex(0x300000);

  dex_files_->GetMethodInformation(maps_.get(), info, 0x300106, &method_name, &method_offset);
  EXPECT_EQ("Main.<init>", method_name);
  EXPECT_EQ(6U, method_offset);
}

TEST_F(DexFilesTest, get_method_information_cached) {
  std::string method_name = "nothing";
  uint64_t method_offset = 0x124;
  MapInfo* info = maps_->Get(kMapDexFiles);

  WriteDescriptor32(0xf800, 0x200000);
  WriteEntry32(0x200000, 0, 0, 0x300000);
  WriteDex(0x300000);

  dex_files_->GetMethodInformation(maps_.get(), info, 0x300100, &method_name, &method_offset);
  EXPECT_EQ("Main.<init>", method_name);
  EXPECT_EQ(0U, method_offset);

  // Clear all memory and make sure that data is acquired from the cache.
  memory_->Clear();
  dex_files_->GetMethodInformation(maps_.get(), info, 0x300100, &method_name, &method_offset);
  EXPECT_EQ("Main.<init>", method_name);
  EXPECT_EQ(0U, method_offset);
}

TEST_F(DexFilesTest, get_method_information_search_libs) {
  std::string method_name = "nothing";
  uint64_t method_offset = 0x124;
  MapInfo* info = maps_->Get(kMapDexFiles);

  WriteDescriptor32(0xf800, 0x200000);
  WriteEntry32(0x200000, 0x200100, 0, 0x100000);
  WriteEntry32(0x200100, 0, 0x200000, 0x300000);
  WriteDex(0x300000);

  // Only search a given named list of libs.
  std::vector<std::string> libs{"libart.so"};
  dex_files_.reset(new DexFiles(process_memory_, libs));
  dex_files_->SetArch(ARCH_ARM);

  dex_files_->GetMethodInformation(maps_.get(), info, 0x300104, &method_name, &method_offset);
  EXPECT_EQ("nothing", method_name);
  EXPECT_EQ(0x124U, method_offset);

  MapInfo* map_info = maps_->Get(kMapGlobal);
  map_info->name = "/system/lib/libart.so";
  dex_files_.reset(new DexFiles(process_memory_, libs));
  dex_files_->SetArch(ARCH_ARM);
  // Make sure that clearing out copy of the libs doesn't affect the
  // DexFiles object.
  libs.clear();

  dex_files_->GetMethodInformation(maps_.get(), info, 0x300104, &method_name, &method_offset);
  EXPECT_EQ("Main.<init>", method_name);
  EXPECT_EQ(4U, method_offset);
}

TEST_F(DexFilesTest, get_method_information_global_skip_zero_32) {
  std::string method_name = "nothing";
  uint64_t method_offset = 0x124;
  MapInfo* info = maps_->Get(kMapDexFiles);

  // First global variable found, but value is zero.
  WriteDescriptor32(0xc800, 0);

  WriteDescriptor32(0xf800, 0x200000);
  WriteEntry32(0x200000, 0, 0, 0x300000);
  WriteDex(0x300000);

  dex_files_->GetMethodInformation(maps_.get(), info, 0x300100, &method_name, &method_offset);
  EXPECT_EQ("Main.<init>", method_name);
  EXPECT_EQ(0U, method_offset);

  // Verify that second is ignored when first is set to non-zero
  dex_files_.reset(new DexFiles(process_memory_));
  dex_files_->SetArch(ARCH_ARM);
  method_name = "fail";
  method_offset = 0x123;
  WriteDescriptor32(0xc800, 0x100000);
  dex_files_->GetMethodInformation(maps_.get(), info, 0x300100, &method_name, &method_offset);
  EXPECT_EQ("fail", method_name);
  EXPECT_EQ(0x123U, method_offset);
}

TEST_F(DexFilesTest, get_method_information_global_skip_zero_64) {
  std::string method_name = "nothing";
  uint64_t method_offset = 0x124;
  MapInfo* info = maps_->Get(kMapDexFiles);

  // First global variable found, but value is zero.
  WriteDescriptor64(0xc800, 0);

  WriteDescriptor64(0xf800, 0x200000);
  WriteEntry64(0x200000, 0, 0, 0x300000);
  WriteDex(0x300000);

  dex_files_->SetArch(ARCH_ARM64);
  dex_files_->GetMethodInformation(maps_.get(), info, 0x300100, &method_name, &method_offset);
  EXPECT_EQ("Main.<init>", method_name);
  EXPECT_EQ(0U, method_offset);

  // Verify that second is ignored when first is set to non-zero
  dex_files_.reset(new DexFiles(process_memory_));
  dex_files_->SetArch(ARCH_ARM64);
  method_name = "fail";
  method_offset = 0x123;
  WriteDescriptor64(0xc800, 0x100000);
  dex_files_->GetMethodInformation(maps_.get(), info, 0x300100, &method_name, &method_offset);
  EXPECT_EQ("fail", method_name);
  EXPECT_EQ(0x123U, method_offset);
}

}  // namespace unwindstack
