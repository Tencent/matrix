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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

#include "libudf_unwind_p.h"
#include "map_info.h"

const map_info_t* find_map_info(const map_info_t* milist, uintptr_t addr) {
    const map_info_t* mi = milist;
    while (mi && !(addr >= mi->start && addr < mi->end)) {
        mi = mi->next;
    }
    return mi;
}

bool is_readable_map(const map_info_t* milist, uintptr_t addr) {
    const map_info_t* mi = find_map_info(milist, addr);
    return mi && mi->is_readable;
}

bool is_writable_map(const map_info_t* milist, uintptr_t addr) {
    const map_info_t* mi = find_map_info(milist, addr);
    return mi && mi->is_writable;
}

bool is_executable_map(const map_info_t* milist, uintptr_t addr) {
    const map_info_t* mi = find_map_info(milist, addr);
    return mi && mi->is_executable;
}

map_info_t* acquire_my_map_info_list() {
    return NULL;
}

void release_my_map_info_list(map_info_t* milist __attribute__((unused))) {
}
