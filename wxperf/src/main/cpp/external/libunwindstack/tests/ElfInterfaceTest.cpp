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

#include <memory>

#include <gtest/gtest.h>

#include <unwindstack/ElfInterface.h>

#include "DwarfEncoding.h"
#include "ElfInterfaceArm.h"

#include "ElfFake.h"
#include "MemoryFake.h"

#if !defined(PT_ARM_EXIDX)
#define PT_ARM_EXIDX 0x70000001
#endif

#if !defined(EM_AARCH64)
#define EM_AARCH64 183
#endif

namespace unwindstack {

class ElfInterfaceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    memory_.Clear();
  }

  void SetStringMemory(uint64_t offset, const char* string) {
    memory_.SetMemory(offset, string, strlen(string) + 1);
  }

  template <typename Ehdr, typename Phdr, typename Dyn, typename ElfInterfaceType>
  void SinglePtLoad();

  template <typename Ehdr, typename Phdr, typename Dyn, typename ElfInterfaceType>
  void MultipleExecutablePtLoads();

  template <typename Ehdr, typename Phdr, typename Dyn, typename ElfInterfaceType>
  void MultipleExecutablePtLoadsIncrementsNotSizeOfPhdr();

  template <typename Ehdr, typename Phdr, typename Dyn, typename ElfInterfaceType>
  void NonExecutablePtLoads();

  template <typename Ehdr, typename Phdr, typename Dyn, typename ElfInterfaceType>
  void ManyPhdrs();

  enum SonameTestEnum : uint8_t {
    SONAME_NORMAL,
    SONAME_DTNULL_AFTER,
    SONAME_DTSIZE_SMALL,
    SONAME_MISSING_MAP,
  };

  template <typename Ehdr, typename Phdr, typename Shdr, typename Dyn>
  void SonameInit(SonameTestEnum test_type = SONAME_NORMAL);

  template <typename ElfInterfaceType>
  void Soname();

  template <typename ElfInterfaceType>
  void SonameAfterDtNull();

  template <typename ElfInterfaceType>
  void SonameSize();

  template <typename ElfInterfaceType>
  void SonameMissingMap();

  template <typename ElfType>
  void InitHeadersEhFrameTest();

  template <typename ElfType>
  void InitHeadersDebugFrame();

  template <typename ElfType>
  void InitHeadersEhFrameFail();

  template <typename ElfType>
  void InitHeadersDebugFrameFail();

  template <typename Ehdr, typename Shdr, typename ElfInterfaceType>
  void InitSectionHeadersMalformed();

  template <typename Ehdr, typename Shdr, typename Sym, typename ElfInterfaceType>
  void InitSectionHeaders(uint64_t entry_size);

  template <typename Ehdr, typename Shdr, typename ElfInterfaceType>
  void InitSectionHeadersOffsets();

  template <typename Sym>
  void InitSym(uint64_t offset, uint32_t value, uint32_t size, uint32_t name_offset,
               uint64_t sym_offset, const char* name);

  MemoryFake memory_;
};

template <typename Sym>
void ElfInterfaceTest::InitSym(uint64_t offset, uint32_t value, uint32_t size, uint32_t name_offset,
                               uint64_t sym_offset, const char* name) {
  Sym sym;
  memset(&sym, 0, sizeof(sym));
  sym.st_info = STT_FUNC;
  sym.st_value = value;
  sym.st_size = size;
  sym.st_name = name_offset;
  sym.st_shndx = SHN_COMMON;

  memory_.SetMemory(offset, &sym, sizeof(sym));
  memory_.SetMemory(sym_offset + name_offset, name, strlen(name) + 1);
}

template <typename Ehdr, typename Phdr, typename Dyn, typename ElfInterfaceType>
void ElfInterfaceTest::SinglePtLoad() {
  std::unique_ptr<ElfInterface> elf(new ElfInterfaceType(&memory_));

  Ehdr ehdr;
  memset(&ehdr, 0, sizeof(ehdr));
  ehdr.e_phoff = 0x100;
  ehdr.e_phnum = 1;
  ehdr.e_phentsize = sizeof(Phdr);
  memory_.SetMemory(0, &ehdr, sizeof(ehdr));

  Phdr phdr;
  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_LOAD;
  phdr.p_vaddr = 0x2000;
  phdr.p_memsz = 0x10000;
  phdr.p_flags = PF_R | PF_X;
  phdr.p_align = 0x1000;
  memory_.SetMemory(0x100, &phdr, sizeof(phdr));

  uint64_t load_bias = 0;
  ASSERT_TRUE(elf->Init(&load_bias));
  EXPECT_EQ(0x2000U, load_bias);

  const std::unordered_map<uint64_t, LoadInfo>& pt_loads = elf->pt_loads();
  ASSERT_EQ(1U, pt_loads.size());
  LoadInfo load_data = pt_loads.at(0);
  ASSERT_EQ(0U, load_data.offset);
  ASSERT_EQ(0x2000U, load_data.table_offset);
  ASSERT_EQ(0x10000U, load_data.table_size);
}

TEST_F(ElfInterfaceTest, elf32_single_pt_load) {
  SinglePtLoad<Elf32_Ehdr, Elf32_Phdr, Elf32_Dyn, ElfInterface32>();
}

TEST_F(ElfInterfaceTest, elf64_single_pt_load) {
  SinglePtLoad<Elf64_Ehdr, Elf64_Phdr, Elf64_Dyn, ElfInterface64>();
}

