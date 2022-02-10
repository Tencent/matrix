/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <mach-o/loader.h>

void kscrash_record_current_all_images(void);

uint32_t __ks_dyld_image_count(void);
const struct mach_header *__ks_dyld_get_image_header(uint32_t image_index);
intptr_t __ks_dyld_get_image_vmaddr_slide(uint32_t image_index);
const char *__ks_dyld_get_image_name(uint32_t image_index);
uintptr_t __ks_first_cmd_after_header(const struct mach_header *const header);
#ifdef __cplusplus
}
#endif
