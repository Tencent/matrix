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

#include <functional>

#include <unwindstack/Elf.h>
#include <unwindstack/MachineMips64.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Memory.h>
#include <unwindstack/RegsMips64.h>
#include <unwindstack/UcontextMips64.h>
#include <unwindstack/UserMips64.h>

namespace unwindstack {

RegsMips64::RegsMips64()
    : RegsImpl<uint64_t>(MIPS64_REG_LAST, Location(LOCATION_REGISTER, MIPS64_REG_RA)) {}

ArchEnum RegsMips64::Arch() {
  return ARCH_MIPS64;
}

uint64_t RegsMips64::pc() {
  return regs_[MIPS64_REG_PC];
}

uint64_t RegsMips64::sp() {
  return regs_[MIPS64_REG_SP];
}

void RegsMips64::set_pc(uint64_t pc) {
  regs_[MIPS64_REG_PC] = pc;
}

void RegsMips64::set_sp(uint64_t sp) {
  regs_[MIPS64_REG_SP] = sp;
}

uint64_t RegsMips64::GetPcAdjustment(uint64_t rel_pc, Elf*) {
  if (rel_pc < 8) {
    return 0;
  }
  // For now, just assume no compact branches
  return 8;
}

bool RegsMips64::SetPcFromReturnAddress(Memory*) {
  uint64_t ra = regs_[MIPS64_REG_RA];
  if (regs_[MIPS64_REG_PC] == ra) {
    return false;
  }

  regs_[MIPS64_REG_PC] = ra;
  return true;
}

void RegsMips64::IterateRegisters(std::function<void(const char*, uint64_t)> fn) {
  fn("r0", regs_[MIPS64_REG_R0]);
  fn("r1", regs_[MIPS64_REG_R1]);
  fn("r2", regs_[MIPS64_REG_R2]);
  fn("r3", regs_[MIPS64_REG_R3]);
  fn("r4", regs_[MIPS64_REG_R4]);
  fn("r5", regs_[MIPS64_REG_R5]);
  fn("r6", regs_[MIPS64_REG_R6]);
  fn("r7", regs_[MIPS64_REG_R7]);
  fn("r8", regs_[MIPS64_REG_R8]);
  fn("r9", regs_[MIPS64_REG_R9]);
  fn("r10", regs_[MIPS64_REG_R10]);
  fn("r11", regs_[MIPS64_REG_R11]);
  fn("r12", regs_[MIPS64_REG_R12]);
  fn("r13", regs_[MIPS64_REG_R13]);
  fn("r14", regs_[MIPS64_REG_R14]);
  fn("r15", regs_[MIPS64_REG_R15]);
  fn("r16", regs_[MIPS64_REG_R16]);
  fn("r17", regs_[MIPS64_REG_R17]);
  fn("r18", regs_[MIPS64_REG_R18]);
  fn("r19", regs_[MIPS64_REG_R19]);
  fn("r20", regs_[MIPS64_REG_R20]);
  fn("r21", regs_[MIPS64_REG_R21]);
  fn("r22", regs_[MIPS64_REG_R22]);
  fn("r23", regs_[MIPS64_REG_R23]);
  fn("r24", regs_[MIPS64_REG_R24]);
  fn("r25", regs_[MIPS64_REG_R25]);
  fn("r26", regs_[MIPS64_REG_R26]);
  fn("r27", regs_[MIPS64_REG_R27]);
  fn("r28", regs_[MIPS64_REG_R28]);
  fn("sp", regs_[MIPS64_REG_SP]);
  fn("r30", regs_[MIPS64_REG_R30]);
  fn("ra", regs_[MIPS64_REG_RA]);
  fn("pc", regs_[MIPS64_REG_PC]);
}

Regs* RegsMips64::Read(void* remote_data) {
  mips64_user_regs* user = reinterpret_cast<mips64_user_regs*>(remote_data);
  RegsMips64* regs = new RegsMips64();
  uint64_t* reg_data = reinterpret_cast<uint64_t*>(regs->RawData());

  memcpy(regs->RawData(), &user->regs[MIPS64_EF_R0], (MIPS64_REG_R31 + 1) * sizeof(uint64_t));

  reg_data[MIPS64_REG_PC] = user->regs[MIPS64_EF_CP0_EPC];
  return regs;
}

Regs* RegsMips64::CreateFromUcontext(void* ucontext) {
  mips64_ucontext_t* mips64_ucontext = reinterpret_cast<mips64_ucontext_t*>(ucontext);

  RegsMips64* regs = new RegsMips64();
  // Copy 64 bit sc_regs over to 64 bit regs
  memcpy(regs->RawData(), &mips64_ucontext->uc_mcontext.sc_regs[0], 32 * sizeof(uint64_t));
  (*regs)[MIPS64_REG_PC] = mips64_ucontext->uc_mcontext.sc_pc;
  return regs;
}

bool RegsMips64::StepIfSignalHandler(uint64_t rel_pc, Elf* elf, Memory* process_memory) {
  uint64_t data;
  Memory* elf_memory = elf->memory();
  // Read from elf memory since it is usually more expensive to read from
  // process memory.
  if (!elf_memory->Read(rel_pc, &data, sizeof(data))) {
    return false;
  }

  // Look for the kernel sigreturn function.
  // __vdso_rt_sigreturn:
  // 0x2402145b     li  v0, 0x145b
  // 0x0000000c     syscall
  if (data != 0x0000000c2402145bULL) {
    return false;
  }

  // vdso_rt_sigreturn => read rt_sigframe
  // offset = siginfo offset + sizeof(siginfo) + uc_mcontext offset
  // read 64 bit sc_regs[32] from stack into 64 bit regs_
  uint64_t sp = regs_[MIPS64_REG_SP];
  if (!process_memory->Read(sp + 24 + 128 + 40, regs_.data(),
                            sizeof(uint64_t) * (MIPS64_REG_LAST - 1))) {
    return false;
  }

  // offset = siginfo offset + sizeof(siginfo) + uc_mcontext offset + sc_pc offset
  // read 64 bit sc_pc from stack into 64 bit regs_[MIPS64_REG_PC]
  if (!process_memory->Read(sp + 24 + 128 + 40 + 576, &regs_[MIPS64_REG_PC], sizeof(uint64_t))) {
    return false;
  }
  return true;
}

Regs* RegsMips64::Clone() {
  return new RegsMips64(*this);
}

}  // namespace unwindstack