template <typename Ehdr, typename Phdr, typename Dyn, typename ElfInterfaceType>
void ElfInterfaceTest::MultipleExecutablePtLoads() {
  std::unique_ptr<ElfInterface> elf(new ElfInterfaceType(&memory_));

  Ehdr ehdr;
  memset(&ehdr, 0, sizeof(ehdr));
  ehdr.e_phoff = 0x100;
  ehdr.e_phnum = 3;
  ehdr.e_phentsize = sizeof(Phdr);
  memory_.SetMemory(0, &ehdr, sizeof(ehdr));

  Phdr phdr;
  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_LOAD;
  phdr.p_vaddr = 0x2000;
  phdr.p_memsz = 0x10000;
  phdr.p_flags = PF_R | PF_X;
  phdr.p_align = 0x1000;
  memory_.SetMemory(0x100, &phdr, sizeof(phdr));

  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_LOAD;
  phdr.p_offset = 0x1000;
  phdr.p_vaddr = 0x2001;
  phdr.p_memsz = 0x10001;
  phdr.p_flags = PF_R | PF_X;
  phdr.p_align = 0x1001;
  memory_.SetMemory(0x100 + sizeof(phdr), &phdr, sizeof(phdr));

  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_LOAD;
  phdr.p_offset = 0x2000;
  phdr.p_vaddr = 0x2002;
  phdr.p_memsz = 0x10002;
  phdr.p_flags = PF_R | PF_X;
  phdr.p_align = 0x1002;
  memory_.SetMemory(0x100 + 2 * sizeof(phdr), &phdr, sizeof(phdr));

  uint64_t load_bias = 0;
  ASSERT_TRUE(elf->Init(&load_bias));
  EXPECT_EQ(0x2000U, load_bias);

  const std::unordered_map<uint64_t, LoadInfo>& pt_loads = elf->pt_loads();
  ASSERT_EQ(3U, pt_loads.size());

  LoadInfo load_data = pt_loads.at(0);
  ASSERT_EQ(0U, load_data.offset);
  ASSERT_EQ(0x2000U, load_data.table_offset);
  ASSERT_EQ(0x10000U, load_data.table_size);

  load_data = pt_loads.at(0x1000);
  ASSERT_EQ(0x1000U, load_data.offset);
  ASSERT_EQ(0x2001U, load_data.table_offset);
  ASSERT_EQ(0x10001U, load_data.table_size);

  load_data = pt_loads.at(0x2000);
  ASSERT_EQ(0x2000U, load_data.offset);
  ASSERT_EQ(0x2002U, load_data.table_offset);
  ASSERT_EQ(0x10002U, load_data.table_size);
}

TEST_F(ElfInterfaceTest, elf32_multiple_executable_pt_loads) {
  MultipleExecutablePtLoads<Elf32_Ehdr, Elf32_Phdr, Elf32_Dyn, ElfInterface32>();
}

TEST_F(ElfInterfaceTest, elf64_multiple_executable_pt_loads) {
  MultipleExecutablePtLoads<Elf64_Ehdr, Elf64_Phdr, Elf64_Dyn, ElfInterface64>();
}

template <typename Ehdr, typename Phdr, typename Dyn, typename ElfInterfaceType>
void ElfInterfaceTest::MultipleExecutablePtLoadsIncrementsNotSizeOfPhdr() {
  std::unique_ptr<ElfInterface> elf(new ElfInterfaceType(&memory_));

  Ehdr ehdr;
  memset(&ehdr, 0, sizeof(ehdr));
  ehdr.e_phoff = 0x100;
  ehdr.e_phnum = 3;
  ehdr.e_phentsize = sizeof(Phdr) + 100;
  memory_.SetMemory(0, &ehdr, sizeof(ehdr));

  Phdr phdr;
  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_LOAD;
  phdr.p_vaddr = 0x2000;
  phdr.p_memsz = 0x10000;
  phdr.p_flags = PF_R | PF_X;
  phdr.p_align = 0x1000;
  memory_.SetMemory(0x100, &phdr, sizeof(phdr));

  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_LOAD;
  phdr.p_offset = 0x1000;
  phdr.p_vaddr = 0x2001;
  phdr.p_memsz = 0x10001;
  phdr.p_flags = PF_R | PF_X;
  phdr.p_align = 0x1001;
  memory_.SetMemory(0x100 + sizeof(phdr) + 100, &phdr, sizeof(phdr));

  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_LOAD;
  phdr.p_offset = 0x2000;
  phdr.p_vaddr = 0x2002;
  phdr.p_memsz = 0x10002;
  phdr.p_flags = PF_R | PF_X;
  phdr.p_align = 0x1002;
  memory_.SetMemory(0x100 + 2 * (sizeof(phdr) + 100), &phdr, sizeof(phdr));

  uint64_t load_bias = 0;
  ASSERT_TRUE(elf->Init(&load_bias));
  EXPECT_EQ(0x2000U, load_bias);

  const std::unordered_map<uint64_t, LoadInfo>& pt_loads = elf->pt_loads();
  ASSERT_EQ(3U, pt_loads.size());

  LoadInfo load_data = pt_loads.at(0);
  ASSERT_EQ(0U, load_data.offset);
  ASSERT_EQ(0x2000U, load_data.table_offset);
  ASSERT_EQ(0x10000U, load_data.table_size);

  load_data = pt_loads.at(0x1000);
  ASSERT_EQ(0x1000U, load_data.offset);
  ASSERT_EQ(0x2001U, load_data.table_offset);
  ASSERT_EQ(0x10001U, load_data.table_size);

  load_data = pt_loads.at(0x2000);
  ASSERT_EQ(0x2000U, load_data.offset);
  ASSERT_EQ(0x2002U, load_data.table_offset);
  ASSERT_EQ(0x10002U, load_data.table_size);
}

TEST_F(ElfInterfaceTest, elf32_multiple_executable_pt_loads_increments_not_size_of_phdr) {
  MultipleExecutablePtLoadsIncrementsNotSizeOfPhdr<Elf32_Ehdr, Elf32_Phdr, Elf32_Dyn,
                                                   ElfInterface32>();
}

TEST_F(ElfInterfaceTest, elf64_multiple_executable_pt_loads_increments_not_size_of_phdr) {
  MultipleExecutablePtLoadsIncrementsNotSizeOfPhdr<Elf64_Ehdr, Elf64_Phdr, Elf64_Dyn,
                                                   ElfInterface64>();
}

