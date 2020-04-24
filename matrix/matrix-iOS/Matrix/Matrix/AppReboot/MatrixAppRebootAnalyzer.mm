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

#import <TargetConditionals.h>

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

#import "MatrixDeviceInfo.h"
#import "MatrixAppRebootAnalyzer.h"
#import "MatrixDeviceInfo.h"
#import "MatrixLogDef.h"
#import "MatrixPathUtil.h"
#import "dyld_image_info.h"
#import "WCCrashBlockFileHandler.h"
#import "WCCrashBlockJsonUtil.h"

// ============================================================================
#pragma mark - MatrixAppRebootInfo
// ============================================================================

@implementation MatrixAppRebootInfo

+ (MatrixAppRebootInfo *)sharedInstance
{
    static MatrixAppRebootInfo *s_instance;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        @try {
            s_instance = [NSKeyedUnarchiver unarchiveObjectWithFile:[MatrixAppRebootInfo infoPath]];
        } @catch (NSException *exception) {

        } @finally {
            if (s_instance == nil) {
                s_instance = [[MatrixAppRebootInfo alloc] init];
            }
        }
    });

    return s_instance;
}

- (void)saveInfo
{
    @try {
        [NSKeyedArchiver archiveRootObject:self toFile:[MatrixAppRebootInfo infoPath]];
    } @catch (NSException *exception) {
    }
}

+ (NSString *)infoPath
{
    return [[MatrixPathUtil appRebootAnalyzerCachePath] stringByAppendingPathComponent:@"AppReboot.dat"];
}

@end

// ============================================================================
#pragma mark - MatrixAppRebootAnalyzer
// ============================================================================

#define CHECK_DEBUGGING                       \
    if ([MatrixDeviceInfo isBeingDebugged]) { \
        MatrixInfo(@"app is being debugged"); \
        return;                               \
    }

static MatrixAppRebootType s_rebootType = MatrixAppRebootTypeBegin;
static NSString *s_lastUserScene = @"";
static NSString *s_lastDumpFileName = @"";
static uint64_t s_lastAppLaunchTime = 0;
static BOOL s_lastTimeDeviceReboot = NO;
static dispatch_block_t s_suspendDelayBlock = nil;
static dispatch_block_t s_foregroundDelayBlock = nil;
static BOOL s_isSuspendKilled = NO;

@implementation MatrixAppRebootAnalyzer

+ (void)installAppRebootAnalyzer
{
    CHECK_DEBUGGING;
#if !TARGET_OS_OSX
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(notifyAppEnterBackground)
                                                 name:UIApplicationDidEnterBackgroundNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(notifyAppEnterBackground)
                                                 name:UIApplicationWillResignActiveNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(notifyAppEnterForeground)
                                                 name:UIApplicationWillEnterForegroundNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(notifyAppEnterForeground)
                                                 name:UIApplicationDidBecomeActiveNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(notifyAppQuitByUser)
                                                 name:UIApplicationWillTerminateNotification
                                               object:nil];
#else
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(notifyAppEnterForeground)
                                                 name:NSApplicationDidBecomeActiveNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(notifyAppEnterBackground)
                                                 name:NSApplicationDidResignActiveNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(notifyAppEnterBackground)
                                                 name:NSApplicationDidHideNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(notifyAppEnterForeground)
                                                 name:NSApplicationDidUnhideNotification
                                               object:nil];
#endif

    atexit(g_matrix_app_exit);
}

