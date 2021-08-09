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

#define LOG_TAG "Corkscrew"

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unwind.h>
#include <stdio.h>

#include "backtrace-arch.h"
#include "backtrace-helper.h"
#include "ptrace-arch.h"
#include "map_info.h"
#include "ptrace.h"

ssize_t libudf_unwind_backtrace(backtrace_frame_t* backtrace, size_t ignore_depth, size_t max_depth) 
{
    ssize_t frames = -1;
    map_info_t* milist = acquire_my_map_info_list();
    frames = unwind_backtrace_signal_arch_selfnogcc(milist, backtrace, ignore_depth, max_depth);
    release_my_map_info_list(milist);
    return frames;
}