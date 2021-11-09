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

#include <gtest/gtest.h>

#include <unwindstack/Elf.h>
#include <unwindstack/MachineArm.h>
#include <unwindstack/MachineArm64.h>
#include <unwindstack/MachineMips.h>
#include <unwindstack/MachineMips64.h>
#include <unwindstack/MachineX86.h>
#include <unwindstack/MachineX86_64.h>
#include <unwindstack/RegsArm.h>
#include <unwindstack/RegsArm64.h>
#include <unwindstack/RegsMips.h>
#include <unwindstack/RegsMips64.h>
#include <unwindstack/RegsX86.h>
#include <unwindstack/RegsX86_64.h>

#include "MemoryFake.h"

namespace unwindstack {

class RegsStepIfSignalHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    elf_memory_ = new MemoryFake;
    elf_.reset(new Elf(elf_memory_));
  }

  void ArmStepIfSignalHandlerNonRt(uint32_t pc_data);
  void ArmStepIfSignalHandlerRt(uint32_t pc_data);

  MemoryFake* elf_memory_;
  MemoryFake process_memory_;
  std::unique_ptr<Elf> elf_;
};

void RegsStepIfSignalHandlerTest::ArmStepIfSignalHandlerNonRt(uint32_t pc_data) {
  uint64_t addr = 0x1000;
  RegsArm regs;
  regs[ARM_REG_PC] = 0x5000;
  regs[ARM_REG_SP] = addr;

  elf_memory_->SetData32(0x5000, pc_data);

  for (uint64_t index = 0; index <= 30; index++) {
    process_memory_.SetData32(addr + index * 4, index * 0x10);
  }

  ASSERT_TRUE(regs.StepIfSignalHandler(0x5000, elf_.get(), &process_memory_));
  EXPECT_EQ(0x100U, regs[ARM_REG_SP]);
  EXPECT_EQ(0x120U, regs[ARM_REG_PC]);
  EXPECT_EQ(0x100U, regs.sp());
  EXPECT_EQ(0x120U, regs.pc());
}

TEST_F(RegsStepIfSignalHandlerTest, arm_step_if_signal_handler_non_rt) {
  // Form 1
  ArmStepIfSignalHandlerNonRt(0xe3a07077);

  // Form 2
  ArmStepIfSignalHandlerNonRt(0xef900077);

  // Form 3
  ArmStepIfSignalHandlerNonRt(0xdf002777);
}

void RegsStepIfSignalHandlerTest::ArmStepIfSignalHandlerRt(uint32_t pc_data) {
  uint64_t addr = 0x1000;
  RegsArm regs;
  regs[ARM_REG_PC] = 0x5000;
  regs[ARM_REG_SP] = addr;

  elf_memory_->SetData32(0x5000, pc_data);

  for (uint64_t index = 0; index <= 100; index++) {
    process_memory_.SetData32(addr + index * 4, index * 0x10);
  }

  ASSERT_TRUE(regs.StepIfSignalHandler(0x5000, elf_.get(), &process_memory_));
  EXPECT_EQ(0x350U, regs[ARM_REG_SP]);
  EXPECT_EQ(0x370U, regs[ARM_REG_PC]);
  EXPECT_EQ(0x350U, regs.sp());
  EXPECT_EQ(0x370U, regs.pc());
}

TEST_F(RegsStepIfSignalHandlerTest, arm_step_if_signal_handler_rt) {
  // Form 1
  ArmStepIfSignalHandlerRt(0xe3a070ad);

  // Form 2
  ArmStepIfSignalHandlerRt(0xef9000ad);

  // Form 3
  ArmStepIfSignalHandlerRt(0xdf0027ad);
}

TEST_F(RegsStepIfSignalHandlerTest, arm64_step_if_signal_handler) {
  uint64_t addr = 0x1000;
  RegsArm64 regs;
  regs[ARM64_REG_PC] = 0x8000;
  regs[ARM64_REG_SP] = addr;

  elf_memory_->SetData64(0x8000, 0xd4000001d2801168ULL);

  for (uint64_t index = 0; index <= 100; index++) {
    process_memory_.SetData64(addr + index * 8, index * 0x10);
  }

  ASSERT_TRUE(regs.StepIfSignalHandler(0x8000, elf_.get(), &process_memory_));
  EXPECT_EQ(0x460U, regs[ARM64_REG_SP]);
  EXPECT_EQ(0x470U, regs[ARM64_REG_PC]);
  EXPECT_EQ(0x460U, regs.sp());
  EXPECT_EQ(0x470U, regs.pc());
}

TEST_F(RegsStepIfSignalHandlerTest, x86_step_if_signal_handler_no_siginfo) {
  uint64_t addr = 0xa00;
  RegsX86 regs;
  regs[X86_REG_EIP] = 0x4100;
  regs[X86_REG_ESP] = addr;

  elf_memory_->SetData64(0x4100, 0x80cd00000077b858ULL);
  for (uint64_t index = 0; index <= 25; index++) {
    process_memory_.SetData32(addr + index * 4, index * 0x10);
  }

  ASSERT_TRUE(regs.StepIfSignalHandler(0x4100, elf_.get(), &process_memory_));
  EXPECT_EQ(0x70U, regs[X86_REG_EBP]);
  EXPECT_EQ(0x80U, regs[X86_REG_ESP]);
  EXPECT_EQ(0x90U, regs[X86_REG_EBX]);
  EXPECT_EQ(0xa0U, regs[X86_REG_EDX]);
  EXPECT_EQ(0xb0U, regs[X86_REG_ECX]);
  EXPECT_EQ(0xc0U, regs[X86_REG_EAX]);
  EXPECT_EQ(0xf0U, regs[X86_REG_EIP]);
  EXPECT_EQ(0x80U, regs.sp());
  EXPECT_EQ(0xf0U, regs.pc());
}

