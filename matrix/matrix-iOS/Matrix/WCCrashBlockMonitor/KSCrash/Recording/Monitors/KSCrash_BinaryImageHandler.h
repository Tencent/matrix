//
//  KSCrash_BinaryImageHandler.h
//  MatrixiOS
//
//  Created by alanllwang on 2020/5/27.
//  Copyright © 2020 wechat. All rights reserved.
//

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
const char* __ks_dyld_get_image_name(uint32_t image_index);

#ifdef __cplusplus
}
#endif