template <typename Ehdr, typename Phdr, typename Dyn, typename ElfInterfaceType>
void ElfInterfaceTest::NonExecutablePtLoads() {
  std::unique_ptr<ElfInterface> elf(new ElfInterfaceType(&memory_));

  Ehdr ehdr;
  memset(&ehdr, 0, sizeof(ehdr));
  ehdr.e_phoff = 0x100;
  ehdr.e_phnum = 3;
  ehdr.e_phentsize = sizeof(Phdr);
  memory_.SetMemory(0, &ehdr, sizeof(ehdr));

  Phdr phdr;
  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_LOAD;
  phdr.p_vaddr = 0x2000;
  phdr.p_memsz = 0x10000;
  phdr.p_flags = PF_R;
  phdr.p_align = 0x1000;
  memory_.SetMemory(0x100, &phdr, sizeof(phdr));

  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_LOAD;
  phdr.p_offset = 0x1000;
  phdr.p_vaddr = 0x2001;
  phdr.p_memsz = 0x10001;
  phdr.p_flags = PF_R | PF_X;
  phdr.p_align = 0x1001;
  memory_.SetMemory(0x100 + sizeof(phdr), &phdr, sizeof(phdr));

  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_LOAD;
  phdr.p_offset = 0x2000;
  phdr.p_vaddr = 0x2002;
  phdr.p_memsz = 0x10002;
  phdr.p_flags = PF_R;
  phdr.p_align = 0x1002;
  memory_.SetMemory(0x100 + 2 * sizeof(phdr), &phdr, sizeof(phdr));

  uint64_t load_bias = 0;
  ASSERT_TRUE(elf->Init(&load_bias));
  EXPECT_EQ(0U, load_bias);

  const std::unordered_map<uint64_t, LoadInfo>& pt_loads = elf->pt_loads();
  ASSERT_EQ(1U, pt_loads.size());

  LoadInfo load_data = pt_loads.at(0x1000);
  ASSERT_EQ(0x1000U, load_data.offset);
  ASSERT_EQ(0x2001U, load_data.table_offset);
  ASSERT_EQ(0x10001U, load_data.table_size);
}

TEST_F(ElfInterfaceTest, elf32_non_executable_pt_loads) {
  NonExecutablePtLoads<Elf32_Ehdr, Elf32_Phdr, Elf32_Dyn, ElfInterface32>();
}

TEST_F(ElfInterfaceTest, elf64_non_executable_pt_loads) {
  NonExecutablePtLoads<Elf64_Ehdr, Elf64_Phdr, Elf64_Dyn, ElfInterface64>();
}

template <typename Ehdr, typename Phdr, typename Dyn, typename ElfInterfaceType>
void ElfInterfaceTest::ManyPhdrs() {
  std::unique_ptr<ElfInterface> elf(new ElfInterfaceType(&memory_));

  Ehdr ehdr;
  memset(&ehdr, 0, sizeof(ehdr));
  ehdr.e_phoff = 0x100;
  ehdr.e_phnum = 7;
  ehdr.e_phentsize = sizeof(Phdr);
  memory_.SetMemory(0, &ehdr, sizeof(ehdr));

  Phdr phdr;
  uint64_t phdr_offset = 0x100;

  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_LOAD;
  phdr.p_vaddr = 0x2000;
  phdr.p_memsz = 0x10000;
  phdr.p_flags = PF_R | PF_X;
  phdr.p_align = 0x1000;
  memory_.SetMemory(phdr_offset, &phdr, sizeof(phdr));
  phdr_offset += sizeof(phdr);

  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_GNU_EH_FRAME;
  memory_.SetMemory(phdr_offset, &phdr, sizeof(phdr));
  phdr_offset += sizeof(phdr);

  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_DYNAMIC;
  memory_.SetMemory(phdr_offset, &phdr, sizeof(phdr));
  phdr_offset += sizeof(phdr);

  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_INTERP;
  memory_.SetMemory(phdr_offset, &phdr, sizeof(phdr));
  phdr_offset += sizeof(phdr);

  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_NOTE;
  memory_.SetMemory(phdr_offset, &phdr, sizeof(phdr));
  phdr_offset += sizeof(phdr);

  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_SHLIB;
  memory_.SetMemory(phdr_offset, &phdr, sizeof(phdr));
  phdr_offset += sizeof(phdr);

  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_GNU_EH_FRAME;
  memory_.SetMemory(phdr_offset, &phdr, sizeof(phdr));
  phdr_offset += sizeof(phdr);

  uint64_t load_bias = 0;
  ASSERT_TRUE(elf->Init(&load_bias));
  EXPECT_EQ(0x2000U, load_bias);

  const std::unordered_map<uint64_t, LoadInfo>& pt_loads = elf->pt_loads();
  ASSERT_EQ(1U, pt_loads.size());

  LoadInfo load_data = pt_loads.at(0);
  ASSERT_EQ(0U, load_data.offset);
  ASSERT_EQ(0x2000U, load_data.table_offset);
  ASSERT_EQ(0x10000U, load_data.table_size);
}

TEST_F(ElfInterfaceTest, elf32_many_phdrs) {
  ElfInterfaceTest::ManyPhdrs<Elf32_Ehdr, Elf32_Phdr, Elf32_Dyn, ElfInterface32>();
}

TEST_F(ElfInterfaceTest, elf64_many_phdrs) {
  ElfInterfaceTest::ManyPhdrs<Elf64_Ehdr, Elf64_Phdr, Elf64_Dyn, ElfInterface64>();
}

