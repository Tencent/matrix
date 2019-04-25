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

#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#import "nsobject_hook.h"
#import "object_event_handler.h"
#include <pthread/pthread.h>

#if __has_feature(objc_arc)
#error This file must be compiled with MRR. Use -fno-objc-arc flag.
#endif

extern mach_port_t g_matrix_block_monitor_dumping_thread_id;

#pragma mark -
#pragma mark NSObject ObjectEventLogging

@interface NSObject (ObjectEventLogging)

+ (id)event_logging_alloc;

@end

@implementation NSObject (ObjectEventLogging)

+ (id)event_logging_alloc
{
	id object = [self event_logging_alloc];
    if (pthread_mach_thread_np(pthread_self()) == g_matrix_block_monitor_dumping_thread_id) {
        return object;
    }
    nsobject_set_last_allocation_event_name(object, class_getName(self.class));
	return object;
}

@end

