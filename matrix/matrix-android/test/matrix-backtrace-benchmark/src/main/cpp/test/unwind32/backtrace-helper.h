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

/* Backtrace helper functions. */

#ifndef _CORKSCREW_BACKTRACE_HELPER_H
#define _CORKSCREW_BACKTRACE_HELPER_H

#include <stdbool.h>
#include <sys/types.h>
#include "backtrace.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Add a program counter to a backtrace if it will fit.
 * Returns the newly added frame, or NULL if none.
 */
backtrace_frame_t* add_backtrace_entry(uintptr_t pc,
        backtrace_frame_t* backtrace,
        size_t ignore_depth, size_t max_depth,
        size_t* ignored_frames, size_t* returned_frames);

#ifdef __cplusplus
}
#endif

#endif // _CORKSCREW_BACKTRACE_HELPER_H
