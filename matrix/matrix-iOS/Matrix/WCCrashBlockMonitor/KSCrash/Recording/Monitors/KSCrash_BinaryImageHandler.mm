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

#import <Foundation/Foundation.h>
#import "KSCrash_BinaryImageHandler.h"
#import "KSCrash.h"
#include <mach-o/dyld.h>
#include <dlfcn.h>
#include <pthread.h>

#define DYLD_IMAGE_MACOUNT 512 + 256
#define LOCK_THREAD pthread_mutex_lock(&m_threadLock);
#define UNLOCK_THREAD pthread_mutex_unlock(&m_threadLock);

struct ks_dyld_image_info {
    const struct mach_header *image_header;
    intptr_t vmaddr_slide;
    const char *image_name;
};

static struct ks_dyld_image_info image_list[DYLD_IMAGE_MACOUNT];
static uint32_t image_count = 0;
static pthread_mutex_t m_threadLock;
static NSMutableSet<NSNumber *> *image_header_set;

static void __add_dyld_image(const struct mach_header *header, intptr_t slide) {
    LOCK_THREAD

    // exceed max count
    if (image_count >= DYLD_IMAGE_MACOUNT) {
        UNLOCK_THREAD
        return;
    }

    // duplicate
    NSNumber *slide_number = [NSNumber numberWithUnsignedLong:(uintptr_t)header];
    if ([image_header_set containsObject:slide_number]) {
        UNLOCK_THREAD
        return;
    }

    Dl_info info = { 0 };
    if (dladdr(header, &info) != 0 && info.dli_fname) {
        int len = (int)strlen(info.dli_fname) + 1; // add '\0'
        struct ks_dyld_image_info image;
        image.image_header = header;
        image.vmaddr_slide = slide;
        image.image_name = (const char *)malloc(len);
        strncpy((char *)image.image_name, info.dli_fname, len);
        image_list[image_count++] = image;
        [image_header_set addObject:[NSNumber numberWithUnsignedLong:(uintptr_t)header]];
        UNLOCK_THREAD
        return;
    }

    UNLOCK_THREAD
}

static void __new_image_add_callback(const struct mach_header *header, intptr_t slide) {
    __add_dyld_image(header, slide);
}

void kscrash_record_current_all_images(void) {
    // 1. init variable
    image_header_set = [[NSMutableSet alloc] init];
    image_count = 0;
    pthread_mutex_init(&m_threadLock, NULL);

    // 2. register add image
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        _dyld_register_func_for_add_image(__new_image_add_callback);
    });
}

#pragma mark - C/OC Bridge
uint32_t __ks_dyld_image_count(void) {
    LOCK_THREAD
    uint32_t dyld_image_count = image_count;
    UNLOCK_THREAD

    return dyld_image_count;
}

const struct mach_header *__ks_dyld_get_image_header(uint32_t image_index) {
    LOCK_THREAD
    if (image_index >= image_count) {
        UNLOCK_THREAD
        return NULL;
    }
    struct ks_dyld_image_info &image_info = image_list[image_index];
    UNLOCK_THREAD

    return image_info.image_header;
}

intptr_t __ks_dyld_get_image_vmaddr_slide(uint32_t image_index) {
    LOCK_THREAD
    if (image_index >= image_count) {
        UNLOCK_THREAD
        return 0;
    }
    struct ks_dyld_image_info &image_info = image_list[image_index];
    UNLOCK_THREAD

    return image_info.vmaddr_slide;
}

const char *__ks_dyld_get_image_name(uint32_t image_index) {
    LOCK_THREAD
    if (image_index >= image_count) {
        UNLOCK_THREAD
        return NULL;
    }
    struct ks_dyld_image_info &image_info = image_list[image_index];
    UNLOCK_THREAD

    return image_info.image_name;
}

uintptr_t __ks_first_cmd_after_header(const struct mach_header *const header) {
    switch (header->magic) {
        case MH_MAGIC:
        case MH_CIGAM:
            return (uintptr_t)(header + 1);
        case MH_MAGIC_64:
        case MH_CIGAM_64:
            return (uintptr_t)(((struct mach_header_64 *)header) + 1);
        default:
            // Header is corrupt
            return 0;
    }
}
