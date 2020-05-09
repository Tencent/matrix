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

#include <unwindstack/Elf.h>
#include <unwindstack/JitDebug.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>

#include "ElfFake.h"
#include "MemoryFake.h"

namespace unwindstack {

class JitDebugTest : public ::testing::Test {
 protected:
  void CreateFakeElf(MapInfo* map_info) {
    MemoryFake* memory = new MemoryFake;
    ElfFake* elf = new ElfFake(memory);
    elf->FakeSetValid(true);
    ElfInterfaceFake* interface = new ElfInterfaceFake(memory);
    elf->FakeSetInterface(interface);
    interface->FakeSetGlobalVariable("__jit_debug_descriptor", 0x800);
    map_info->elf.reset(elf);
  }

  void Init(ArchEnum arch) {
    jit_debug_.reset(new JitDebug(process_memory_));
    jit_debug_->SetArch(arch);

    maps_.reset(
        new BufferMaps("1000-4000 ---s 00000000 00:00 0 /fake/elf1\n"
                       "4000-6000 r--s 00000000 00:00 0 /fake/elf1\n"
                       "6000-8000 -wxs 00000000 00:00 0 /fake/elf1\n"
                       "a000-c000 --xp 00000000 00:00 0 /fake/elf2\n"
                       "c000-f000 rw-p 00001000 00:00 0 /fake/elf2\n"
                       "f000-11000 r--p 00000000 00:00 0 /fake/elf3\n"
                       "11000-12000 rw-p 00001000 00:00 0 /fake/elf3\n"
                       "12000-14000 r--p 00000000 00:00 0 /fake/elf4\n"
                       "100000-110000 rw-p 0001000 00:00 0 /fake/elf4\n"
                       "200000-210000 rw-p 0002000 00:00 0 /fake/elf4\n"));
    ASSERT_TRUE(maps_->Parse());

    MapInfo* map_info = maps_->Get(3);
    ASSERT_TRUE(map_info != nullptr);
    CreateFakeElf(map_info);

    map_info = maps_->Get(5);
    ASSERT_TRUE(map_info != nullptr);
    CreateFakeElf(map_info);

    map_info = maps_->Get(7);
    ASSERT_TRUE(map_info != nullptr);
    CreateFakeElf(map_info);
  }

  void SetUp() override {
    memory_ = new MemoryFake;
    process_memory_.reset(memory_);

    Init(ARCH_ARM);
  }

  template <typename EhdrType, typename ShdrType>
  void CreateElf(uint64_t offset, uint8_t class_type, uint8_t machine_type, uint32_t pc,
                 uint32_t size) {
    EhdrType ehdr;
    memset(&ehdr, 0, sizeof(ehdr));
    uint64_t sh_offset = sizeof(ehdr);
    memcpy(ehdr.e_ident, ELFMAG, SELFMAG);
    ehdr.e_ident[EI_CLASS] = class_type;
    ehdr.e_machine = machine_type;
    ehdr.e_shstrndx = 1;
    ehdr.e_shoff = sh_offset;
    ehdr.e_shentsize = sizeof(ShdrType);
    ehdr.e_shnum = 3;
    memory_->SetMemory(offset, &ehdr, sizeof(ehdr));

    ShdrType shdr;
    memset(&shdr, 0, sizeof(shdr));
    shdr.sh_type = SHT_NULL;
    memory_->SetMemory(offset + sh_offset, &shdr, sizeof(shdr));

    sh_offset += sizeof(shdr);
    memset(&shdr, 0, sizeof(shdr));
    shdr.sh_type = SHT_STRTAB;
    shdr.sh_name = 1;
    shdr.sh_offset = 0x500;
    shdr.sh_size = 0x100;
    memory_->SetMemory(offset + sh_offset, &shdr, sizeof(shdr));
    memory_->SetMemory(offset + 0x500, ".debug_frame");

    sh_offset += sizeof(shdr);
    memset(&shdr, 0, sizeof(shdr));
    shdr.sh_type = SHT_PROGBITS;
    shdr.sh_name = 0;
    shdr.sh_addr = 0x600;
    shdr.sh_offset = 0x600;
    shdr.sh_size = 0x200;
    memory_->SetMemory(offset + sh_offset, &shdr, sizeof(shdr));

    // Now add a single cie/fde.
    uint64_t dwarf_offset = offset + 0x600;
    if (class_type == ELFCLASS32) {
      // CIE 32 information.
      memory_->SetData32(dwarf_offset, 0xfc);
      memory_->SetData32(dwarf_offset + 0x4, 0xffffffff);
      memory_->SetData8(dwarf_offset + 0x8, 1);
      memory_->SetData8(dwarf_offset + 0x9, '\0');
      memory_->SetData8(dwarf_offset + 0xa, 0x4);
      memory_->SetData8(dwarf_offset + 0xb, 0x4);
      memory_->SetData8(dwarf_offset + 0xc, 0x1);

      // FDE 32 information.
      memory_->SetData32(dwarf_offset + 0x100, 0xfc);
      memory_->SetData32(dwarf_offset + 0x104, 0);
      memory_->SetData32(dwarf_offset + 0x108, pc);
      memory_->SetData32(dwarf_offset + 0x10c, size);
    } else {
      // CIE 64 information.
      memory_->SetData32(dwarf_offset, 0xffffffff);
      memory_->SetData64(dwarf_offset + 4, 0xf4);
      memory_->SetData64(dwarf_offset + 0xc, 0xffffffffffffffffULL);
      memory_->SetData8(dwarf_offset + 0x14, 1);
      memory_->SetData8(dwarf_offset + 0x15, '\0');
      memory_->SetData8(dwarf_offset + 0x16, 0x4);
      memory_->SetData8(dwarf_offset + 0x17, 0x4);
      memory_->SetData8(dwarf_offset + 0x18, 0x1);

      // FDE 64 information.
      memory_->SetData32(dwarf_offset + 0x100, 0xffffffff);
      memory_->SetData64(dwarf_offset + 0x104, 0xf4);
      memory_->SetData64(dwarf_offset + 0x10c, 0);
      memory_->SetData64(dwarf_offset + 0x114, pc);
      memory_->SetData64(dwarf_offset + 0x11c, size);
    }
  }

