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

/* Useful ptrace() utility functions. */

#ifndef _CORKSCREW_PTRACE_H
#define _CORKSCREW_PTRACE_H

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>

#include "map_info.h"


#ifdef __cplusplus
extern "C" {
#endif

/* Stores information about a process that is used for several different
 * ptrace() based operations. */
typedef struct {
    map_info_t* map_info_list;
} ptrace_context_t;

/* Describes how to access memory from a process. */
typedef struct {
    pid_t tid;
    const map_info_t* map_info_list;
} memory_t;

#if __i386__
/* ptrace() register context. */
typedef struct pt_regs_x86 {
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint32_t eax;
    uint32_t xds;
    uint32_t xes;
    uint32_t xfs;
    uint32_t xgs;
    uint32_t orig_eax;
    uint32_t eip;
    uint32_t xcs;
    uint32_t eflags;
    uint32_t esp;
    uint32_t xss;
} pt_regs_x86_t;
#endif

#if __mips__
/* ptrace() GET_REGS context. */
typedef struct pt_regs_mips {
    uint64_t regs[32];
    uint64_t lo;
    uint64_t hi;
    uint64_t cp0_epc;
    uint64_t cp0_badvaddr;
    uint64_t cp0_status;
    uint64_t cp0_cause;
} pt_regs_mips_t;
#endif

/*
 * Initializes a memory structure for accessing memory from this process.
 */
void init_memory(memory_t* memory, const map_info_t* map_info_list);

/*
 * Initializes a memory structure for accessing memory from another process
 * using ptrace().
 */
void init_memory_ptrace(memory_t* memory, pid_t tid);

/*
 * Reads a word of memory safely.
 * If the memory is local, ensures that the address is readable before dereferencing it.
 * Returns false and a value of 0xffffffff if the word could not be read.
 */
bool try_get_word(const memory_t* memory, uintptr_t ptr, uint32_t* out_value);

bool try_get_word_stack(uintptr_t ptr, uint32_t* out_value);

#ifdef __cplusplus
}
#endif

#endif // _CORKSCREW_PTRACE_H
