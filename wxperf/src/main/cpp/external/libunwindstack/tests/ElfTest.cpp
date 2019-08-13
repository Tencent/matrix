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
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <unwindstack/Elf.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/RegsArm.h>

#include "ElfFake.h"
#include "ElfTestUtils.h"
#include "LogFake.h"
#include "MemoryFake.h"

#if !defined(PT_ARM_EXIDX)
#define PT_ARM_EXIDX 0x70000001
#endif

namespace unwindstack {

class ElfTest : public ::testing::Test {
 protected:
  void SetUp() override {
    memory_ = new MemoryFake;
  }

  void InitElf32(uint32_t machine_type) {
    Elf32_Ehdr ehdr;
    TestInitEhdr<Elf32_Ehdr>(&ehdr, ELFCLASS32, machine_type);

    ehdr.e_phoff = 0x100;
    ehdr.e_ehsize = sizeof(ehdr);
    ehdr.e_phentsize = sizeof(Elf32_Phdr);
    ehdr.e_phnum = 1;
    ehdr.e_shentsize = sizeof(Elf32_Shdr);
    if (machine_type == EM_ARM) {
      ehdr.e_flags = 0x5000200;
      ehdr.e_phnum = 2;
    }
    memory_->SetMemory(0, &ehdr, sizeof(ehdr));

    Elf32_Phdr phdr;
    memset(&phdr, 0, sizeof(phdr));
    phdr.p_type = PT_LOAD;
    phdr.p_filesz = 0x10000;
    phdr.p_memsz = 0x10000;
    phdr.p_flags = PF_R | PF_X;
    phdr.p_align = 0x1000;
    memory_->SetMemory(0x100, &phdr, sizeof(phdr));

    if (machine_type == EM_ARM) {
      memset(&phdr, 0, sizeof(phdr));
      phdr.p_type = PT_ARM_EXIDX;
      phdr.p_offset = 0x30000;
      phdr.p_vaddr = 0x30000;
      phdr.p_paddr = 0x30000;
      phdr.p_filesz = 16;
      phdr.p_memsz = 16;
      phdr.p_flags = PF_R;
      phdr.p_align = 0x4;
      memory_->SetMemory(0x100 + sizeof(phdr), &phdr, sizeof(phdr));
    }
  }

  void InitElf64(uint32_t machine_type) {
    Elf64_Ehdr ehdr;
    TestInitEhdr<Elf64_Ehdr>(&ehdr, ELFCLASS64, machine_type);

    ehdr.e_phoff = 0x100;
    ehdr.e_flags = 0x5000200;
    ehdr.e_ehsize = sizeof(ehdr);
    ehdr.e_phentsize = sizeof(Elf64_Phdr);
    ehdr.e_phnum = 1;
    ehdr.e_shentsize = sizeof(Elf64_Shdr);
    memory_->SetMemory(0, &ehdr, sizeof(ehdr));

    Elf64_Phdr phdr;
    memset(&phdr, 0, sizeof(phdr));
    phdr.p_type = PT_LOAD;
    phdr.p_filesz = 0x10000;
    phdr.p_memsz = 0x10000;
    phdr.p_flags = PF_R | PF_X;
    phdr.p_align = 0x1000;
    memory_->SetMemory(0x100, &phdr, sizeof(phdr));
  }