+ (void)checkRebootType
{
    if ([MatrixDeviceInfo isBeingDebugged]) {
        MatrixInfo(@"app is being debugged");
        MatrixAppRebootInfo *info = [MatrixAppRebootInfo sharedInstance];
        info.isAppCrashed = NO;
        info.isAppQuitByExit = NO;
        info.isAppQuitByUser = NO;
        info.isAppWillSuspend = NO;
        info.isAppEnterBackground = NO;
        info.isAppEnterForeground = NO;
        info.isAppBackgroundFetch = NO;
        info.isAppSuspendKilled = NO;
        info.isAppMainThreadBlocked = NO;
        info.dumpFileName = @"";
        info.userScene = @"";
        [info saveInfo];
        return;
    }
    
    MatrixAppRebootInfo *info = [MatrixAppRebootInfo sharedInstance];

    if (info.isAppCrashed) {
        s_rebootType = MatrixAppRebootTypeNormalCrash;
    } else if (info.isAppQuitByUser) {
        s_rebootType = MatrixAppRebootTypeQuitByUser;
    } else if (info.isAppQuitByExit) {
        s_rebootType = MatrixAppRebootTypeQuitByExit;
    } else if (info.isAppWillSuspend || info.isAppBackgroundFetch) {
        if (info.isAppSuspendKilled) {
            s_rebootType = MatrixAppRebootTypeAppSuspendCrash;
        } else {
            s_rebootType = MatrixAppRebootTypeAppSuspendOOM;
        }
    } else if ([MatrixAppRebootAnalyzer isAppChange]) {
        s_rebootType = MatrixAppRebootTypeAPPVersionChange;
    } else if ([MatrixAppRebootAnalyzer isOSChange]) {
        s_rebootType = MatrixAppRebootTypeOSVersionChange;
    } else if ([MatrixAppRebootAnalyzer isOSReboot]) {
        s_rebootType = MatrixAppRebootTypeOSReboot;
    } else if (info.isAppEnterBackground) {
        s_rebootType = MatrixAppRebootTypeAppBackgroundOOM;
    } else if (info.isAppEnterForeground) {
        if (info.isAppMainThreadBlocked) {
            s_rebootType = MatrixAppRebootTypeAppForegroundDeadLoop;
            s_lastDumpFileName = info.dumpFileName;
        } else {
            s_rebootType = MatrixAppRebootTypeAppForegroundOOM;
            s_lastDumpFileName = @"";
        }
    } else {
        s_rebootType = MatrixAppRebootTypeOtherReason;
    }

    MatrixInfo(@"checkRebootType reboot type : %lu , last dump file name : %@", (unsigned long)s_rebootType, s_lastDumpFileName);

    s_lastUserScene = info.userScene;
    s_lastAppLaunchTime = info.appLaunchTime;
    s_lastTimeDeviceReboot = [MatrixAppRebootAnalyzer isOSReboot];
    
    // revert to original state

    // save last os version
    info.osVersion = [MatrixDeviceInfo systemVersion];

    // save last app uuid
    info.appUUID = @(app_uuid());

    // save app launch time stamp
    info.appLaunchTime = (uint64_t) time(NULL);

    //
    info.isAppCrashed = NO;
    info.isAppQuitByExit = NO;
    info.isAppQuitByUser = NO;
    info.isAppWillSuspend = NO;
    info.isAppEnterBackground = NO;
    info.isAppEnterForeground = NO;
    info.isAppBackgroundFetch = NO;
    info.isAppSuspendKilled = NO;
    info.isAppMainThreadBlocked = NO;
    info.dumpFileName = @"";
    info.userScene = @"";

    [info saveInfo];
}

+ (BOOL)isAfterLastLaunchUserRebootDevice
{
    return s_lastTimeDeviceReboot;
}

+ (MatrixAppRebootType)lastRebootType
{
    return s_rebootType;
}

+ (uint64_t)appLaunchTime
{
    MatrixAppRebootInfo *info = [MatrixAppRebootInfo sharedInstance];
    return info.appLaunchTime;
}

+ (uint64_t)lastAppLaunchTime
{
    return s_lastAppLaunchTime;
}

void g_matrix_app_exit()
{
    [MatrixAppRebootAnalyzer notifyAppQuitByExit];
}

+ (void)notifyAppQuitByExit
{
    CHECK_DEBUGGING;

    MatrixDebug(@"app quit by exit");
    MatrixAppRebootInfo *info = [MatrixAppRebootInfo sharedInstance];
    info.isAppQuitByExit = YES;
    [info saveInfo];
}

+ (void)notifyAppQuitByUser
{
    CHECK_DEBUGGING;

    MatrixDebug(@"app quit by user");
    MatrixAppRebootInfo *info = [MatrixAppRebootInfo sharedInstance];
    info.isAppQuitByUser = YES;
    [info saveInfo];
}

+ (void)notifyAppCrashed
{
    CHECK_DEBUGGING;

    MatrixDebug(@"app is crashed");
    MatrixAppRebootInfo *info = [MatrixAppRebootInfo sharedInstance];
    info.isAppCrashed = YES;
    [info saveInfo];
}

+ (void)notifyAppEnterForeground
{
    CHECK_DEBUGGING;

    // delay one second
    s_foregroundDelayBlock = dispatch_block_create(DISPATCH_BLOCK_NO_QOS_CLASS, ^{
        MatrixDebug(@"enter foreground or become active");

        MatrixAppRebootInfo *info = [MatrixAppRebootInfo sharedInstance];
        info.isAppEnterForeground = YES;
        info.isAppWillSuspend = NO;
        info.isAppEnterBackground = NO;
        info.isAppBackgroundFetch = NO;

        if (s_suspendDelayBlock != nil) {
            dispatch_block_cancel(s_suspendDelayBlock);
            s_suspendDelayBlock = nil;
        }
        if (s_isSuspendKilled) {
            // 取消标记位
            s_isSuspendKilled = NO;
            info.isAppSuspendKilled = NO;
            MatrixDebug(@"clear isSuspendKilled flag");
        }

        [info saveInfo];
    });
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)), dispatch_get_main_queue(), s_foregroundDelayBlock);
}

