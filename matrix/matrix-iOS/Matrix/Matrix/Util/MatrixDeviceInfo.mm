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

#include <sys/sysctl.h>
#include <sys/mount.h>

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>
#endif

#define kIPadSystemNamePrefix @"iPad "

@implementation MatrixDeviceInfo

+ (BOOL)isiPad
{
    static BOOL s_isiPad = NO;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSString *nsModel = [MatrixDeviceInfo model];
        s_isiPad = [nsModel hasPrefix:@"iPad"];
    });
    return s_isiPad;
}

+ (NSString *)SystemNameForDeviceType
{
    NSString *systemName = [MatrixDeviceInfo systemName];
    if ([MatrixDeviceInfo isiPad]) {
        systemName = [kIPadSystemNamePrefix stringByAppendingString:systemName];
    }
    return systemName;
}

+ (NSString *)getDeviceType
{
    static NSString *deviceType = @"";
    if (deviceType.length == 0) {
        NSString *systemName = [self SystemNameForDeviceType];
        NSString *systemVersion = [MatrixDeviceInfo systemVersion];
        deviceType = [NSString stringWithFormat:@"%@%@", systemName, systemVersion];
    }
    return deviceType;
}

+ (NSString *)getSysInfoByName:(char *)typeSpeifier
{
    size_t size;
    sysctlbyname(typeSpeifier, NULL, &size, NULL, 0);
    char *answer = (char *) malloc(size);
    sysctlbyname(typeSpeifier, answer, &size, NULL, 0);
    NSString *results = [NSString stringWithCString:answer encoding:NSUTF8StringEncoding];
    if (results == nil) {
        results = @"";
    }
    free(answer);
    return results;
}

+ (int)getSysInfo:(uint)typeSpecifier
{
    size_t size = sizeof(int);
    int results;
    int mib[2] = {CTL_HW, (int) typeSpecifier};
    sysctl(mib, 2, &results, &size, NULL, 0);
    return results;
}

+ (NSString *)systemName
{
#if !TARGET_OS_OSX
    return [UIDevice currentDevice].systemName;
#else
    NSProcessInfo *pInfo = [NSProcessInfo processInfo];
    return [pInfo operatingSystemVersionString];
#endif
}


+ (NSString *)systemVersion
{
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

+ (NSString *)model
{
#if !TARGET_OS_OSX
    return [UIDevice currentDevice].model;
#endif
    return [MatrixDeviceInfo getSysInfoByName:(char *)"hw.model"];
}

+ (NSString *)platform
{
    return [MatrixDeviceInfo getSysInfoByName:(char *)"hw.machine"];
}

+ (int)cpuCount
{
    return [MatrixDeviceInfo getSysInfo:HW_NCPU];
}

+ (int)cpuFrequency
{
    return [MatrixDeviceInfo getSysInfo:HW_CPU_FREQ];
}

+ (int)busFrequency
{
    return [MatrixDeviceInfo getSysInfo:HW_BUS_FREQ];
}

+ (int)totalMemory
{
    return [MatrixDeviceInfo getSysInfo:HW_PHYSMEM];
}

+ (int)userMemory
{
    return [MatrixDeviceInfo getSysInfo:HW_USERMEM];
}

+ (int)cacheLine
{
    return [MatrixDeviceInfo getSysInfo:HW_CACHELINE];
}

+ (int)L1ICacheSize
{
    return [MatrixDeviceInfo getSysInfo:HW_L1ICACHESIZE];
}

+ (int)L1DCacheSize
{
    return [MatrixDeviceInfo getSysInfo:HW_L1DCACHESIZE];
}

+ (int)L2CacheSize
{
    return [MatrixDeviceInfo getSysInfo:HW_L2CACHESIZE];
}

+ (int)L3CacheSize
{
    return [MatrixDeviceInfo getSysInfo:HW_L3CACHESIZE];
}

+ (BOOL)isBeingDebugged
{
    // Returns true if the current process is being debugged (either
	// running under the debugger or has a debugger attached post facto).
	int                 junk;
	int                 mib[4];
	struct kinfo_proc   info;
	size_t              size;
	
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
