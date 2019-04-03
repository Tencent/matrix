/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef dyld_image_info_h
#define dyld_image_info_h

#include <mach/vm_types.h>

/**
 * The filtering strategy of the stack
 *
 * If the malloc size more than 'skipMinMallocSize', the stack will be saved.
 * Otherwise if the stack contains App's symbols in the last 'skipMaxStackDepth' address,
 * the stack also be saved.
 */
extern int skip_max_stack_depth;
extern int skip_min_malloc_size;

extern bool is_ios9_plus;

struct dyld_image_info_file;

bool prepare_dyld_image_logger(const char *event_dir);
bool is_stack_frames_should_skip(uintptr_t *frames, int32_t count, uint64_t malloc_size, uint32_t type_flags);
const char *app_uuid();

dyld_image_info_file *open_dyld_image_info_file(const char *event_dir);
void close_dyld_image_info_file(dyld_image_info_file *file_context);

void transform_frames(dyld_image_info_file *file_context, uint64_t *src_frames, uint64_t *out_offsets, char const **out_uuids, char const **out_image_names, bool *out_is_app_images, int32_t count);

#endif /* dyld_image_info_h */