  MemoryFake* memory_;
};

TEST_F(ElfTest, invalid_memory) {
  Elf elf(memory_);

  ASSERT_FALSE(elf.Init(false));
  ASSERT_FALSE(elf.valid());
}

TEST_F(ElfTest, elf_invalid) {
  Elf elf(memory_);

  InitElf32(EM_386);

  // Corrupt the ELF signature.
  memory_->SetData32(0, 0x7f000000);

  ASSERT_FALSE(elf.Init(false));
  ASSERT_FALSE(elf.valid());
  ASSERT_TRUE(elf.interface() == nullptr);

  std::string name;
  ASSERT_FALSE(elf.GetSoname(&name));

  uint64_t func_offset;
  ASSERT_FALSE(elf.GetFunctionName(0, &name, &func_offset));

  bool finished;
  ASSERT_FALSE(elf.Step(0, 0, nullptr, nullptr, &finished));
}

TEST_F(ElfTest, elf32_invalid_machine) {
  Elf elf(memory_);

  InitElf32(EM_PPC);

  ResetLogs();
  ASSERT_FALSE(elf.Init(false));

  ASSERT_EQ("", GetFakeLogBuf());
  ASSERT_EQ("4 unwind 32 bit elf that is neither arm nor x86 nor mips: e_machine = 20\n\n",
            GetFakeLogPrint());
}

TEST_F(ElfTest, elf64_invalid_machine) {
  Elf elf(memory_);

  InitElf64(EM_PPC64);

  ResetLogs();
  ASSERT_FALSE(elf.Init(false));

  ASSERT_EQ("", GetFakeLogBuf());
  ASSERT_EQ("4 unwind 64 bit elf that is neither aarch64 nor x86_64 nor mips64: e_machine = 21\n\n",
            GetFakeLogPrint());
}

TEST_F(ElfTest, elf_arm) {
  Elf elf(memory_);

  InitElf32(EM_ARM);

  ASSERT_TRUE(elf.Init(false));
  ASSERT_TRUE(elf.valid());
  ASSERT_EQ(static_cast<uint32_t>(EM_ARM), elf.machine_type());
  ASSERT_EQ(ELFCLASS32, elf.class_type());
  ASSERT_TRUE(elf.interface() != nullptr);
}

TEST_F(ElfTest, elf_mips) {
  Elf elf(memory_);

  InitElf32(EM_MIPS);

  ASSERT_TRUE(elf.Init(false));
  ASSERT_TRUE(elf.valid());
  ASSERT_EQ(static_cast<uint32_t>(EM_MIPS), elf.machine_type());
  ASSERT_EQ(ELFCLASS32, elf.class_type());
  ASSERT_TRUE(elf.interface() != nullptr);
}

TEST_F(ElfTest, elf_x86) {
  Elf elf(memory_);

  InitElf32(EM_386);

  ASSERT_TRUE(elf.Init(false));
  ASSERT_TRUE(elf.valid());
  ASSERT_EQ(static_cast<uint32_t>(EM_386), elf.machine_type());
  ASSERT_EQ(ELFCLASS32, elf.class_type());
  ASSERT_TRUE(elf.interface() != nullptr);
}

TEST_F(ElfTest, elf_arm64) {
  Elf elf(memory_);

  InitElf64(EM_AARCH64);

  ASSERT_TRUE(elf.Init(false));
  ASSERT_TRUE(elf.valid());
  ASSERT_EQ(static_cast<uint32_t>(EM_AARCH64), elf.machine_type());
  ASSERT_EQ(ELFCLASS64, elf.class_type());
  ASSERT_TRUE(elf.interface() != nullptr);
}

TEST_F(ElfTest, elf_x86_64) {
  Elf elf(memory_);

  InitElf64(EM_X86_64);

  ASSERT_TRUE(elf.Init(false));
  ASSERT_TRUE(elf.valid());
  ASSERT_EQ(static_cast<uint32_t>(EM_X86_64), elf.machine_type());
  ASSERT_EQ(ELFCLASS64, elf.class_type());
  ASSERT_TRUE(elf.interface() != nullptr);
}

TEST_F(ElfTest, elf_mips64) {
  Elf elf(memory_);

  InitElf64(EM_MIPS);

  ASSERT_TRUE(elf.Init(false));
  ASSERT_TRUE(elf.valid());
  ASSERT_EQ(static_cast<uint32_t>(EM_MIPS), elf.machine_type());
  ASSERT_EQ(ELFCLASS64, elf.class_type());
  ASSERT_TRUE(elf.interface() != nullptr);
}

TEST_F(ElfTest, gnu_debugdata_init_fail32) {
  TestInitGnuDebugdata<Elf32_Ehdr, Elf32_Shdr>(ELFCLASS32, EM_ARM, false,
                                               [&](uint64_t offset, const void* ptr, size_t size) {
                                                 memory_->SetMemory(offset, ptr, size);
                                               });

  Elf elf(memory_);
  ASSERT_TRUE(elf.Init(false));
  ASSERT_TRUE(elf.interface() != nullptr);
  ASSERT_TRUE(elf.gnu_debugdata_interface() == nullptr);
  EXPECT_EQ(0x1acU, elf.interface()->gnu_debugdata_offset());
  EXPECT_EQ(0x100U, elf.interface()->gnu_debugdata_size());
}

TEST_F(ElfTest, gnu_debugdata_init_fail64) {
  TestInitGnuDebugdata<Elf64_Ehdr, Elf64_Shdr>(ELFCLASS64, EM_AARCH64, false,
                                               [&](uint64_t offset, const void* ptr, size_t size) {
                                                 memory_->SetMemory(offset, ptr, size);
                                               });

  Elf elf(memory_);
  ASSERT_TRUE(elf.Init(false));
  ASSERT_TRUE(elf.interface() != nullptr);
  ASSERT_TRUE(elf.gnu_debugdata_interface() == nullptr);
  EXPECT_EQ(0x200U, elf.interface()->gnu_debugdata_offset());
  EXPECT_EQ(0x100U, elf.interface()->gnu_debugdata_size());
}

TEST_F(ElfTest, gnu_debugdata_init32) {
  TestInitGnuDebugdata<Elf32_Ehdr, Elf32_Shdr>(ELFCLASS32, EM_ARM, true,
                                               [&](uint64_t offset, const void* ptr, size_t size) {
                                                 memory_->SetMemory(offset, ptr, size);
                                               });

  Elf elf(memory_);
  ASSERT_TRUE(elf.Init(true));
  ASSERT_TRUE(elf.interface() != nullptr);
  ASSERT_TRUE(elf.gnu_debugdata_interface() != nullptr);
  EXPECT_EQ(0x1acU, elf.interface()->gnu_debugdata_offset());
  EXPECT_EQ(0x8cU, elf.interface()->gnu_debugdata_size());
}

TEST_F(ElfTest, gnu_debugdata_init64) {
  TestInitGnuDebugdata<Elf64_Ehdr, Elf64_Shdr>(ELFCLASS64, EM_AARCH64, true,
                                               [&](uint64_t offset, const void* ptr, size_t size) {
                                                 memory_->SetMemory(offset, ptr, size);
                                               });

  Elf elf(memory_);
  ASSERT_TRUE(elf.Init(true));
  ASSERT_TRUE(elf.interface() != nullptr);
  ASSERT_TRUE(elf.gnu_debugdata_interface() != nullptr);
  EXPECT_EQ(0x200U, elf.interface()->gnu_debugdata_offset());
  EXPECT_EQ(0x90U, elf.interface()->gnu_debugdata_size());
}

TEST_F(ElfTest, rel_pc) {
  ElfFake elf(memory_);

  ElfInterfaceFake* interface = new ElfInterfaceFake(memory_);
  elf.FakeSetInterface(interface);

  elf.FakeSetValid(true);
  elf.FakeSetLoadBias(0);
  MapInfo map_info(0x1000, 0x2000);

  ASSERT_EQ(0x101U, elf.GetRelPc(0x1101, &map_info));

  elf.FakeSetLoadBias(0x3000);
  ASSERT_EQ(0x3101U, elf.GetRelPc(0x1101, &map_info));

  elf.FakeSetValid(false);
  elf.FakeSetLoadBias(0);
  ASSERT_EQ(0x101U, elf.GetRelPc(0x1101, &map_info));
}

TEST_F(ElfTest, step_in_signal_map) {
  ElfFake elf(memory_);

  RegsArm regs;
  regs[13] = 0x50000;
  regs[15] = 0x8000;

  ElfInterfaceFake* interface = new ElfInterfaceFake(memory_);
  elf.FakeSetInterface(interface);

  memory_->SetData32(0x3000, 0xdf0027ad);
  MemoryFake process_memory;
  process_memory.SetData32(0x50000, 0);
  for (size_t i = 0; i < 16; i++) {
    process_memory.SetData32(0x500a0 + i * sizeof(uint32_t), i);
  }

  elf.FakeSetValid(true);
  elf.FakeSetLoadBias(0);
  bool finished;
  ASSERT_TRUE(elf.Step(0x3000, 0x1000, &regs, &process_memory, &finished));
  EXPECT_FALSE(finished);
  EXPECT_EQ(15U, regs.pc());
  EXPECT_EQ(13U, regs.sp());
}

class ElfInterfaceMock : public ElfInterface {
 public:
  ElfInterfaceMock(Memory* memory) : ElfInterface(memory) {}
  virtual ~ElfInterfaceMock() = default;

