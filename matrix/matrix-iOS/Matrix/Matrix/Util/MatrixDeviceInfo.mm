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

#import "MatrixDeviceInfo.h"
#import "MatrixLogDef.h"

#include <sys/sysctl.h>
#include <sys/mount.h>
#include <mach/mach_error.h>
#include <mach/mach_host.h>
#include <mach/mach_port.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#include <mach/vm_map.h>
#include <os/proc.h>

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>
#endif

#define kIPadSystemNamePrefix @"iPad "

@implementation MatrixDeviceInfo

+ (BOOL)isiPad {
    static BOOL s_isiPad = NO;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSString *nsModel = [MatrixDeviceInfo model];
        s_isiPad = [nsModel hasPrefix:@"iPad"];
    });
    return s_isiPad;
}

+ (NSString *)SystemNameForDeviceType {
    NSString *systemName = [MatrixDeviceInfo systemName];
    if ([MatrixDeviceInfo isiPad]) {
        systemName = [kIPadSystemNamePrefix stringByAppendingString:systemName];
    }
    return systemName;
}

+ (NSString *)getDeviceType {
    static NSString *deviceType = @"";
    if (deviceType.length == 0) {
        NSString *systemName = [self SystemNameForDeviceType];
        NSString *systemVersion = [MatrixDeviceInfo systemVersion];
        deviceType = [NSString stringWithFormat:@"%@%@", systemName, systemVersion];
    }
    return deviceType;
}

+ (NSString *)getSysInfoByName:(char *)typeSpeifier {
    size_t size;
    sysctlbyname(typeSpeifier, NULL, &size, NULL, 0);
    char *answer = (char *)malloc(size);
    sysctlbyname(typeSpeifier, answer, &size, NULL, 0);
    NSString *results = [NSString stringWithCString:answer encoding:NSUTF8StringEncoding];
    if (results == nil) {
        results = @"";
    }
    free(answer);
    return results;
}

+ (int)getSysInfo:(uint)typeSpecifier {
    size_t size = sizeof(int);
    int results;
    int mib[2] = { CTL_HW, (int)typeSpecifier };
    sysctl(mib, 2, &results, &size, NULL, 0);
    return results;
}

+ (NSString *)systemName {
#if !TARGET_OS_OSX
    return [UIDevice currentDevice].systemName;
#else
    NSProcessInfo *pInfo = [NSProcessInfo processInfo];
    return [pInfo operatingSystemVersionString];
#endif
}

+ (NSString *)systemVersion {
#if !TARGET_OS_OSX
    return [UIDevice currentDevice].systemVersion;
#else
    static NSString *g_s_systemVersion = nil;
    if (g_s_systemVersion == nil) {
        NSDictionary *sv = [NSDictionary dictionaryWithContentsOfFile:@"/System/Library/CoreServices/SystemVersion.plist"];
        NSString *productVersion = [sv objectForKey:@"ProductVersion"];
        NSString *productBuildVersion = [sv objectForKey:@"ProductBuildVersion"];
        g_s_systemVersion = [NSString stringWithFormat:@"OSX %@ build(%@)", productVersion, productBuildVersion];
    }
    return g_s_systemVersion;
#endif
}

+ (NSString *)model {
#if !TARGET_OS_OSX
    return [UIDevice currentDevice].model;
#endif
    return [MatrixDeviceInfo getSysInfoByName:(char *)"hw.model"];
}

+ (NSString *)platform {
    return [MatrixDeviceInfo getSysInfoByName:(char *)"hw.machine"];
}

+ (int)cpuCount {
    static int s_cpuCount = 0;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        s_cpuCount = [MatrixDeviceInfo getSysInfo:HW_NCPU];
    });
    return s_cpuCount;
}

+ (int)cpuFrequency {
    return [MatrixDeviceInfo getSysInfo:HW_CPU_FREQ];
}