TEST_F(ElfInterfaceTest, elf32_arm) {
  ElfInterfaceArm elf_arm(&memory_);

  Elf32_Ehdr ehdr;
  memset(&ehdr, 0, sizeof(ehdr));
  ehdr.e_phoff = 0x100;
  ehdr.e_phnum = 1;
  ehdr.e_phentsize = sizeof(Elf32_Phdr);
  memory_.SetMemory(0, &ehdr, sizeof(ehdr));

  Elf32_Phdr phdr;
  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_ARM_EXIDX;
  phdr.p_vaddr = 0x2000;
  phdr.p_memsz = 16;
  memory_.SetMemory(0x100, &phdr, sizeof(phdr));

  // Add arm exidx entries.
  memory_.SetData32(0x2000, 0x1000);
  memory_.SetData32(0x2008, 0x1000);

  uint64_t load_bias = 0;
  ASSERT_TRUE(elf_arm.Init(&load_bias));
  EXPECT_EQ(0U, load_bias);

  std::vector<uint32_t> entries;
  for (auto addr : elf_arm) {
    entries.push_back(addr);
  }
  ASSERT_EQ(2U, entries.size());
  ASSERT_EQ(0x3000U, entries[0]);
  ASSERT_EQ(0x3008U, entries[1]);

  ASSERT_EQ(0x2000U, elf_arm.start_offset());
  ASSERT_EQ(2U, elf_arm.total_entries());
}

template <typename Ehdr, typename Phdr, typename Shdr, typename Dyn>
void ElfInterfaceTest::SonameInit(SonameTestEnum test_type) {
  Ehdr ehdr;
  memset(&ehdr, 0, sizeof(ehdr));
  ehdr.e_shoff = 0x200;
  ehdr.e_shnum = 2;
  ehdr.e_shentsize = sizeof(Shdr);
  ehdr.e_phoff = 0x100;
  ehdr.e_phnum = 1;
  ehdr.e_phentsize = sizeof(Phdr);
  memory_.SetMemory(0, &ehdr, sizeof(ehdr));

  Shdr shdr;
  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_STRTAB;
  if (test_type == SONAME_MISSING_MAP) {
    shdr.sh_addr = 0x20100;
  } else {
    shdr.sh_addr = 0x10100;
  }
  shdr.sh_offset = 0x10000;
  memory_.SetMemory(0x200 + sizeof(shdr), &shdr, sizeof(shdr));

  Phdr phdr;
  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_DYNAMIC;
  phdr.p_offset = 0x2000;
  phdr.p_memsz = sizeof(Dyn) * 3;
  memory_.SetMemory(0x100, &phdr, sizeof(phdr));

  uint64_t offset = 0x2000;
  Dyn dyn;

  dyn.d_tag = DT_STRTAB;
  dyn.d_un.d_ptr = 0x10100;
  memory_.SetMemory(offset, &dyn, sizeof(dyn));
  offset += sizeof(dyn);

  dyn.d_tag = DT_STRSZ;
  if (test_type == SONAME_DTSIZE_SMALL) {
    dyn.d_un.d_val = 0x10;
  } else {
    dyn.d_un.d_val = 0x1000;
  }
  memory_.SetMemory(offset, &dyn, sizeof(dyn));
  offset += sizeof(dyn);

  if (test_type == SONAME_DTNULL_AFTER) {
    dyn.d_tag = DT_NULL;
    memory_.SetMemory(offset, &dyn, sizeof(dyn));
    offset += sizeof(dyn);
  }

  dyn.d_tag = DT_SONAME;
  dyn.d_un.d_val = 0x10;
  memory_.SetMemory(offset, &dyn, sizeof(dyn));
  offset += sizeof(dyn);

  dyn.d_tag = DT_NULL;
  memory_.SetMemory(offset, &dyn, sizeof(dyn));

  SetStringMemory(0x10010, "fake_soname.so");
}

template <typename ElfInterfaceType>
void ElfInterfaceTest::Soname() {
  std::unique_ptr<ElfInterface> elf(new ElfInterfaceType(&memory_));

  uint64_t load_bias = 0;
  ASSERT_TRUE(elf->Init(&load_bias));
  EXPECT_EQ(0U, load_bias);

  std::string name;
  ASSERT_TRUE(elf->GetSoname(&name));
  ASSERT_STREQ("fake_soname.so", name.c_str());
}

TEST_F(ElfInterfaceTest, elf32_soname) {
  SonameInit<Elf32_Ehdr, Elf32_Phdr, Elf32_Shdr, Elf32_Dyn>();
  Soname<ElfInterface32>();
}

TEST_F(ElfInterfaceTest, elf64_soname) {
  SonameInit<Elf64_Ehdr, Elf64_Phdr, Elf64_Shdr, Elf64_Dyn>();
  Soname<ElfInterface64>();
}

template <typename ElfInterfaceType>
void ElfInterfaceTest::SonameAfterDtNull() {
  std::unique_ptr<ElfInterface> elf(new ElfInterfaceType(&memory_));

  uint64_t load_bias = 0;
  ASSERT_TRUE(elf->Init(&load_bias));
  EXPECT_EQ(0U, load_bias);

  std::string name;
  ASSERT_FALSE(elf->GetSoname(&name));
}

TEST_F(ElfInterfaceTest, elf32_soname_after_dt_null) {
  SonameInit<Elf32_Ehdr, Elf32_Phdr, Elf32_Shdr, Elf32_Dyn>(SONAME_DTNULL_AFTER);
  SonameAfterDtNull<ElfInterface32>();
}

TEST_F(ElfInterfaceTest, elf64_soname_after_dt_null) {
  SonameInit<Elf64_Ehdr, Elf64_Phdr, Elf64_Shdr, Elf64_Dyn>(SONAME_DTNULL_AFTER);
  SonameAfterDtNull<ElfInterface64>();
}

template <typename ElfInterfaceType>
void ElfInterfaceTest::SonameSize() {
  std::unique_ptr<ElfInterface> elf(new ElfInterfaceType(&memory_));

  uint64_t load_bias = 0;
  ASSERT_TRUE(elf->Init(&load_bias));
  EXPECT_EQ(0U, load_bias);

  std::string name;
  ASSERT_FALSE(elf->GetSoname(&name));
}

