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

#include <utility>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include <unwindstack/Elf.h>
#include <unwindstack/ElfInterface.h>
#include <unwindstack/MachineArm.h>
#include <unwindstack/MachineArm64.h>
#include <unwindstack/MachineMips.h>
#include <unwindstack/MachineMips64.h>
#include <unwindstack/MachineX86.h>
#include <unwindstack/MachineX86_64.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/RegsArm.h>
#include <unwindstack/RegsArm64.h>
#include <unwindstack/RegsMips.h>
#include <unwindstack/RegsMips64.h>
#include <unwindstack/RegsX86.h>
#include <unwindstack/RegsX86_64.h>

namespace unwindstack {

struct Register {
  std::string expected_name;
  uint64_t offset;

  bool operator==(const Register& rhs) const {
    return std::tie(expected_name, offset) == std::tie(rhs.expected_name, rhs.offset);
  }
};

template<typename T>
class RegsIterateTest : public ::testing::Test {
};

template<typename RegsType>
std::vector<Register> ExpectedRegisters();

template<>
std::vector<Register> ExpectedRegisters<RegsArm>() {
  std::vector<Register> result;
  result.push_back({"r0", ARM_REG_R0});
  result.push_back({"r1", ARM_REG_R1});
  result.push_back({"r2", ARM_REG_R2});
  result.push_back({"r3", ARM_REG_R3});
  result.push_back({"r4", ARM_REG_R4});
  result.push_back({"r5", ARM_REG_R5});
  result.push_back({"r6", ARM_REG_R6});
  result.push_back({"r7", ARM_REG_R7});
  result.push_back({"r8", ARM_REG_R8});
  result.push_back({"r9", ARM_REG_R9});
  result.push_back({"r10", ARM_REG_R10});
  result.push_back({"r11", ARM_REG_R11});
  result.push_back({"ip", ARM_REG_R12});
  result.push_back({"sp", ARM_REG_SP});
  result.push_back({"lr", ARM_REG_LR});
  result.push_back({"pc", ARM_REG_PC});
  return result;
}

template<>
std::vector<Register> ExpectedRegisters<RegsArm64>() {
  std::vector<Register> result;
  result.push_back({"x0", ARM64_REG_R0});
  result.push_back({"x1", ARM64_REG_R1});
  result.push_back({"x2", ARM64_REG_R2});
  result.push_back({"x3", ARM64_REG_R3});
  result.push_back({"x4", ARM64_REG_R4});
  result.push_back({"x5", ARM64_REG_R5});
  result.push_back({"x6", ARM64_REG_R6});
  result.push_back({"x7", ARM64_REG_R7});
  result.push_back({"x8", ARM64_REG_R8});
  result.push_back({"x9", ARM64_REG_R9});
  result.push_back({"x10", ARM64_REG_R10});
  result.push_back({"x11", ARM64_REG_R11});
  result.push_back({"x12", ARM64_REG_R12});
  result.push_back({"x13", ARM64_REG_R13});
  result.push_back({"x14", ARM64_REG_R14});
  result.push_back({"x15", ARM64_REG_R15});
  result.push_back({"x16", ARM64_REG_R16});
  result.push_back({"x17", ARM64_REG_R17});
  result.push_back({"x18", ARM64_REG_R18});
  result.push_back({"x19", ARM64_REG_R19});
  result.push_back({"x20", ARM64_REG_R20});
  result.push_back({"x21", ARM64_REG_R21});
  result.push_back({"x22", ARM64_REG_R22});
  result.push_back({"x23", ARM64_REG_R23});
  result.push_back({"x24", ARM64_REG_R24});
  result.push_back({"x25", ARM64_REG_R25});
  result.push_back({"x26", ARM64_REG_R26});
  result.push_back({"x27", ARM64_REG_R27});
  result.push_back({"x28", ARM64_REG_R28});
  result.push_back({"x29", ARM64_REG_R29});
  result.push_back({"sp", ARM64_REG_SP});
  result.push_back({"lr", ARM64_REG_LR});
  result.push_back({"pc", ARM64_REG_PC});
  return result;
}

template<>
std::vector<Register> ExpectedRegisters<RegsX86>() {
  std::vector<Register> result;
  result.push_back({"eax", X86_REG_EAX});
  result.push_back({"ebx", X86_REG_EBX});
  result.push_back({"ecx", X86_REG_ECX});
  result.push_back({"edx", X86_REG_EDX});
  result.push_back({"ebp", X86_REG_EBP});
  result.push_back({"edi", X86_REG_EDI});
  result.push_back({"esi", X86_REG_ESI});
  result.push_back({"esp", X86_REG_ESP});
  result.push_back({"eip", X86_REG_EIP});
  return result;
}

template<>
std::vector<Register> ExpectedRegisters<RegsX86_64>() {
  std::vector<Register> result;
  result.push_back({"rax", X86_64_REG_RAX});
  result.push_back({"rbx", X86_64_REG_RBX});
  result.push_back({"rcx", X86_64_REG_RCX});
  result.push_back({"rdx", X86_64_REG_RDX});
  result.push_back({"r8", X86_64_REG_R8});
  result.push_back({"r9", X86_64_REG_R9});
  result.push_back({"r10", X86_64_REG_R10});
  result.push_back({"r11", X86_64_REG_R11});
  result.push_back({"r12", X86_64_REG_R12});
  result.push_back({"r13", X86_64_REG_R13});
  result.push_back({"r14", X86_64_REG_R14});
  result.push_back({"r15", X86_64_REG_R15});
  result.push_back({"rdi", X86_64_REG_RDI});
  result.push_back({"rsi", X86_64_REG_RSI});
  result.push_back({"rbp", X86_64_REG_RBP});
  result.push_back({"rsp", X86_64_REG_RSP});
  result.push_back({"rip", X86_64_REG_RIP});
  return result;
}

template<>
std::vector<Register> ExpectedRegisters<RegsMips>() {
  std::vector<Register> result;
  result.push_back({"r0", MIPS_REG_R0});
  result.push_back({"r1", MIPS_REG_R1});
  result.push_back({"r2", MIPS_REG_R2});
  result.push_back({"r3", MIPS_REG_R3});
  result.push_back({"r4", MIPS_REG_R4});
  result.push_back({"r5", MIPS_REG_R5});
  result.push_back({"r6", MIPS_REG_R6});
  result.push_back({"r7", MIPS_REG_R7});
  result.push_back({"r8", MIPS_REG_R8});
  result.push_back({"r9", MIPS_REG_R9});
  result.push_back({"r10", MIPS_REG_R10});
  result.push_back({"r11", MIPS_REG_R11});
  result.push_back({"r12", MIPS_REG_R12});
  result.push_back({"r13", MIPS_REG_R13});
  result.push_back({"r14", MIPS_REG_R14});
  result.push_back({"r15", MIPS_REG_R15});
  result.push_back({"r16", MIPS_REG_R16});
  result.push_back({"r17", MIPS_REG_R17});
  result.push_back({"r18", MIPS_REG_R18});
  result.push_back({"r19", MIPS_REG_R19});
  result.push_back({"r20", MIPS_REG_R20});
  result.push_back({"r21", MIPS_REG_R21});
  result.push_back({"r22", MIPS_REG_R22});
  result.push_back({"r23", MIPS_REG_R23});
  result.push_back({"r24", MIPS_REG_R24});
  result.push_back({"r25", MIPS_REG_R25});
  result.push_back({"r26", MIPS_REG_R26});
  result.push_back({"r27", MIPS_REG_R27});
  result.push_back({"r28", MIPS_REG_R28});
  result.push_back({"sp", MIPS_REG_SP});
  result.push_back({"r30", MIPS_REG_R30});
  result.push_back({"ra", MIPS_REG_RA});
  result.push_back({"pc", MIPS_REG_PC});

  return result;
}

template<>
std::vector<Register> ExpectedRegisters<RegsMips64>() {
  std::vector<Register> result;
  result.push_back({"r0", MIPS64_REG_R0});
  result.push_back({"r1", MIPS64_REG_R1});
  result.push_back({"r2", MIPS64_REG_R2});
  result.push_back({"r3", MIPS64_REG_R3});
  result.push_back({"r4", MIPS64_REG_R4});
  result.push_back({"r5", MIPS64_REG_R5});
  result.push_back({"r6", MIPS64_REG_R6});
  result.push_back({"r7", MIPS64_REG_R7});
  result.push_back({"r8", MIPS64_REG_R8});
  result.push_back({"r9", MIPS64_REG_R9});
  result.push_back({"r10", MIPS64_REG_R10});
  result.push_back({"r11", MIPS64_REG_R11});
  result.push_back({"r12", MIPS64_REG_R12});
  result.push_back({"r13", MIPS64_REG_R13});
  result.push_back({"r14", MIPS64_REG_R14});
  result.push_back({"r15", MIPS64_REG_R15});
  result.push_back({"r16", MIPS64_REG_R16});
  result.push_back({"r17", MIPS64_REG_R17});
  result.push_back({"r18", MIPS64_REG_R18});
  result.push_back({"r19", MIPS64_REG_R19});
  result.push_back({"r20", MIPS64_REG_R20});
  result.push_back({"r21", MIPS64_REG_R21});
  result.push_back({"r22", MIPS64_REG_R22});
  result.push_back({"r23", MIPS64_REG_R23});
  result.push_back({"r24", MIPS64_REG_R24});
  result.push_back({"r25", MIPS64_REG_R25});
  result.push_back({"r26", MIPS64_REG_R26});
  result.push_back({"r27", MIPS64_REG_R27});
  result.push_back({"r28", MIPS64_REG_R28});
  result.push_back({"sp", MIPS64_REG_SP});
  result.push_back({"r30", MIPS64_REG_R30});
  result.push_back({"ra", MIPS64_REG_RA});
  result.push_back({"pc", MIPS64_REG_PC});

  return result;
}

using RegTypes = ::testing::Types<RegsArm, RegsArm64, RegsX86, RegsX86_64, RegsMips, RegsMips64>;
TYPED_TEST_CASE(RegsIterateTest, RegTypes);

TYPED_TEST(RegsIterateTest, iterate) {
  std::vector<Register> expected = ExpectedRegisters<TypeParam>();
  TypeParam regs;
  for (const auto& reg : expected) {
    regs[reg.offset] = reg.offset;
  }

  std::vector<Register> actual;
  regs.IterateRegisters([&actual](const char* name, uint64_t value) {
    actual.push_back({name, value});
  });

  ASSERT_EQ(expected, actual);
}

}  // namespace unwindstack