+ (float)cpuUsage {
    kern_return_t kr;
    mach_msg_type_number_t count;
    static host_cpu_load_info_data_t previous_info = { 0, 0, 0, 0 };
    host_cpu_load_info_data_t info;

    count = HOST_CPU_LOAD_INFO_COUNT;

    kr = host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&info, &count);
    if (kr != KERN_SUCCESS) {
        return 0;
    }

    natural_t user = info.cpu_ticks[CPU_STATE_USER] - previous_info.cpu_ticks[CPU_STATE_USER];
    natural_t nice = info.cpu_ticks[CPU_STATE_NICE] - previous_info.cpu_ticks[CPU_STATE_NICE];
    natural_t system = info.cpu_ticks[CPU_STATE_SYSTEM] - previous_info.cpu_ticks[CPU_STATE_SYSTEM];
    natural_t idle = info.cpu_ticks[CPU_STATE_IDLE] - previous_info.cpu_ticks[CPU_STATE_IDLE];
    natural_t total = user + nice + system + idle;
    previous_info = info;

    if (total == 0) {
        return 0;
    } else {
        return (user + nice + system) * 100.0 / total;
    }
}

//+ (float)cpuUsage {
//    CGFloat cpuUsage = 0;
//    unsigned _numCPUs;
//    static processor_info_array_t _cpuInfo = NULL, _prevCPUInfo = NULL;
//    static mach_msg_type_number_t _numCPUInfo, _numPrevCPUInfo;
//
//    int _mib[2U] = { CTL_HW, HW_NCPU };
//    size_t _sizeOfNumCPUs = sizeof(_numCPUs);
//    int _status = sysctl(_mib, 2U, &_numCPUs, &_sizeOfNumCPUs, NULL, 0U);
//    if (_status) {
//        _numCPUs = 1;
//    }
//
//    natural_t _numCPUsU = 0U;
//    kern_return_t err = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &_numCPUsU, &_cpuInfo, &_numCPUInfo);
//    if (err == KERN_SUCCESS) {
//        for (unsigned i = 0U; i < _numCPUs; ++i) {
//            CGFloat _inUse, _total = 0;
//            if (_prevCPUInfo) {
//                _inUse = ((_cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_USER] - _prevCPUInfo[(CPU_STATE_MAX * i) + CPU_STATE_USER])
//                          + (_cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_SYSTEM] - _prevCPUInfo[(CPU_STATE_MAX * i) + CPU_STATE_SYSTEM])
//                          + (_cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_NICE] - _prevCPUInfo[(CPU_STATE_MAX * i) + CPU_STATE_NICE]));
//                _total = _inUse + (_cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_IDLE] - _prevCPUInfo[(CPU_STATE_MAX * i) + CPU_STATE_IDLE]);
//            } else {
//                _inUse = _cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_USER] + _cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_SYSTEM]
//                         + _cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_NICE];
//                _total = _inUse + _cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_IDLE];
//            }
//
//            if (_total != 0) {
//                cpuUsage += _inUse / _total;
//            }
//        }
//
//        if (_prevCPUInfo) {
//            size_t prevCpuInfoSize = sizeof(integer_t) * _numPrevCPUInfo;
//            vm_deallocate(mach_task_self(), (vm_address_t)_prevCPUInfo, prevCpuInfoSize);
//        }
//
//        _prevCPUInfo = _cpuInfo;
//        _numPrevCPUInfo = _numCPUInfo;
//
//        _cpuInfo = NULL;
//        _numCPUInfo = 0U;
//
//        return cpuUsage * 100.0;
//    } else {
//        return -1;
//    }
//}

+ (float)appCpuUsage {
    const task_t thisTask = mach_task_self();
    thread_array_t thread_list;
    mach_msg_type_number_t thread_count;
    kern_return_t kr = task_threads(thisTask, &thread_list, &thread_count);
    if (kr != KERN_SUCCESS) {
        return -1;
    }

    float tot_cpu = 0;

    for (int j = 0; j < thread_count; j++) {
        thread_info_data_t thinfo;
        mach_msg_type_number_t thread_info_count = THREAD_INFO_MAX;
        kr = thread_info(thread_list[j], THREAD_BASIC_INFO, (thread_info_t)thinfo, &thread_info_count);
        if (kr != KERN_SUCCESS) {
            tot_cpu = -1;
            goto cleanup;
        }
        thread_basic_info_t basic_info_th = (thread_basic_info_t)thinfo;
        if (!(basic_info_th->flags & TH_FLAGS_IDLE)) {
            tot_cpu = tot_cpu + basic_info_th->cpu_usage / (float)TH_USAGE_SCALE * 100.0;
        }
    }

cleanup:
    for (int i = 0; i < thread_count; i++) {
        mach_port_deallocate(thisTask, thread_list[i]);
    }

    kr = vm_deallocate(thisTask, (vm_offset_t)thread_list, thread_count * sizeof(thread_t));
    assert(kr == KERN_SUCCESS);

    return tot_cpu;
}