TEST_F(RegsStepIfSignalHandlerTest, x86_step_if_signal_handler_siginfo) {
  uint64_t addr = 0xa00;
  RegsX86 regs;
  regs[X86_REG_EIP] = 0x4100;
  regs[X86_REG_ESP] = addr;

  elf_memory_->SetData64(0x4100, 0x0080cd000000adb8ULL);
  addr += 8;
  // Pointer to ucontext data.
  process_memory_.SetData32(addr, 0x8100);

  addr = 0x8100;
  for (uint64_t index = 0; index <= 30; index++) {
    process_memory_.SetData32(addr + index * 4, index * 0x10);
  }

  ASSERT_TRUE(regs.StepIfSignalHandler(0x4100, elf_.get(), &process_memory_));
  EXPECT_EQ(0xb0U, regs[X86_REG_EBP]);
  EXPECT_EQ(0xc0U, regs[X86_REG_ESP]);
  EXPECT_EQ(0xd0U, regs[X86_REG_EBX]);
  EXPECT_EQ(0xe0U, regs[X86_REG_EDX]);
  EXPECT_EQ(0xf0U, regs[X86_REG_ECX]);
  EXPECT_EQ(0x100U, regs[X86_REG_EAX]);
  EXPECT_EQ(0x130U, regs[X86_REG_EIP]);
  EXPECT_EQ(0xc0U, regs.sp());
  EXPECT_EQ(0x130U, regs.pc());
}

TEST_F(RegsStepIfSignalHandlerTest, x86_64_step_if_signal_handler) {
  uint64_t addr = 0x500;
  RegsX86_64 regs;
  regs[X86_64_REG_RIP] = 0x7000;
  regs[X86_64_REG_RSP] = addr;

  elf_memory_->SetData64(0x7000, 0x0f0000000fc0c748);
  elf_memory_->SetData16(0x7008, 0x0f05);

  for (uint64_t index = 0; index <= 30; index++) {
    process_memory_.SetData64(addr + index * 8, index * 0x10);
  }

  ASSERT_TRUE(regs.StepIfSignalHandler(0x7000, elf_.get(), &process_memory_));
  EXPECT_EQ(0x140U, regs[X86_64_REG_RSP]);
  EXPECT_EQ(0x150U, regs[X86_64_REG_RIP]);
  EXPECT_EQ(0x140U, regs.sp());
  EXPECT_EQ(0x150U, regs.pc());
}

TEST_F(RegsStepIfSignalHandlerTest, mips_step_if_signal_handler_non_rt) {
  uint64_t addr = 0x1000;
  RegsMips regs;
  regs[MIPS_REG_PC] = 0x8000;
  regs[MIPS_REG_SP] = addr;

  elf_memory_->SetData64(0x8000, 0x0000000c24021017ULL);

  for (uint64_t index = 0; index <= 50; index++) {
    process_memory_.SetData64(addr + index * 8, index * 0x10);
  }

  ASSERT_TRUE(regs.StepIfSignalHandler(0x8000, elf_.get(), &process_memory_));
  EXPECT_EQ(0x220U, regs[MIPS_REG_SP]);
  EXPECT_EQ(0x040U, regs[MIPS_REG_PC]);
  EXPECT_EQ(0x220U, regs.sp());
  EXPECT_EQ(0x040U, regs.pc());
}

TEST_F(RegsStepIfSignalHandlerTest, mips_step_if_signal_handler_rt) {
  uint64_t addr = 0x1000;
  RegsMips regs;
  regs[MIPS_REG_PC] = 0x8000;
  regs[MIPS_REG_SP] = addr;

  elf_memory_->SetData64(0x8000, 0x0000000c24021061ULL);

  for (uint64_t index = 0; index <= 100; index++) {
    process_memory_.SetData64(addr + index * 8, index * 0x10);
  }

  ASSERT_TRUE(regs.StepIfSignalHandler(0x8000, elf_.get(), &process_memory_));
  EXPECT_EQ(0x350U, regs[MIPS_REG_SP]);
  EXPECT_EQ(0x170U, regs[MIPS_REG_PC]);
  EXPECT_EQ(0x350U, regs.sp());
  EXPECT_EQ(0x170U, regs.pc());
}

TEST_F(RegsStepIfSignalHandlerTest, mips64_step_if_signal_handler) {
  uint64_t addr = 0x1000;
  RegsMips64 regs;
  regs[MIPS64_REG_PC] = 0x8000;
  regs[MIPS64_REG_SP] = addr;

  elf_memory_->SetData64(0x8000, 0x0000000c2402145bULL);

  for (uint64_t index = 0; index <= 100; index++) {
    process_memory_.SetData64(addr + index * 8, index * 0x10);
  }

  ASSERT_TRUE(regs.StepIfSignalHandler(0x8000, elf_.get(), &process_memory_));
  EXPECT_EQ(0x350U, regs[MIPS64_REG_SP]);
  EXPECT_EQ(0x600U, regs[MIPS64_REG_PC]);
  EXPECT_EQ(0x350U, regs.sp());
  EXPECT_EQ(0x600U, regs.pc());
}

}  // namespace unwindstack