TEST_F(ElfInterfaceTest, elf32_soname_size) {
  SonameInit<Elf32_Ehdr, Elf32_Phdr, Elf32_Shdr, Elf32_Dyn>(SONAME_DTSIZE_SMALL);
  SonameSize<ElfInterface32>();
}

TEST_F(ElfInterfaceTest, elf64_soname_size) {
  SonameInit<Elf64_Ehdr, Elf64_Phdr, Elf64_Shdr, Elf64_Dyn>(SONAME_DTSIZE_SMALL);
  SonameSize<ElfInterface64>();
}

// Verify that there is no map from STRTAB in the dynamic section to a
// STRTAB entry in the section headers.
template <typename ElfInterfaceType>
void ElfInterfaceTest::SonameMissingMap() {
  std::unique_ptr<ElfInterface> elf(new ElfInterfaceType(&memory_));

  uint64_t load_bias = 0;
  ASSERT_TRUE(elf->Init(&load_bias));
  EXPECT_EQ(0U, load_bias);

  std::string name;
  ASSERT_FALSE(elf->GetSoname(&name));
}

TEST_F(ElfInterfaceTest, elf32_soname_missing_map) {
  SonameInit<Elf32_Ehdr, Elf32_Phdr, Elf32_Shdr, Elf32_Dyn>(SONAME_MISSING_MAP);
  SonameMissingMap<ElfInterface32>();
}

TEST_F(ElfInterfaceTest, elf64_soname_missing_map) {
  SonameInit<Elf64_Ehdr, Elf64_Phdr, Elf64_Shdr, Elf64_Dyn>(SONAME_MISSING_MAP);
  SonameMissingMap<ElfInterface64>();
}

template <typename ElfType>
void ElfInterfaceTest::InitHeadersEhFrameTest() {
  ElfType elf(&memory_);

  elf.FakeSetEhFrameOffset(0x10000);
  elf.FakeSetEhFrameSize(0);
  elf.FakeSetDebugFrameOffset(0);
  elf.FakeSetDebugFrameSize(0);

  memory_.SetMemory(0x10000,
                    std::vector<uint8_t>{0x1, DW_EH_PE_udata2, DW_EH_PE_udata2, DW_EH_PE_udata2});
  memory_.SetData32(0x10004, 0x500);
  memory_.SetData32(0x10008, 250);

  elf.InitHeaders();

  EXPECT_FALSE(elf.eh_frame() == nullptr);
  EXPECT_TRUE(elf.debug_frame() == nullptr);
}

TEST_F(ElfInterfaceTest, init_headers_eh_frame32) {
  InitHeadersEhFrameTest<ElfInterface32Fake>();
}

TEST_F(ElfInterfaceTest, init_headers_eh_frame64) {
  InitHeadersEhFrameTest<ElfInterface64Fake>();
}

template <typename ElfType>
void ElfInterfaceTest::InitHeadersDebugFrame() {
  ElfType elf(&memory_);

  elf.FakeSetEhFrameOffset(0);
  elf.FakeSetEhFrameSize(0);
  elf.FakeSetDebugFrameOffset(0x5000);
  elf.FakeSetDebugFrameSize(0x200);

  memory_.SetData32(0x5000, 0xfc);
  memory_.SetData32(0x5004, 0xffffffff);
  memory_.SetData8(0x5008, 1);
  memory_.SetData8(0x5009, '\0');

  memory_.SetData32(0x5100, 0xfc);
  memory_.SetData32(0x5104, 0);
  memory_.SetData32(0x5108, 0x1500);
  memory_.SetData32(0x510c, 0x200);

  elf.InitHeaders();

  EXPECT_TRUE(elf.eh_frame() == nullptr);
  EXPECT_FALSE(elf.debug_frame() == nullptr);
}

TEST_F(ElfInterfaceTest, init_headers_debug_frame32) {
  InitHeadersDebugFrame<ElfInterface32Fake>();
}

TEST_F(ElfInterfaceTest, init_headers_debug_frame64) {
  InitHeadersDebugFrame<ElfInterface64Fake>();
}

template <typename ElfType>
void ElfInterfaceTest::InitHeadersEhFrameFail() {
  ElfType elf(&memory_);

  elf.FakeSetEhFrameOffset(0x1000);
  elf.FakeSetEhFrameSize(0x100);
  elf.FakeSetDebugFrameOffset(0);
  elf.FakeSetDebugFrameSize(0);

  elf.InitHeaders();

  EXPECT_TRUE(elf.eh_frame() == nullptr);
  EXPECT_EQ(0U, elf.eh_frame_offset());
  EXPECT_EQ(static_cast<uint64_t>(-1), elf.eh_frame_size());
  EXPECT_TRUE(elf.debug_frame() == nullptr);
}

TEST_F(ElfInterfaceTest, init_headers_eh_frame32_fail) {
  InitHeadersEhFrameFail<ElfInterface32Fake>();
}

TEST_F(ElfInterfaceTest, init_headers_eh_frame64_fail) {
  InitHeadersEhFrameFail<ElfInterface64Fake>();
}

template <typename ElfType>
void ElfInterfaceTest::InitHeadersDebugFrameFail() {
  ElfType elf(&memory_);

  elf.FakeSetEhFrameOffset(0);
  elf.FakeSetEhFrameSize(0);
  elf.FakeSetDebugFrameOffset(0x1000);
  elf.FakeSetDebugFrameSize(0x100);

  elf.InitHeaders();

  EXPECT_TRUE(elf.eh_frame() == nullptr);
  EXPECT_TRUE(elf.debug_frame() == nullptr);
  EXPECT_EQ(0U, elf.debug_frame_offset());
  EXPECT_EQ(static_cast<uint64_t>(-1), elf.debug_frame_size());
}