+ (void)notifyAppEnterBackground
{
    CHECK_DEBUGGING
    
    MatrixDebug(@"enter background");

    // s_foregroundDelayBlock canceled, to avoid misjudgment
    if (s_foregroundDelayBlock) {
        dispatch_block_cancel(s_foregroundDelayBlock);
        s_foregroundDelayBlock = nil;
    }

    MatrixAppRebootInfo *info = [MatrixAppRebootInfo sharedInstance];
    info.isAppEnterBackground = YES;
    info.isAppEnterForeground = NO;
    info.isAppWillSuspend = NO;
    [info saveInfo];
}

+ (void)notifyAppBackgroundFetch
{
    CHECK_DEBUGGING
    
    MatrixDebug(@"background fetch");

    MatrixAppRebootInfo *info = [MatrixAppRebootInfo sharedInstance];
    info.isAppBackgroundFetch = YES;
    [info saveInfo];
}

+ (void)notifyAppWillSuspend
{
    CHECK_DEBUGGING
    
    MatrixDebug(@"will suspend");

    // s_foregroundDelayBlock canceled, to avoid misjudgment
    if (s_foregroundDelayBlock) {
        dispatch_block_cancel(s_foregroundDelayBlock);
        s_foregroundDelayBlock = nil;
    }

    if (s_suspendDelayBlock) {
        dispatch_block_cancel(s_suspendDelayBlock);
        s_suspendDelayBlock = nil;
    }

    MatrixAppRebootInfo *info = [MatrixAppRebootInfo sharedInstance];
    info.isAppWillSuspend = YES;
    info.isAppEnterBackground = NO;
    info.isAppEnterForeground = NO;
    [info saveInfo];

    // iOS 11.0.x is a high probability of being killed by the system when suspend,
    // the feature is the app after suspend will continue to run for 20s, then killed by watchdog
    NSTimeInterval now = [[NSDate date] timeIntervalSince1970];
    s_suspendDelayBlock = dispatch_block_create(DISPATCH_BLOCK_NO_QOS_CLASS, ^{
        if ([[NSDate date] timeIntervalSince1970] - now < 20) {
            s_isSuspendKilled = YES;
            info.isAppSuspendKilled = YES;
            [info saveInfo];
            MatrixInfo(@"maybe killed after several seconds...");
        }
    });
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(18 * NSEC_PER_SEC)), dispatch_get_main_queue(), s_suspendDelayBlock);
}

+ (void)setUserScene:(NSString *)scene
{
    CHECK_DEBUGGING;

    MatrixDebug(@"set scene: %@", scene);
    MatrixAppRebootInfo *info = [MatrixAppRebootInfo sharedInstance];
    info.userScene = scene;
    [info saveInfo];
}

+ (NSString *)userSceneOfLastRun
{
    return s_lastUserScene;
}

+ (void)isForegroundMainThreadBlock:(BOOL)yn
{
    static BOOL lastValue = NO;
    if (lastValue != yn) {
        lastValue = yn;
        MatrixAppRebootInfo *info = [MatrixAppRebootInfo sharedInstance];
        info.isAppMainThreadBlocked = yn;
        [info saveInfo];

        if (yn == NO) {
            [self setDumpFileName:@""];
        }
    }
}

+ (void)setDumpFileName:(NSString *)fileName
{
    MatrixAppRebootInfo *info = [MatrixAppRebootInfo sharedInstance];
    info.dumpFileName = fileName;
    [info saveInfo];
}

+ (NSString *)lastDumpFileName
{
    return s_lastDumpFileName;
}

+ (BOOL)checkXPCReboot
{
    /*
     XPC 卡顿检测逻辑
     1. 获取当天的卡顿文件
     2. 将文件解析成 NSDictionary
     3. 检查是否包含耗时堆栈，如果有则利用主线程耗时堆栈进行检测，如果没有则利用 thread 0 堆栈进行检测
     */
    
    // 获取当天的卡顿堆栈
    NSDate *today = [NSDate date];
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setDateFormat:@"yyyy-MM-dd"];
    NSString *todayStr = [formatter stringFromDate:today];
    NSArray *dumpTypeArray = @[@((NSUInteger)EDumpType_MainThreadBlock),
                               @((NSUInteger)EDumpType_BackgroundMainThreadBlock),
                               @((NSUInteger)EDumpType_LaunchBlock),
                               @((NSUInteger)EDumpType_BlockThreadTooMuch),
                               @((NSUInteger)EDumpType_BlockAndBeKilled)];
    
    NSUInteger xpcLagCount = 0;
    for (NSNumber *dumpType in dumpTypeArray) {
        xpcLagCount += [self checkXPCLagFile:(EDumpType)[dumpType unsignedIntegerValue] withDate:todayStr];
    }
    
    if (xpcLagCount > 1) {
        MatrixInfo(@"did occur xpc lag, xpc lag number is:(%lu)", xpcLagCount);
        return YES;
    }
    MatrixInfo(@"no xpc lag");
    return NO;
}