  bool Init(uint64_t*) override { return false; }
  void InitHeaders() override {}
  bool GetSoname(std::string*) override { return false; }
  bool GetFunctionName(uint64_t, uint64_t, std::string*, uint64_t*) override { return false; }

  MOCK_METHOD5(Step, bool(uint64_t, uint64_t, Regs*, Memory*, bool*));
  MOCK_METHOD2(GetGlobalVariable, bool(const std::string&, uint64_t*));
  MOCK_METHOD1(IsValidPc, bool(uint64_t));

  void MockSetDynamicOffset(uint64_t offset) { dynamic_offset_ = offset; }
  void MockSetDynamicVaddr(uint64_t vaddr) { dynamic_vaddr_ = vaddr; }
  void MockSetDynamicSize(uint64_t size) { dynamic_size_ = size; }
};

TEST_F(ElfTest, step_in_interface) {
  ElfFake elf(memory_);
  elf.FakeSetValid(true);
  elf.FakeSetLoadBias(0);

  RegsArm regs;

  ElfInterfaceMock* interface = new ElfInterfaceMock(memory_);
  elf.FakeSetInterface(interface);
  MemoryFake process_memory;

  bool finished;
  EXPECT_CALL(*interface, Step(0x1000, 0, &regs, &process_memory, &finished))
      .WillOnce(::testing::Return(true));

  ASSERT_TRUE(elf.Step(0x1004, 0x1000, &regs, &process_memory, &finished));
}

TEST_F(ElfTest, step_in_interface_non_zero_load_bias) {
  ElfFake elf(memory_);
  elf.FakeSetValid(true);
  elf.FakeSetLoadBias(0x4000);

  RegsArm regs;

  ElfInterfaceMock* interface = new ElfInterfaceMock(memory_);
  elf.FakeSetInterface(interface);
  MemoryFake process_memory;

  bool finished;
  EXPECT_CALL(*interface, Step(0x7300, 0x4000, &regs, &process_memory, &finished))
      .WillOnce(::testing::Return(true));

  ASSERT_TRUE(elf.Step(0x7304, 0x7300, &regs, &process_memory, &finished));
}

TEST_F(ElfTest, get_global_invalid_elf) {
  ElfFake elf(memory_);
  elf.FakeSetValid(false);

  std::string global("something");
  uint64_t offset;
  ASSERT_FALSE(elf.GetGlobalVariable(global, &offset));
}

TEST_F(ElfTest, get_global_valid_not_in_interface) {
  ElfFake elf(memory_);
  elf.FakeSetValid(true);
  elf.FakeSetLoadBias(0);

  ElfInterfaceMock* interface = new ElfInterfaceMock(memory_);
  elf.FakeSetInterface(interface);

  uint64_t offset;
  std::string global("something");
  EXPECT_CALL(*interface, GetGlobalVariable(global, &offset)).WillOnce(::testing::Return(false));

  ASSERT_FALSE(elf.GetGlobalVariable(global, &offset));
}

TEST_F(ElfTest, get_global_valid_below_load_bias) {
  ElfFake elf(memory_);
  elf.FakeSetValid(true);
  elf.FakeSetLoadBias(0x1000);

  ElfInterfaceMock* interface = new ElfInterfaceMock(memory_);
  elf.FakeSetInterface(interface);

  uint64_t offset;
  std::string global("something");
  EXPECT_CALL(*interface, GetGlobalVariable(global, &offset))
      .WillOnce(::testing::DoAll(::testing::SetArgPointee<1>(0x300), ::testing::Return(true)));

  ASSERT_FALSE(elf.GetGlobalVariable(global, &offset));
}

TEST_F(ElfTest, get_global_valid_dynamic_zero) {
  ElfFake elf(memory_);
  elf.FakeSetValid(true);
  elf.FakeSetLoadBias(0);

  ElfInterfaceMock* interface = new ElfInterfaceMock(memory_);
  elf.FakeSetInterface(interface);

  ElfInterfaceMock* gnu_interface = new ElfInterfaceMock(memory_);
  elf.FakeSetGnuDebugdataInterface(gnu_interface);

  uint64_t offset;
  std::string global("something");
  EXPECT_CALL(*interface, GetGlobalVariable(global, &offset)).WillOnce(::testing::Return(false));

  EXPECT_CALL(*gnu_interface, GetGlobalVariable(global, &offset))
      .WillOnce(::testing::DoAll(::testing::SetArgPointee<1>(0x500), ::testing::Return(true)));

  ASSERT_TRUE(elf.GetGlobalVariable(global, &offset));
  EXPECT_EQ(0x500U, offset);
}

TEST_F(ElfTest, get_global_valid_in_gnu_debugdata_dynamic_zero) {
  ElfFake elf(memory_);
  elf.FakeSetValid(true);
  elf.FakeSetLoadBias(0);

  ElfInterfaceMock* interface = new ElfInterfaceMock(memory_);
  elf.FakeSetInterface(interface);

  uint64_t offset;
  std::string global("something");
  EXPECT_CALL(*interface, GetGlobalVariable(global, &offset))
      .WillOnce(::testing::DoAll(::testing::SetArgPointee<1>(0x300), ::testing::Return(true)));

  ASSERT_TRUE(elf.GetGlobalVariable(global, &offset));
  EXPECT_EQ(0x300U, offset);
}

TEST_F(ElfTest, get_global_valid_dynamic_zero_non_zero_load_bias) {
  ElfFake elf(memory_);
  elf.FakeSetValid(true);
  elf.FakeSetLoadBias(0x100);

  ElfInterfaceMock* interface = new ElfInterfaceMock(memory_);
  elf.FakeSetInterface(interface);

  uint64_t offset;
  std::string global("something");
  EXPECT_CALL(*interface, GetGlobalVariable(global, &offset))
      .WillOnce(::testing::DoAll(::testing::SetArgPointee<1>(0x300), ::testing::Return(true)));

  ASSERT_TRUE(elf.GetGlobalVariable(global, &offset));
  EXPECT_EQ(0x200U, offset);
}

TEST_F(ElfTest, get_global_valid_dynamic_adjust_negative) {
  ElfFake elf(memory_);
  elf.FakeSetValid(true);
  elf.FakeSetLoadBias(0);

  ElfInterfaceMock* interface = new ElfInterfaceMock(memory_);
  interface->MockSetDynamicOffset(0x400);
  interface->MockSetDynamicVaddr(0x800);
  interface->MockSetDynamicSize(0x100);
  elf.FakeSetInterface(interface);

  uint64_t offset;
  std::string global("something");
  EXPECT_CALL(*interface, GetGlobalVariable(global, &offset))
      .WillOnce(::testing::DoAll(::testing::SetArgPointee<1>(0x850), ::testing::Return(true)));

  ASSERT_TRUE(elf.GetGlobalVariable(global, &offset));
  EXPECT_EQ(0x450U, offset);
}

TEST_F(ElfTest, get_global_valid_dynamic_adjust_positive) {
  ElfFake elf(memory_);
  elf.FakeSetValid(true);
  elf.FakeSetLoadBias(0);

  ElfInterfaceMock* interface = new ElfInterfaceMock(memory_);
  interface->MockSetDynamicOffset(0x1000);
  interface->MockSetDynamicVaddr(0x800);
  interface->MockSetDynamicSize(0x100);
  elf.FakeSetInterface(interface);

  uint64_t offset;
  std::string global("something");
  EXPECT_CALL(*interface, GetGlobalVariable(global, &offset))
      .WillOnce(::testing::DoAll(::testing::SetArgPointee<1>(0x850), ::testing::Return(true)));

  ASSERT_TRUE(elf.GetGlobalVariable(global, &offset));
  EXPECT_EQ(0x1050U, offset);
}

TEST_F(ElfTest, is_valid_pc_elf_invalid) {
  ElfFake elf(memory_);
  elf.FakeSetValid(false);
  elf.FakeSetLoadBias(0);

  EXPECT_FALSE(elf.IsValidPc(0x100));
  EXPECT_FALSE(elf.IsValidPc(0x200));
}

TEST_F(ElfTest, is_valid_pc_interface) {
  ElfFake elf(memory_);
  elf.FakeSetValid(true);
  elf.FakeSetLoadBias(0);

  ElfInterfaceMock* interface = new ElfInterfaceMock(memory_);
  elf.FakeSetInterface(interface);

  EXPECT_CALL(*interface, IsValidPc(0x1500)).WillOnce(::testing::Return(true));

  EXPECT_TRUE(elf.IsValidPc(0x1500));
}

TEST_F(ElfTest, is_valid_pc_non_zero_load_bias) {
  ElfFake elf(memory_);
  elf.FakeSetValid(true);
  elf.FakeSetLoadBias(0x1000);

  ElfInterfaceMock* interface = new ElfInterfaceMock(memory_);
  elf.FakeSetInterface(interface);

  EXPECT_CALL(*interface, IsValidPc(0x500)).WillOnce(::testing::Return(true));

  EXPECT_FALSE(elf.IsValidPc(0x100));
  EXPECT_FALSE(elf.IsValidPc(0x200));
  EXPECT_TRUE(elf.IsValidPc(0x1500));
}

TEST_F(ElfTest, is_valid_pc_from_gnu_debugdata) {
  ElfFake elf(memory_);
  elf.FakeSetValid(true);
  elf.FakeSetLoadBias(0);

  ElfInterfaceMock* interface = new ElfInterfaceMock(memory_);
  elf.FakeSetInterface(interface);
  ElfInterfaceMock* gnu_interface = new ElfInterfaceMock(memory_);
  elf.FakeSetGnuDebugdataInterface(gnu_interface);

  EXPECT_CALL(*interface, IsValidPc(0x1500)).WillOnce(::testing::Return(false));
  EXPECT_CALL(*gnu_interface, IsValidPc(0x1500)).WillOnce(::testing::Return(true));

  EXPECT_TRUE(elf.IsValidPc(0x1500));
}

TEST_F(ElfTest, error_code_not_valid) {
  ElfFake elf(memory_);
  elf.FakeSetValid(false);

  ErrorData error{ERROR_MEMORY_INVALID, 0x100};
  elf.GetLastError(&error);
  EXPECT_EQ(ERROR_MEMORY_INVALID, error.code);
  EXPECT_EQ(0x100U, error.address);
}

TEST_F(ElfTest, error_code_valid) {
  ElfFake elf(memory_);
  elf.FakeSetValid(true);
  ElfInterfaceFake* interface = new ElfInterfaceFake(memory_);
  elf.FakeSetInterface(interface);
  interface->FakeSetErrorCode(ERROR_MEMORY_INVALID);
  interface->FakeSetErrorAddress(0x1000);

  ErrorData error{ERROR_NONE, 0};
  elf.GetLastError(&error);
  EXPECT_EQ(ERROR_MEMORY_INVALID, error.code);
  EXPECT_EQ(0x1000U, error.address);
  EXPECT_EQ(ERROR_MEMORY_INVALID, elf.GetLastErrorCode());
  EXPECT_EQ(0x1000U, elf.GetLastErrorAddress());
}

}  // namespace unwindstack