TEST_F(ElfInterfaceTest, init_headers_debug_frame32_fail) {
  InitHeadersDebugFrameFail<ElfInterface32Fake>();
}

TEST_F(ElfInterfaceTest, init_headers_debug_frame64_fail) {
  InitHeadersDebugFrameFail<ElfInterface64Fake>();
}

template <typename Ehdr, typename Shdr, typename ElfInterfaceType>
void ElfInterfaceTest::InitSectionHeadersMalformed() {
  std::unique_ptr<ElfInterfaceType> elf(new ElfInterfaceType(&memory_));

  Ehdr ehdr;
  memset(&ehdr, 0, sizeof(ehdr));
  ehdr.e_shoff = 0x1000;
  ehdr.e_shnum = 10;
  ehdr.e_shentsize = sizeof(Shdr);
  memory_.SetMemory(0, &ehdr, sizeof(ehdr));

  uint64_t load_bias = 0;
  ASSERT_TRUE(elf->Init(&load_bias));
  EXPECT_EQ(0U, load_bias);
}

TEST_F(ElfInterfaceTest, init_section_headers_malformed32) {
  InitSectionHeadersMalformed<Elf32_Ehdr, Elf32_Shdr, ElfInterface32>();
}

TEST_F(ElfInterfaceTest, init_section_headers_malformed64) {
  InitSectionHeadersMalformed<Elf64_Ehdr, Elf64_Shdr, ElfInterface64>();
}

template <typename Ehdr, typename Shdr, typename Sym, typename ElfInterfaceType>
void ElfInterfaceTest::InitSectionHeaders(uint64_t entry_size) {
  std::unique_ptr<ElfInterfaceType> elf(new ElfInterfaceType(&memory_));

  uint64_t offset = 0x1000;

  Ehdr ehdr;
  memset(&ehdr, 0, sizeof(ehdr));
  ehdr.e_shoff = offset;
  ehdr.e_shnum = 10;
  ehdr.e_shentsize = entry_size;
  memory_.SetMemory(0, &ehdr, sizeof(ehdr));

  offset += ehdr.e_shentsize;

  Shdr shdr;
  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_SYMTAB;
  shdr.sh_link = 4;
  shdr.sh_addr = 0x5000;
  shdr.sh_offset = 0x5000;
  shdr.sh_entsize = sizeof(Sym);
  shdr.sh_size = shdr.sh_entsize * 10;
  memory_.SetMemory(offset, &shdr, sizeof(shdr));
  offset += ehdr.e_shentsize;

  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_DYNSYM;
  shdr.sh_link = 4;
  shdr.sh_addr = 0x6000;
  shdr.sh_offset = 0x6000;
  shdr.sh_entsize = sizeof(Sym);
  shdr.sh_size = shdr.sh_entsize * 10;
  memory_.SetMemory(offset, &shdr, sizeof(shdr));
  offset += ehdr.e_shentsize;

  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_PROGBITS;
  shdr.sh_name = 0xa000;
  memory_.SetMemory(offset, &shdr, sizeof(shdr));
  offset += ehdr.e_shentsize;

  // The string data for the entries.
  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_STRTAB;
  shdr.sh_name = 0x20000;
  shdr.sh_offset = 0xf000;
  shdr.sh_size = 0x1000;
  memory_.SetMemory(offset, &shdr, sizeof(shdr));
  offset += ehdr.e_shentsize;

  InitSym<Sym>(0x5000, 0x90000, 0x1000, 0x100, 0xf000, "function_one");
  InitSym<Sym>(0x6000, 0xd0000, 0x1000, 0x300, 0xf000, "function_two");

  uint64_t load_bias = 0;
  ASSERT_TRUE(elf->Init(&load_bias));
  EXPECT_EQ(0U, load_bias);
  EXPECT_EQ(0U, elf->debug_frame_offset());
  EXPECT_EQ(0U, elf->debug_frame_size());
  EXPECT_EQ(0U, elf->gnu_debugdata_offset());
  EXPECT_EQ(0U, elf->gnu_debugdata_size());

  // Look in the first symbol table.
  std::string name;
  uint64_t name_offset;
  ASSERT_TRUE(elf->GetFunctionName(0x90010, 0, &name, &name_offset));
  EXPECT_EQ("function_one", name);
  EXPECT_EQ(16U, name_offset);
  ASSERT_TRUE(elf->GetFunctionName(0xd0020, 0, &name, &name_offset));
  EXPECT_EQ("function_two", name);
  EXPECT_EQ(32U, name_offset);
}

TEST_F(ElfInterfaceTest, init_section_headers32) {
  InitSectionHeaders<Elf32_Ehdr, Elf32_Shdr, Elf32_Sym, ElfInterface32>(sizeof(Elf32_Shdr));
}

TEST_F(ElfInterfaceTest, init_section_headers64) {
  InitSectionHeaders<Elf64_Ehdr, Elf64_Shdr, Elf64_Sym, ElfInterface64>(sizeof(Elf64_Shdr));
}

TEST_F(ElfInterfaceTest, init_section_headers_non_std_entry_size32) {
  InitSectionHeaders<Elf32_Ehdr, Elf32_Shdr, Elf32_Sym, ElfInterface32>(0x100);
}

TEST_F(ElfInterfaceTest, init_section_headers_non_std_entry_size64) {
  InitSectionHeaders<Elf64_Ehdr, Elf64_Shdr, Elf64_Sym, ElfInterface64>(0x100);
}