  void WriteDescriptor32(uint64_t addr, uint32_t entry);
  void WriteDescriptor64(uint64_t addr, uint64_t entry);
  void WriteEntry32Pack(uint64_t addr, uint32_t prev, uint32_t next, uint32_t elf_addr,
                        uint64_t elf_size);
  void WriteEntry32Pad(uint64_t addr, uint32_t prev, uint32_t next, uint32_t elf_addr,
                       uint64_t elf_size);
  void WriteEntry64(uint64_t addr, uint64_t prev, uint64_t next, uint64_t elf_addr,
                    uint64_t elf_size);

  std::shared_ptr<Memory> process_memory_;
  MemoryFake* memory_;
  std::unique_ptr<JitDebug> jit_debug_;
  std::unique_ptr<BufferMaps> maps_;
};

void JitDebugTest::WriteDescriptor32(uint64_t addr, uint32_t entry) {
  // Format of the 32 bit JITDescriptor structure:
  //   uint32_t version
  memory_->SetData32(addr, 1);
  //   uint32_t action_flag
  memory_->SetData32(addr + 4, 0);
  //   uint32_t relevant_entry
  memory_->SetData32(addr + 8, 0);
  //   uint32_t first_entry
  memory_->SetData32(addr + 12, entry);
}

void JitDebugTest::WriteDescriptor64(uint64_t addr, uint64_t entry) {
  // Format of the 64 bit JITDescriptor structure:
  //   uint32_t version
  memory_->SetData32(addr, 1);
  //   uint32_t action_flag
  memory_->SetData32(addr + 4, 0);
  //   uint64_t relevant_entry
  memory_->SetData64(addr + 8, 0);
  //   uint64_t first_entry
  memory_->SetData64(addr + 16, entry);
}

void JitDebugTest::WriteEntry32Pack(uint64_t addr, uint32_t prev, uint32_t next, uint32_t elf_addr,
                                    uint64_t elf_size) {
  // Format of the 32 bit JITCodeEntry structure:
  //   uint32_t next
  memory_->SetData32(addr, next);
  //   uint32_t prev
  memory_->SetData32(addr + 4, prev);
  //   uint32_t symfile_addr
  memory_->SetData32(addr + 8, elf_addr);
  //   uint64_t symfile_size
  memory_->SetData64(addr + 12, elf_size);
}

void JitDebugTest::WriteEntry32Pad(uint64_t addr, uint32_t prev, uint32_t next, uint32_t elf_addr,
                                   uint64_t elf_size) {
  // Format of the 32 bit JITCodeEntry structure:
  //   uint32_t next
  memory_->SetData32(addr, next);
  //   uint32_t prev
  memory_->SetData32(addr + 4, prev);
  //   uint32_t symfile_addr
  memory_->SetData32(addr + 8, elf_addr);
  //   uint32_t pad
  memory_->SetData32(addr + 12, 0);
  //   uint64_t symfile_size
  memory_->SetData64(addr + 16, elf_size);
}

void JitDebugTest::WriteEntry64(uint64_t addr, uint64_t prev, uint64_t next, uint64_t elf_addr,
                                uint64_t elf_size) {
  // Format of the 64 bit JITCodeEntry structure:
  //   uint64_t next
  memory_->SetData64(addr, next);
  //   uint64_t prev
  memory_->SetData64(addr + 8, prev);
  //   uint64_t symfile_addr
  memory_->SetData64(addr + 16, elf_addr);
  //   uint64_t symfile_size
  memory_->SetData64(addr + 24, elf_size);
}

TEST_F(JitDebugTest, get_elf_invalid) {
  Elf* elf = jit_debug_->GetElf(maps_.get(), 0x1500);
  ASSERT_TRUE(elf == nullptr);
}

TEST_F(JitDebugTest, get_elf_no_global_variable) {
  maps_.reset(new BufferMaps(""));
  Elf* elf = jit_debug_->GetElf(maps_.get(), 0x1500);
  ASSERT_TRUE(elf == nullptr);
}

TEST_F(JitDebugTest, get_elf_no_valid_descriptor_in_memory) {
  CreateElf<Elf32_Ehdr, Elf32_Shdr>(0x4000, ELFCLASS32, EM_ARM, 0x1500, 0x200);

  Elf* elf = jit_debug_->GetElf(maps_.get(), 0x1500);
  ASSERT_TRUE(elf == nullptr);
}

TEST_F(JitDebugTest, get_elf_no_valid_code_entry) {
  CreateElf<Elf32_Ehdr, Elf32_Shdr>(0x4000, ELFCLASS32, EM_ARM, 0x1500, 0x200);

  WriteDescriptor32(0xf800, 0x200000);

  Elf* elf = jit_debug_->GetElf(maps_.get(), 0x1500);
  ASSERT_TRUE(elf == nullptr);
}

TEST_F(JitDebugTest, get_elf_invalid_descriptor_first_entry) {
  CreateElf<Elf32_Ehdr, Elf32_Shdr>(0x4000, ELFCLASS32, EM_ARM, 0x1500, 0x200);

  WriteDescriptor32(0xf800, 0);

  Elf* elf = jit_debug_->GetElf(maps_.get(), 0x1500);
  ASSERT_TRUE(elf == nullptr);
}

TEST_F(JitDebugTest, get_elf_invalid_descriptor_version) {
  CreateElf<Elf32_Ehdr, Elf32_Shdr>(0x4000, ELFCLASS32, EM_ARM, 0x1500, 0x200);

  WriteDescriptor32(0xf800, 0x20000);
  // Set the version to an invalid value.
  memory_->SetData32(0xf800, 2);

  Elf* elf = jit_debug_->GetElf(maps_.get(), 0x1500);
  ASSERT_TRUE(elf == nullptr);
}

TEST_F(JitDebugTest, get_elf_32) {
  CreateElf<Elf32_Ehdr, Elf32_Shdr>(0x4000, ELFCLASS32, EM_ARM, 0x1500, 0x200);

  WriteDescriptor32(0xf800, 0x200000);
  WriteEntry32Pad(0x200000, 0, 0, 0x4000, 0x1000);

  Elf* elf = jit_debug_->GetElf(maps_.get(), 0x1500);
  ASSERT_TRUE(elf != nullptr);

  // Clear the memory and verify all of the data is cached.
  memory_->Clear();
  Elf* elf2 = jit_debug_->GetElf(maps_.get(), 0x1500);
  ASSERT_TRUE(elf2 != nullptr);
  EXPECT_EQ(elf, elf2);
}

TEST_F(JitDebugTest, get_multiple_jit_debug_descriptors_valid) {
  CreateElf<Elf32_Ehdr, Elf32_Shdr>(0x4000, ELFCLASS32, EM_ARM, 0x1500, 0x200);
  CreateElf<Elf32_Ehdr, Elf32_Shdr>(0x5000, ELFCLASS32, EM_ARM, 0x2000, 0x300);

  WriteDescriptor32(0xf800, 0x200000);
  WriteEntry32Pad(0x200000, 0, 0, 0x4000, 0x1000);
  WriteDescriptor32(0x12800, 0x201000);
  WriteEntry32Pad(0x201000, 0, 0, 0x5000, 0x1000);

  ASSERT_TRUE(jit_debug_->GetElf(maps_.get(), 0x1500) != nullptr);
  ASSERT_TRUE(jit_debug_->GetElf(maps_.get(), 0x2000) == nullptr);

  // Now clear the descriptor entry for the first one.
  WriteDescriptor32(0xf800, 0);
  jit_debug_.reset(new JitDebug(process_memory_));
  jit_debug_->SetArch(ARCH_ARM);

  ASSERT_TRUE(jit_debug_->GetElf(maps_.get(), 0x1500) == nullptr);
  ASSERT_TRUE(jit_debug_->GetElf(maps_.get(), 0x2000) != nullptr);
}

TEST_F(JitDebugTest, get_elf_x86) {
  Init(ARCH_X86);

  CreateElf<Elf32_Ehdr, Elf32_Shdr>(0x4000, ELFCLASS32, EM_ARM, 0x1500, 0x200);

  WriteDescriptor32(0xf800, 0x200000);
  WriteEntry32Pack(0x200000, 0, 0, 0x4000, 0x1000);

  jit_debug_->SetArch(ARCH_X86);
  Elf* elf = jit_debug_->GetElf(maps_.get(), 0x1500);
  ASSERT_TRUE(elf != nullptr);

  // Clear the memory and verify all of the data is cached.
  memory_->Clear();
  Elf* elf2 = jit_debug_->GetElf(maps_.get(), 0x1500);
  ASSERT_TRUE(elf2 != nullptr);
  EXPECT_EQ(elf, elf2);
}

TEST_F(JitDebugTest, get_elf_64) {
  Init(ARCH_ARM64);

  CreateElf<Elf64_Ehdr, Elf64_Shdr>(0x4000, ELFCLASS64, EM_AARCH64, 0x1500, 0x200);

  WriteDescriptor64(0xf800, 0x200000);
  WriteEntry64(0x200000, 0, 0, 0x4000, 0x1000);

  Elf* elf = jit_debug_->GetElf(maps_.get(), 0x1500);
  ASSERT_TRUE(elf != nullptr);

  // Clear the memory and verify all of the data is cached.
  memory_->Clear();
  Elf* elf2 = jit_debug_->GetElf(maps_.get(), 0x1500);
  ASSERT_TRUE(elf2 != nullptr);
  EXPECT_EQ(elf, elf2);
}

TEST_F(JitDebugTest, get_elf_multiple_entries) {
  CreateElf<Elf32_Ehdr, Elf32_Shdr>(0x4000, ELFCLASS32, EM_ARM, 0x1500, 0x200);
  CreateElf<Elf32_Ehdr, Elf32_Shdr>(0x5000, ELFCLASS32, EM_ARM, 0x2300, 0x400);

  WriteDescriptor32(0xf800, 0x200000);
  WriteEntry32Pad(0x200000, 0, 0x200100, 0x4000, 0x1000);
  WriteEntry32Pad(0x200100, 0x200100, 0, 0x5000, 0x1000);

  Elf* elf_2 = jit_debug_->GetElf(maps_.get(), 0x2400);
  ASSERT_TRUE(elf_2 != nullptr);

  Elf* elf_1 = jit_debug_->GetElf(maps_.get(), 0x1600);
  ASSERT_TRUE(elf_1 != nullptr);

  // Clear the memory and verify all of the data is cached.
  memory_->Clear();
  EXPECT_EQ(elf_1, jit_debug_->GetElf(maps_.get(), 0x1500));
  EXPECT_EQ(elf_1, jit_debug_->GetElf(maps_.get(), 0x16ff));
  EXPECT_EQ(elf_2, jit_debug_->GetElf(maps_.get(), 0x2300));
  EXPECT_EQ(elf_2, jit_debug_->GetElf(maps_.get(), 0x26ff));
  EXPECT_EQ(nullptr, jit_debug_->GetElf(maps_.get(), 0x1700));
  EXPECT_EQ(nullptr, jit_debug_->GetElf(maps_.get(), 0x2700));
}

TEST_F(JitDebugTest, get_elf_search_libs) {
  CreateElf<Elf32_Ehdr, Elf32_Shdr>(0x4000, ELFCLASS32, EM_ARM, 0x1500, 0x200);

  WriteDescriptor32(0xf800, 0x200000);
  WriteEntry32Pad(0x200000, 0, 0, 0x4000, 0x1000);

  // Only search a given named list of libs.
  std::vector<std::string> libs{"libart.so"};
  jit_debug_.reset(new JitDebug(process_memory_, libs));
  jit_debug_->SetArch(ARCH_ARM);
  EXPECT_TRUE(jit_debug_->GetElf(maps_.get(), 0x1500) == nullptr);

  // Change the name of the map that includes the value and verify this works.
  MapInfo* map_info = maps_->Get(5);
  map_info->name = "/system/lib/libart.so";
  map_info = maps_->Get(6);
  map_info->name = "/system/lib/libart.so";
  jit_debug_.reset(new JitDebug(process_memory_, libs));
  // Make sure that clearing our copy of the libs doesn't affect the
  // JitDebug object.
  libs.clear();
  jit_debug_->SetArch(ARCH_ARM);
  EXPECT_TRUE(jit_debug_->GetElf(maps_.get(), 0x1500) != nullptr);
}

}  // namespace unwindstack
