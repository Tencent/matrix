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
#import <TargetConditionals.h>

@interface MatrixDeviceInfo : NSObject

+ (NSString *)getDeviceType;

+ (NSString *)systemName;
+ (NSString *)systemVersion;
+ (NSString *)model;
+ (NSString *)platform;
+ (int)cpuCount;
+ (int)cpuFrequency;
+ (float)cpuUsage;
+ (float)appCpuUsage;
+ (int)busFrequency;
+ (int)totalMemory __deprecated;
+ (int)userMemory __deprecated;
+ (int)cacheLine;
+ (int)L1ICacheSize;
+ (int)L1DCacheSize;
+ (int)L2CacheSize;
+ (int)L3CacheSize;

+ (BOOL)isBeingDebugged;

@end

#ifdef __cplusplus
extern "C" {
#endif

uint64_t matrix_physicalMemory(); // 设备的物理内存总量（bytes），来源：sysctlbyname("hw.memsize")
uint64_t matrix_footprintMemory(); // 进程已经使用的内存量（bytes），来源：task_vm_info.phys_footprint
uint64_t matrix_availableMemory(); // 进程剩余可用的内存量（bytes），来源：os_proc_available_memory

#ifdef __cplusplus
} // extern "C"
#endif
