//
//  KSCrash_BinaryImageHandler.m
//  MatrixiOS
//
//  Created by alanllwang on 2020/5/27.
//  Copyright © 2020 wechat. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "KSCrash_BinaryImageHandler.h"
#import "KSCrash.h"
#include <mach-o/dyld.h>
#include <dlfcn.h>
#include <pthread.h>

#define DYLD_IMAGE_MACOUNT 512+256
#define LOCK_THREAD pthread_mutex_lock(&m_threadLock);
#define UNLOCK_THREAD pthread_mutex_unlock(&m_threadLock);

struct ks_dyld_image_info {
    const struct mach_header* image_header;
    intptr_t                  vmaddr_slide;
    const char*               image_name;
};

static struct ks_dyld_image_info image_list[DYLD_IMAGE_MACOUNT];
static uint32_t image_count = 0;
static pthread_mutex_t m_threadLock;
static NSMutableSet<NSNumber *> *image_header_set;

static void __add_dyld_image(const struct mach_header *header, intptr_t slide)
{
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
    
    Dl_info info = {0};
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

static void __new_image_add_callback(const struct mach_header *header, intptr_t slide)
{
    __add_dyld_image(header, slide);
}

void kscrash_record_current_all_images(void)
{
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
uint32_t __ks_dyld_image_count(void)
{
    LOCK_THREAD
    uint32_t dyld_image_count = image_count;
    UNLOCK_THREAD
    
    return dyld_image_count;
}

const struct mach_header *__ks_dyld_get_image_header(uint32_t image_index)
{
    LOCK_THREAD
    if (image_index >= image_count) {
        UNLOCK_THREAD
        return NULL;
    }
    struct ks_dyld_image_info image_info = image_list[image_index];
    const struct mach_header *image_mach_header = image_info.image_header;
    UNLOCK_THREAD
    return image_mach_header;
}

intptr_t __ks_dyld_get_image_vmaddr_slide(uint32_t image_index)
{
    LOCK_THREAD
    if (image_index >= image_count) {
        UNLOCK_THREAD
        return 0;
    }
    struct ks_dyld_image_info image_info = image_list[image_index];
    intptr_t image_vmaddr_slide = image_info.vmaddr_slide;
    UNLOCK_THREAD
    return image_vmaddr_slide;
}

const char* __ks_dyld_get_image_name(uint32_t image_index)
{
    LOCK_THREAD
    if (image_index >= image_count) {
        UNLOCK_THREAD
        return NULL;
    }
    struct ks_dyld_image_info image_info = image_list[image_index];
    const char* image_name = image_info.image_name;
    UNLOCK_THREAD
    return image_name;
}
