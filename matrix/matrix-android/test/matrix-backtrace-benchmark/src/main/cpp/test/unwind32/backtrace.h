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

/* A stack unwinder. */

#ifndef _CORKSCREW_BACKTRACE_H
#define _CORKSCREW_BACKTRACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include "ptrace.h"
#include "map_info.h"

/*
 * Describes a single frame of a backtrace.
 */
typedef uintptr_t backtrace_frame_t;

/*
 * Unwinds the call stack for the current thread of execution.
 * Populates the backtrace array with the program counters from the call stack.
 * Returns the number of frames collected, or -1 if an error occurred.
 */
__attribute__((visibility("default")))
ssize_t libudf_unwind_backtrace(backtrace_frame_t* backtrace, size_t ignore_depth, size_t max_depth);

ssize_t libudf_unwind_backtrace_gcc(backtrace_frame_t* backtrace, size_t ignore_depth, size_t max_depth);

/*
 * Unwinds the call stack of a task within a remote process using ptrace().
 * Populates the backtrace array with the program counters from the call stack.
 * Returns the number of frames collected, or -1 if an error occurred.
 */
ssize_t libudf_unwind_backtrace_ptrace(pid_t tid, const ptrace_context_t* context,
        backtrace_frame_t* backtrace, size_t ignore_depth, size_t max_depth);

#ifdef __cplusplus
}
#endif

#endif // _CORKSCREW_BACKTRACE_H