template <typename Ehdr, typename Shdr, typename ElfInterfaceType>
void ElfInterfaceTest::InitSectionHeadersOffsets() {
  std::unique_ptr<ElfInterfaceType> elf(new ElfInterfaceType(&memory_));

  uint64_t offset = 0x2000;

  Ehdr ehdr;
  memset(&ehdr, 0, sizeof(ehdr));
  ehdr.e_shoff = offset;
  ehdr.e_shnum = 10;
  ehdr.e_shentsize = sizeof(Shdr);
  ehdr.e_shstrndx = 2;
  memory_.SetMemory(0, &ehdr, sizeof(ehdr));

  offset += ehdr.e_shentsize;

  Shdr shdr;
  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_PROGBITS;
  shdr.sh_link = 2;
  shdr.sh_name = 0x200;
  shdr.sh_addr = 0x5000;
  shdr.sh_offset = 0x5000;
  shdr.sh_entsize = 0x100;
  shdr.sh_size = 0x800;
  memory_.SetMemory(offset, &shdr, sizeof(shdr));
  offset += ehdr.e_shentsize;

  // The string data for section header names.
  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_STRTAB;
  shdr.sh_name = 0x20000;
  shdr.sh_offset = 0xf000;
  shdr.sh_size = 0x1000;
  memory_.SetMemory(offset, &shdr, sizeof(shdr));
  offset += ehdr.e_shentsize;

  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_PROGBITS;
  shdr.sh_link = 2;
  shdr.sh_name = 0x100;
  shdr.sh_addr = 0x6000;
  shdr.sh_offset = 0x6000;
  shdr.sh_entsize = 0x100;
  shdr.sh_size = 0x500;
  memory_.SetMemory(offset, &shdr, sizeof(shdr));
  offset += ehdr.e_shentsize;

  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_PROGBITS;
  shdr.sh_link = 2;
  shdr.sh_name = 0x300;
  shdr.sh_addr = 0x7000;
  shdr.sh_offset = 0x7000;
  shdr.sh_entsize = 0x100;
  shdr.sh_size = 0x800;
  memory_.SetMemory(offset, &shdr, sizeof(shdr));
  offset += ehdr.e_shentsize;

  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_PROGBITS;
  shdr.sh_link = 2;
  shdr.sh_name = 0x400;
  shdr.sh_addr = 0x6000;
  shdr.sh_offset = 0xa000;
  shdr.sh_entsize = 0x100;
  shdr.sh_size = 0xf00;
  memory_.SetMemory(offset, &shdr, sizeof(shdr));
  offset += ehdr.e_shentsize;

  memory_.SetMemory(0xf100, ".debug_frame", sizeof(".debug_frame"));
  memory_.SetMemory(0xf200, ".gnu_debugdata", sizeof(".gnu_debugdata"));
  memory_.SetMemory(0xf300, ".eh_frame", sizeof(".eh_frame"));
  memory_.SetMemory(0xf400, ".eh_frame_hdr", sizeof(".eh_frame_hdr"));

  uint64_t load_bias = 0;
  ASSERT_TRUE(elf->Init(&load_bias));
  EXPECT_EQ(0U, load_bias);
  EXPECT_EQ(0x6000U, elf->debug_frame_offset());
  EXPECT_EQ(0x500U, elf->debug_frame_size());
  EXPECT_EQ(0x5000U, elf->gnu_debugdata_offset());
  EXPECT_EQ(0x800U, elf->gnu_debugdata_size());
  EXPECT_EQ(0x7000U, elf->eh_frame_offset());
  EXPECT_EQ(0x800U, elf->eh_frame_size());
  EXPECT_EQ(0xa000U, elf->eh_frame_hdr_offset());
  EXPECT_EQ(0xf00U, elf->eh_frame_hdr_size());
}

TEST_F(ElfInterfaceTest, init_section_headers_offsets32) {
  InitSectionHeadersOffsets<Elf32_Ehdr, Elf32_Shdr, ElfInterface32>();
}

TEST_F(ElfInterfaceTest, init_section_headers_offsets64) {
  InitSectionHeadersOffsets<Elf64_Ehdr, Elf64_Shdr, ElfInterface64>();
}

TEST_F(ElfInterfaceTest, is_valid_pc_from_pt_load) {
  std::unique_ptr<ElfInterface> elf(new ElfInterface32(&memory_));

  Elf32_Ehdr ehdr;
  memset(&ehdr, 0, sizeof(ehdr));
  ehdr.e_phoff = 0x100;
  ehdr.e_phnum = 1;
  ehdr.e_phentsize = sizeof(Elf32_Phdr);
  memory_.SetMemory(0, &ehdr, sizeof(ehdr));

  Elf32_Phdr phdr;
  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_LOAD;
  phdr.p_vaddr = 0;
  phdr.p_memsz = 0x10000;
  phdr.p_flags = PF_R | PF_X;
  phdr.p_align = 0x1000;
  memory_.SetMemory(0x100, &phdr, sizeof(phdr));

  uint64_t load_bias = 0;
  ASSERT_TRUE(elf->Init(&load_bias));
  EXPECT_EQ(0U, load_bias);
  EXPECT_TRUE(elf->IsValidPc(0));
  EXPECT_TRUE(elf->IsValidPc(0x5000));
  EXPECT_TRUE(elf->IsValidPc(0xffff));
  EXPECT_FALSE(elf->IsValidPc(0x10000));
}

TEST_F(ElfInterfaceTest, is_valid_pc_from_pt_load_non_zero_load_bias) {
  std::unique_ptr<ElfInterface> elf(new ElfInterface32(&memory_));

  Elf32_Ehdr ehdr;
  memset(&ehdr, 0, sizeof(ehdr));
  ehdr.e_phoff = 0x100;
  ehdr.e_phnum = 1;
  ehdr.e_phentsize = sizeof(Elf32_Phdr);
  memory_.SetMemory(0, &ehdr, sizeof(ehdr));

  Elf32_Phdr phdr;
  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_LOAD;
  phdr.p_vaddr = 0x2000;
  phdr.p_memsz = 0x10000;
  phdr.p_flags = PF_R | PF_X;
  phdr.p_align = 0x1000;
  memory_.SetMemory(0x100, &phdr, sizeof(phdr));

  uint64_t load_bias = 0;
  ASSERT_TRUE(elf->Init(&load_bias));
  EXPECT_EQ(0x2000U, load_bias);
  EXPECT_FALSE(elf->IsValidPc(0));
  EXPECT_FALSE(elf->IsValidPc(0x1000));
  EXPECT_FALSE(elf->IsValidPc(0x1fff));
  EXPECT_TRUE(elf->IsValidPc(0x2000));
  EXPECT_TRUE(elf->IsValidPc(0x5000));
  EXPECT_TRUE(elf->IsValidPc(0x11fff));
  EXPECT_FALSE(elf->IsValidPc(0x12000));
}

