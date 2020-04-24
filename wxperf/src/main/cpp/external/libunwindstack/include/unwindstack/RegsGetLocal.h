/*
 * Copyright (C) 2016 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _LIBUNWINDSTACK_REGS_GET_LOCAL_H
#define _LIBUNWINDSTACK_REGS_GET_LOCAL_H

namespace unwindstack {

#if defined(__arm__)

inline __attribute__((__always_inline__)) void AsmGetRegs(void* reg_data) {
  asm volatile(
      ".align 2\n"
      "bx pc\n"
      "nop\n"
      ".code 32\n"
      "stmia %[base], {r0-r12}\n"
      "add %[base], #52\n"
      "mov r1, r13\n"
      "mov r2, r14\n"
      "mov r3, r15\n"
      "stmia %[base], {r1-r3}\n"
      "orr %[base], pc, #1\n"
      "bx %[base]\n"
      : [base] "+r"(reg_data)
      :
      : "memory");
}

#elif defined(__aarch64__)

inline __attribute__((__always_inline__)) void AsmGetRegs(void* reg_data) {
  asm volatile(
      "1:\n"
      "stp x0, x1, [%[base], #0]\n"
      "stp x2, x3, [%[base], #16]\n"
      "stp x4, x5, [%[base], #32]\n"
      "stp x6, x7, [%[base], #48]\n"
      "stp x8, x9, [%[base], #64]\n"
      "stp x10, x11, [%[base], #80]\n"
      "stp x12, x13, [%[base], #96]\n"
      "stp x14, x15, [%[base], #112]\n"
      "stp x16, x17, [%[base], #128]\n"
      "stp x18, x19, [%[base], #144]\n"
      "stp x20, x21, [%[base], #160]\n"
      "stp x22, x23, [%[base], #176]\n"
      "stp x24, x25, [%[base], #192]\n"
      "stp x26, x27, [%[base], #208]\n"
      "stp x28, x29, [%[base], #224]\n"
      "str x30, [%[base], #240]\n"
      "mov x12, sp\n"
      "adr x13, 1b\n"
      "stp x12, x13, [%[base], #248]\n"
      : [base] "+r"(reg_data)
      :
      : "x12", "x13", "memory");
}

#elif defined(__i386__) || defined(__x86_64__) || defined(__mips__)

extern "C" void AsmGetRegs(void* regs);

#endif

inline __attribute__((__always_inline__)) void RegsGetLocal(Regs* regs) {
  AsmGetRegs(regs->RawData());
}


}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_REGS_GET_LOCAL_H