+ (int)busFrequency {
    return [MatrixDeviceInfo getSysInfo:HW_BUS_FREQ];
}

+ (int)totalMemory {
    return [MatrixDeviceInfo getSysInfo:HW_PHYSMEM];
}

+ (int)userMemory {
    return [MatrixDeviceInfo getSysInfo:HW_USERMEM];
}

+ (int)cacheLine {
    return [MatrixDeviceInfo getSysInfo:HW_CACHELINE];
}

+ (int)L1ICacheSize {
    return [MatrixDeviceInfo getSysInfo:HW_L1ICACHESIZE];
}

+ (int)L1DCacheSize {
    return [MatrixDeviceInfo getSysInfo:HW_L1DCACHESIZE];
}

+ (int)L2CacheSize {
    return [MatrixDeviceInfo getSysInfo:HW_L2CACHESIZE];
}

+ (int)L3CacheSize {
    return [MatrixDeviceInfo getSysInfo:HW_L3CACHESIZE];
}

+ (BOOL)isBeingDebugged {
    // Returns true if the current process is being debugged (either
    // running under the debugger or has a debugger attached post facto).
    int junk;
    int mib[4];
    struct kinfo_proc info;
    size_t size;

    // Initialize the flags so that, if sysctl fails for some bizarre
    // reason, we get a predictable result.

    info.kp_proc.p_flag = 0;

    // Initialize mib, which tells sysctl the info we want, in this case
    // we're looking for information about a specific process ID.

    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = getpid();

    // Call sysctl.

    size = sizeof(info);
    junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);
    assert(junk == 0);

    // We're being debugged if the P_TRACED flag is set.
    return ((info.kp_proc.p_flag & P_TRACED) != 0);
}

@end

uint64_t matrix_physicalMemory() {
    uint64_t value;
    size_t size = sizeof(value);

    // same as [[NSProcessInfo processInfo] physicalMemory]
    int ret = sysctlbyname("hw.memsize", &value, &size, NULL, 0);
    if (ret != 0) {
        // 不用kssysctl_uint64ForName，因为出错时KSLOG_ERROR可能会调用malloc
        return 0;
    }

    return value;
}

uint64_t matrix_footprintMemory() {
    task_vm_info_data_t vmInfo;
    mach_msg_type_number_t infoCount = TASK_VM_INFO_COUNT;
    kern_return_t kernReturn = task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&vmInfo, &infoCount);

    if (kernReturn != KERN_SUCCESS) {
        // MatrixError会调用ObjC方法
        // MatrixError(@"Error with task_info(): %s", mach_error_string(kernReturn));
        return 0;
    }

    return vmInfo.phys_footprint;
}

uint64_t matrix_availableMemory() {
#if !TARGET_OS_OSX
    if (@available(iOS 13.0, *)) {
        return os_proc_available_memory();
    }
#endif

    // Fallback on earlier versions

    vm_statistics64_data_t vmStats;
    mach_msg_type_number_t infoCount = HOST_VM_INFO_COUNT;
    kern_return_t kernReturn = host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&vmStats, &infoCount);

    if (kernReturn != KERN_SUCCESS) {
        // MatrixError会调用ObjC方法
        // MatrixError(@"Error with host_statistics(): %s", mach_error_string(kernReturn));
        return 0;
    }

    return (uint64_t)vm_page_size * (vmStats.free_count + vmStats.inactive_count);
}
