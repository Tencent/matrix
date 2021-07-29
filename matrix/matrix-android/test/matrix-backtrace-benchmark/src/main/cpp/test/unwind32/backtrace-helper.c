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
//#define LOG_NDEBUG 0

#include "backtrace-helper.h"

backtrace_frame_t* add_backtrace_entry(uintptr_t pc, backtrace_frame_t* backtrace,
        size_t ignore_depth, size_t max_depth,
        size_t* ignored_frames, size_t* returned_frames) {
    if (*ignored_frames < ignore_depth) {
        *ignored_frames += 1;
        return NULL;
    }
    if (*returned_frames >= max_depth) {
        return NULL;
    }
    backtrace_frame_t* frame = &backtrace[*returned_frames];
    *frame = pc;
    *returned_frames += 1;
    return frame;
}