+ (NSUInteger)checkXPCLagFile:(EDumpType)dumpType withDate:(NSString *)date
{
    NSArray *lagFileArr = [WCCrashBlockFileHandler getLagReportIDWithType:dumpType withDate:date];
    NSUInteger xpcLagCount = 0;
    
    // 将文件解析成 NSDictionary, 并检查卡顿的所有日志文件
    for (NSString* lagFile in lagFileArr) {
        @autoreleasepool {
            NSData *lagData = [WCCrashBlockFileHandler getLagDataWithReportID:lagFile andReportType:dumpType];
            
            NSError *error = nil;
            NSMutableDictionary *lagDataDic = [WCCrashBlockJsonUtil jsonDecode:lagData withError:&error];
            if (error != nil) {
                MatrixInfo(@"decode data error %@", lagFile);
                continue;
            }
            if (lagDataDic == nil) {
                MatrixInfo(@"decode data is nil %@", lagFile);
                continue;
            }
            
            NSMutableDictionary *crashDic = [lagDataDic objectForKey:@"crash"];
            if (crashDic == nil) {
                MatrixInfo(@"decode data doesn't contain crash info %@", lagFile);
                continue;
            }
            
            NSMutableArray *threadsArr = [crashDic objectForKey:@"threads"];
            if (threadsArr == nil || threadsArr.count == 0) {
                MatrixInfo(@"decode data doesn't contain threads info %@", lagFile);
                continue;
            }
            
            NSMutableDictionary *threadInfoDic = [threadsArr firstObject];
            NSMutableDictionary *threadContentsDic = [threadInfoDic objectForKey:@"backtrace"];
            if (threadContentsDic == nil) {
                MatrixInfo(@"decode data doesn't contain thread info %@", lagFile);
                continue;
            }
            
            NSMutableArray *threadContentsArr = [threadContentsDic objectForKey:@"contents"];
            if (threadContentsArr == nil || threadContentsArr.count == 0) {
                MatrixInfo(@"decode data doesn't contain thread info %@", lagFile);
                continue;
            }
            
            for (NSMutableDictionary *threadDetailDic in threadContentsArr) {
                NSString *objectName = [threadDetailDic objectForKey:@"object_name"];
                if (objectName != nil && [objectName isEqualToString:@"libxpc.dylib"]) {
                    MatrixInfo(@"occur xpc lag, lag file is: %@", lagFile);
                    xpcLagCount += 1;
                    break;
                }
            }
        }
    }
    return xpcLagCount;
}

// ============================================================================
#pragma mark - Private
// ============================================================================

+ (BOOL)isAppChange
{
    MatrixAppRebootInfo *info = [MatrixAppRebootInfo sharedInstance];
    NSString *lastUUID = info.appUUID;
    NSString *nowUUID = @(app_uuid());

    MatrixDebug(@"app uuid now: %@, last: %@", nowUUID, lastUUID);
    return (lastUUID != nil && ![lastUUID isEqualToString:nowUUID]);
}

+ (BOOL)isOSChange
{
    MatrixAppRebootInfo *info = [MatrixAppRebootInfo sharedInstance];
    NSString *lastOsVersion = info.osVersion;
    NSString *nowOsVersion = [MatrixDeviceInfo systemVersion];

    MatrixDebug(@"osversion now: %@ last: %@", nowOsVersion, lastOsVersion);
    return ![lastOsVersion isEqualToString:nowOsVersion];
}

+ (uint64_t)getSystemLaunchTimeStamp
{
    NSTimeInterval time = [NSProcessInfo processInfo].systemUptime;
    NSDate *curDate = [[NSDate alloc] init];
    NSDate *startTime = [curDate dateByAddingTimeInterval:-time];
    return [startTime timeIntervalSince1970];
}

+ (BOOL)isOSReboot
{
    MatrixAppRebootInfo *info = [MatrixAppRebootInfo sharedInstance];
    uint64_t lastAppLaunchTimeStamp = info.appLaunchTime;
    uint64_t systemStartTimeStamp = [MatrixAppRebootAnalyzer getSystemLaunchTimeStamp];

    MatrixDebug(@"system start: %lld app launch: %lld", systemStartTimeStamp, lastAppLaunchTimeStamp);
    return systemStartTimeStamp > lastAppLaunchTimeStamp;
}

@end
