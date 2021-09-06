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

#ifndef _CORKSCREW_PTRACE_ARCH_H
#define _CORKSCREW_PTRACE_ARCH_H

#include "ptrace.h"
#include "map_info.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Custom extra data we stuff into map_info_t structures as part
 * of our ptrace_context_t. */
typedef struct {
#ifdef __arm__
    uintptr_t exidx_start;
    size_t exidx_size;
#elif __i386__
    uintptr_t eh_frame_hdr;
#endif
} map_info_data_t;

#ifdef __cplusplus
}
#endif

#endif // _CORKSCREW_PTRACE_ARCH_H
