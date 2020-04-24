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
#include <string.h>

#include <functional>

#include <unwindstack/Elf.h>
#include <unwindstack/MachineMips.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Memory.h>
#include <unwindstack/RegsMips.h>
#include <unwindstack/UcontextMips.h>
#include <unwindstack/UserMips.h>

namespace unwindstack {

RegsMips::RegsMips()
    : RegsImpl<uint32_t>(MIPS_REG_LAST, Location(LOCATION_REGISTER, MIPS_REG_RA)) {}

ArchEnum RegsMips::Arch() {
  return ARCH_MIPS;
}

uint64_t RegsMips::pc() {
  return regs_[MIPS_REG_PC];
}

uint64_t RegsMips::sp() {
  return regs_[MIPS_REG_SP];
}

void RegsMips::set_pc(uint64_t pc) {
  regs_[MIPS_REG_PC] = static_cast<uint32_t>(pc);
}

void RegsMips::set_sp(uint64_t sp) {
  regs_[MIPS_REG_SP] = static_cast<uint32_t>(sp);
}

uint64_t RegsMips::GetPcAdjustment(uint64_t rel_pc, Elf*) {
  if (rel_pc < 8) {
    return 0;
  }
  // For now, just assume no compact branches
  return 8;
}

bool RegsMips::SetPcFromReturnAddress(Memory*) {
  uint32_t ra = regs_[MIPS_REG_RA];
  if (regs_[MIPS_REG_PC] == ra) {
    return false;
  }

  regs_[MIPS_REG_PC] = ra;
  return true;
}

void RegsMips::IterateRegisters(std::function<void(const char*, uint64_t)> fn) {
  fn("r0", regs_[MIPS_REG_R0]);
  fn("r1", regs_[MIPS_REG_R1]);
  fn("r2", regs_[MIPS_REG_R2]);
  fn("r3", regs_[MIPS_REG_R3]);
  fn("r4", regs_[MIPS_REG_R4]);
  fn("r5", regs_[MIPS_REG_R5]);
  fn("r6", regs_[MIPS_REG_R6]);
  fn("r7", regs_[MIPS_REG_R7]);
  fn("r8", regs_[MIPS_REG_R8]);
  fn("r9", regs_[MIPS_REG_R9]);
  fn("r10", regs_[MIPS_REG_R10]);
  fn("r11", regs_[MIPS_REG_R11]);
  fn("r12", regs_[MIPS_REG_R12]);
  fn("r13", regs_[MIPS_REG_R13]);
  fn("r14", regs_[MIPS_REG_R14]);
  fn("r15", regs_[MIPS_REG_R15]);
  fn("r16", regs_[MIPS_REG_R16]);
  fn("r17", regs_[MIPS_REG_R17]);
  fn("r18", regs_[MIPS_REG_R18]);
  fn("r19", regs_[MIPS_REG_R19]);
  fn("r20", regs_[MIPS_REG_R20]);
  fn("r21", regs_[MIPS_REG_R21]);
  fn("r22", regs_[MIPS_REG_R22]);
  fn("r23", regs_[MIPS_REG_R23]);
  fn("r24", regs_[MIPS_REG_R24]);
  fn("r25", regs_[MIPS_REG_R25]);
  fn("r26", regs_[MIPS_REG_R26]);
  fn("r27", regs_[MIPS_REG_R27]);
  fn("r28", regs_[MIPS_REG_R28]);
  fn("sp", regs_[MIPS_REG_SP]);
  fn("r30", regs_[MIPS_REG_R30]);
  fn("ra", regs_[MIPS_REG_RA]);
  fn("pc", regs_[MIPS_REG_PC]);
}

Regs* RegsMips::Read(void* remote_data) {
  mips_user_regs* user = reinterpret_cast<mips_user_regs*>(remote_data);
  RegsMips* regs = new RegsMips();
  uint32_t* reg_data = reinterpret_cast<uint32_t*>(regs->RawData());

  memcpy(regs->RawData(), &user->regs[MIPS32_EF_R0], (MIPS_REG_R31 + 1) * sizeof(uint32_t));

  reg_data[MIPS_REG_PC] = user->regs[MIPS32_EF_CP0_EPC];
  return regs;
}

Regs* RegsMips::CreateFromUcontext(void* ucontext) {
  mips_ucontext_t* mips_ucontext = reinterpret_cast<mips_ucontext_t*>(ucontext);

  RegsMips* regs = new RegsMips();
  // Copy 64 bit sc_regs over to 32 bit regs
  for (int i = 0; i < 32; i++) {
      (*regs)[MIPS_REG_R0 + i] = mips_ucontext->uc_mcontext.sc_regs[i];
  }
  (*regs)[MIPS_REG_PC] = mips_ucontext->uc_mcontext.sc_pc;
  return regs;
}

bool RegsMips::StepIfSignalHandler(uint64_t rel_pc, Elf* elf, Memory* process_memory) {
  uint64_t data;
  uint64_t offset = 0;
  Memory* elf_memory = elf->memory();
  // Read from elf memory since it is usually more expensive to read from
  // process memory.
  if (!elf_memory->ReadFully(rel_pc, &data, sizeof(data))) {
    return false;
  }

  // Look for the kernel sigreturn functions.
  // __vdso_rt_sigreturn:
  // 0x24021061     li  v0, 0x1061
  // 0x0000000c     syscall
  // __vdso_sigreturn:
  // 0x24021017     li  v0, 0x1017
  // 0x0000000c     syscall
  if (data == 0x0000000c24021061ULL) {
    // vdso_rt_sigreturn => read rt_sigframe
    // offset = siginfo offset + sizeof(siginfo) + uc_mcontext offset + sc_pc offset
    offset = 24 + 128 + 24 + 8;
  } else if (data == 0x0000000c24021017LL) {
    // vdso_sigreturn => read sigframe
    // offset = sigcontext offset + sc_pc offset
    offset = 24 + 8;
  } else {
    return false;
  }

  // read sc_pc and sc_regs[32] from stack
  uint64_t values[MIPS_REG_LAST];
  if (!process_memory->ReadFully(regs_[MIPS_REG_SP] + offset, values, sizeof(values))) {
    return false;
  }

  // Copy 64 bit sc_pc over to 32 bit regs_[MIPS_REG_PC]
  regs_[MIPS_REG_PC] = values[0];

  // Copy 64 bit sc_regs over to 32 bit regs
  for (int i = 0; i < 32; i++) {
      regs_[MIPS_REG_R0 + i] = values[1 + i];
  }
  return true;
}

Regs* RegsMips::Clone() {
  return new RegsMips(*this);
}

}  // namespace unwindstack