TEST_F(ElfInterfaceTest, is_valid_pc_from_debug_frame) {
  std::unique_ptr<ElfInterface> elf(new ElfInterface32(&memory_));

  uint64_t sh_offset = 0x100;

  Elf32_Ehdr ehdr;
  memset(&ehdr, 0, sizeof(ehdr));
  ehdr.e_shstrndx = 1;
  ehdr.e_shoff = sh_offset;
  ehdr.e_shentsize = sizeof(Elf32_Shdr);
  ehdr.e_shnum = 3;
  memory_.SetMemory(0, &ehdr, sizeof(ehdr));

  Elf32_Shdr shdr;
  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_NULL;
  memory_.SetMemory(sh_offset, &shdr, sizeof(shdr));

  sh_offset += sizeof(shdr);
  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_STRTAB;
  shdr.sh_name = 1;
  shdr.sh_offset = 0x500;
  shdr.sh_size = 0x100;
  memory_.SetMemory(sh_offset, &shdr, sizeof(shdr));
  memory_.SetMemory(0x500, ".debug_frame");

  sh_offset += sizeof(shdr);
  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_PROGBITS;
  shdr.sh_name = 0;
  shdr.sh_addr = 0x600;
  shdr.sh_offset = 0x600;
  shdr.sh_size = 0x200;
  memory_.SetMemory(sh_offset, &shdr, sizeof(shdr));

  // CIE 32.
  memory_.SetData32(0x600, 0xfc);
  memory_.SetData32(0x604, 0xffffffff);
  memory_.SetData8(0x608, 1);
  memory_.SetData8(0x609, '\0');
  memory_.SetData8(0x60a, 0x4);
  memory_.SetData8(0x60b, 0x4);
  memory_.SetData8(0x60c, 0x1);

  // FDE 32.
  memory_.SetData32(0x700, 0xfc);
  memory_.SetData32(0x704, 0);
  memory_.SetData32(0x708, 0x2100);
  memory_.SetData32(0x70c, 0x200);

  uint64_t load_bias = 0;
  ASSERT_TRUE(elf->Init(&load_bias));
  elf->InitHeaders();
  EXPECT_EQ(0U, load_bias);
  EXPECT_FALSE(elf->IsValidPc(0));
  EXPECT_FALSE(elf->IsValidPc(0x20ff));
  EXPECT_TRUE(elf->IsValidPc(0x2100));
  EXPECT_TRUE(elf->IsValidPc(0x2200));
  EXPECT_TRUE(elf->IsValidPc(0x22ff));
  EXPECT_FALSE(elf->IsValidPc(0x2300));
}

TEST_F(ElfInterfaceTest, is_valid_pc_from_eh_frame) {
  std::unique_ptr<ElfInterface> elf(new ElfInterface32(&memory_));

  uint64_t sh_offset = 0x100;

  Elf32_Ehdr ehdr;
  memset(&ehdr, 0, sizeof(ehdr));
  ehdr.e_shstrndx = 1;
  ehdr.e_shoff = sh_offset;
  ehdr.e_shentsize = sizeof(Elf32_Shdr);
  ehdr.e_shnum = 3;
  memory_.SetMemory(0, &ehdr, sizeof(ehdr));

  Elf32_Shdr shdr;
  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_NULL;
  memory_.SetMemory(sh_offset, &shdr, sizeof(shdr));

  sh_offset += sizeof(shdr);
  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_STRTAB;
  shdr.sh_name = 1;
  shdr.sh_offset = 0x500;
  shdr.sh_size = 0x100;
  memory_.SetMemory(sh_offset, &shdr, sizeof(shdr));
  memory_.SetMemory(0x500, ".eh_frame");

  sh_offset += sizeof(shdr);
  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_PROGBITS;
  shdr.sh_name = 0;
  shdr.sh_addr = 0x600;
  shdr.sh_offset = 0x600;
  shdr.sh_size = 0x200;
  memory_.SetMemory(sh_offset, &shdr, sizeof(shdr));

  // CIE 32.
  memory_.SetData32(0x600, 0xfc);
  memory_.SetData32(0x604, 0);
  memory_.SetData8(0x608, 1);
  memory_.SetData8(0x609, '\0');
  memory_.SetData8(0x60a, 0x4);
  memory_.SetData8(0x60b, 0x4);
  memory_.SetData8(0x60c, 0x1);

  // FDE 32.
  memory_.SetData32(0x700, 0xfc);
  memory_.SetData32(0x704, 0x104);
  memory_.SetData32(0x708, 0x20f8);
  memory_.SetData32(0x70c, 0x200);

  uint64_t load_bias = 0;
  ASSERT_TRUE(elf->Init(&load_bias));
  elf->InitHeaders();
  EXPECT_EQ(0U, load_bias);
  EXPECT_FALSE(elf->IsValidPc(0));
  EXPECT_FALSE(elf->IsValidPc(0x27ff));
  EXPECT_TRUE(elf->IsValidPc(0x2800));
  EXPECT_TRUE(elf->IsValidPc(0x2900));
  EXPECT_TRUE(elf->IsValidPc(0x29ff));
  EXPECT_FALSE(elf->IsValidPc(0x2a00));
}

}  // namespace unwindstack
