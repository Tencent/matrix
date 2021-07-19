/*
 * Copyright (C) 2011 The Android Open Source Project
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

/* Architecture dependent functions. */

#ifndef _CORKSCREW_BACKTRACE_ARCH_H
#define _CORKSCREW_BACKTRACE_ARCH_H

#include <signal.h>

#include "ptrace-arch.h"
#include "backtrace.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Rewind the program counter by one instruction. */
uintptr_t rewind_pc_arch(const memory_t* memory, uintptr_t pc);

ssize_t unwind_backtrace_signal_arch(siginfo_t* siginfo, void* sigcontext,
        const map_info_t* map_info_list,
        backtrace_frame_t* backtrace, size_t ignore_depth, size_t max_depth);

ssize_t unwind_backtrace_ptrace_arch(pid_t tid, const ptrace_context_t* context,
        backtrace_frame_t* backtrace, size_t ignore_depth, size_t max_depth);

#ifdef __cplusplus
}
#endif

#endif // _CORKSCREW_BACKTRACE_ARCH_H
