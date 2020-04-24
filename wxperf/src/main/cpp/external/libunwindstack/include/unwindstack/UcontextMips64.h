/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef _LIBUNWINDSTACK_UCONTEXT_MIPS64_H
#define _LIBUNWINDSTACK_UCONTEXT_MIPS64_H

#include <stdint.h>

#include <unwindstack/MachineMips64.h>

namespace unwindstack {

struct mips64_stack_t {
  uint64_t ss_sp;    // void __user*
  uint64_t ss_size;  // size_t
  int32_t ss_flags;  // int
};

struct mips64_mcontext_t {
  uint64_t sc_regs[32];
  uint64_t sc_fpregs[32];
  uint64_t sc_mdhi;
  uint64_t sc_hi1;
  uint64_t sc_hi2;
  uint64_t sc_hi3;
  uint64_t sc_mdlo;
  uint64_t sc_lo1;
  uint64_t sc_lo2;
  uint64_t sc_lo3;
  uint64_t sc_pc;
  // Nothing else is used, so don't define it.
};

struct mips64_ucontext_t {
  uint64_t uc_flags;  // unsigned long
  uint64_t uc_link;   // struct ucontext*
  mips64_stack_t uc_stack;
  mips64_mcontext_t uc_mcontext;
  // Nothing else is used, so don't define it.
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_UCONTEXT_MIPS64_H
