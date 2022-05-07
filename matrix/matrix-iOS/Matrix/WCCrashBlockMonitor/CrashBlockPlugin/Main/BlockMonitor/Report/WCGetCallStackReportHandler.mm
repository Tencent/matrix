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

#import "WCGetCallStackReportHandler.h"
#import "WCCrashReportInfoUtil.h"
#import "KSCrashReportFields.h"
#import "WCCrashBlockJsonUtil.h"
#import "WCBlockMonitorMgr.h"

#define CustomFrameStack "frame_stack"
#define CustomStackString "stack_string"
#define CustomReportField "scene"
#define CustomExtraInfo "custom_info"

static NSString *g_bundleName = nil;

static NSString *getBundleName() {
    if (g_bundleName == nil) {
        g_bundleName = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleName"];
    }
    if (g_bundleName == nil) {
        g_bundleName = @"Unknown";
    }
    return g_bundleName;
}

@implementation WCGetCallStackReportHandler

+ (NSDictionary *)getCustomReportInfoWithReportID:(NSString *)reportID {
    long timestamp = time(NULL);
    NSMutableDictionary *reportDictionary = [[NSMutableDictionary alloc] init];
    [reportDictionary setValue:@"0.5" forKey:@KSCrashField_Version];
    [reportDictionary setValue:reportID forKey:@KSCrashField_ID];
    [reportDictionary setValue:getBundleName() forKey:@KSCrashField_ProcessName];
    [reportDictionary setValue:[NSNumber numberWithLong:timestamp] forKey:@KSCrashField_Timestamp];
    [reportDictionary setValue:@"custom" forKey:@KSCrashField_Type];
    return [reportDictionary copy];
}

+ (NSData *)getReportJsonDataWithCallStackString:(NSString *)callStackString
                                    withReportID:(NSString *)reportID
                                    withDumpType:(EDumpType)dumpType
                                       withScene:(NSString *)scene {
    if (callStackString == nil || [callStackString length] == 0) {
        return nil;
    }
    NSMutableDictionary *reportDictionary = [NSMutableDictionary dictionary];
    [reportDictionary setValue:[WCGetCallStackReportHandler getCustomReportInfoWithReportID:reportID] forKey:@KSCrashField_Report];
    [reportDictionary setValue:[[WCCrashReportInfoUtil sharedInstance] getSystemInfo] forKey:@KSCrashField_System];
    if (scene != nil && [scene length] > 0) {
        [reportDictionary setValue:scene forKey:@CustomReportField];
    }
    NSDictionary *userDic = [[WCBlockMonitorMgr shareInstance] getUserInfoForCurrentDumpForDumpType:dumpType];
    [reportDictionary setValue:userDic forKey:@KSCrashExcType_User];
    [reportDictionary setValue:callStackString forKey:@CustomStackString];
    NSData *jsonData = [WCCrashBlockJsonUtil jsonEncode:reportDictionary withError:nil];
    return jsonData;
}

+ (NSData *)getReportJsonDataWithPowerConsumeStack:(NSArray<NSDictionary *> *)PowerConsumeStackArray
                                      withReportID:(NSString *)reportID
                                      withDumpType:(EDumpType)dumpType;
{
    if (PowerConsumeStackArray == nil || [PowerConsumeStackArray count] == 0) {
        return nil;
    }
    NSMutableDictionary *reportDictionary = [NSMutableDictionary dictionary];
    [reportDictionary setValue:[WCGetCallStackReportHandler getCustomReportInfoWithReportID:reportID] forKey:@KSCrashField_Report];
    [reportDictionary setValue:[[WCCrashReportInfoUtil sharedInstance] getBinaryImages] forKey:@KSCrashField_BinaryImages];
    [reportDictionary setValue:[[WCCrashReportInfoUtil sharedInstance] getSystemInfo] forKey:@KSCrashField_System];
    NSDictionary *userDic = [[WCBlockMonitorMgr shareInstance] getUserInfoForCurrentDumpForDumpType:dumpType];
    [reportDictionary setValue:userDic forKey:@KSCrashExcType_User];
    [reportDictionary setValue:[NSNumber numberWithInt:(int)dumpType] forKey:@KSCrashField_DumpType];
    [reportDictionary setValue:PowerConsumeStackArray forKey:@CustomStackString];
    NSData *jsonData = [WCCrashBlockJsonUtil jsonEncode:reportDictionary withError:nil];
    return jsonData;
}

+ (NSData *)getReportJsonDataWithDiskIOStack:(NSArray<NSDictionary *> *)stackArray
                              withCustomInfo:(NSDictionary *)customInfo
                                withReportID:(NSString *)reportID
                                withDumpType:(EDumpType)dumpType {
    if (stackArray == nil || [stackArray count] == 0) {
        return nil;
    }
    NSMutableDictionary *reportDictionary = [NSMutableDictionary dictionary];
    [reportDictionary setValue:[WCGetCallStackReportHandler getCustomReportInfoWithReportID:reportID] forKey:@KSCrashField_Report];
    [reportDictionary setValue:[[WCCrashReportInfoUtil sharedInstance] getBinaryImages] forKey:@KSCrashField_BinaryImages];
    [reportDictionary setValue:[[WCCrashReportInfoUtil sharedInstance] getSystemInfo] forKey:@KSCrashField_System];
    NSDictionary *userDic = [[WCBlockMonitorMgr shareInstance] getUserInfoForCurrentDumpForDumpType:dumpType];
    [reportDictionary setValue:userDic forKey:@KSCrashExcType_User];
    [reportDictionary setValue:[NSNumber numberWithInt:(int)dumpType] forKey:@KSCrashField_DumpType];
    [reportDictionary setValue:stackArray forKey:@CustomStackString];
    [reportDictionary setValue:customInfo forKey:@CustomExtraInfo];
    NSData *jsonData = [WCCrashBlockJsonUtil jsonEncode:reportDictionary withError:nil];
    return jsonData;
}

+ (NSData *)getReportJsonDataWithFPSStack:(NSArray<NSDictionary *> *)stackArray
                           withCustomInfo:(NSDictionary *)customInfo
                             withReportID:(NSString *)reportID
                             withDumpType:(EDumpType)dumpType {
    if (stackArray == nil || [stackArray count] == 0) {
        return nil;
    }
    NSMutableDictionary *reportDictionary = [NSMutableDictionary dictionary];
    [reportDictionary setValue:[WCGetCallStackReportHandler getCustomReportInfoWithReportID:reportID] forKey:@KSCrashField_Report];
    [reportDictionary setValue:[[WCCrashReportInfoUtil sharedInstance] getBinaryImages] forKey:@KSCrashField_BinaryImages];
    [reportDictionary setValue:[[WCCrashReportInfoUtil sharedInstance] getSystemInfo] forKey:@KSCrashField_System];
    NSDictionary *userDic = [[WCBlockMonitorMgr shareInstance] getUserInfoForCurrentDumpForDumpType:dumpType];
    [reportDictionary setValue:userDic forKey:@KSCrashExcType_User];
    [reportDictionary setValue:[NSNumber numberWithInt:(int)dumpType] forKey:@KSCrashField_DumpType];
    [reportDictionary setValue:stackArray forKey:@CustomStackString];
    [reportDictionary setValue:customInfo forKey:@CustomExtraInfo];
    NSData *jsonData = [WCCrashBlockJsonUtil jsonEncode:reportDictionary withError:nil];

    return jsonData;
}

+ (NSData *)getReportJsonDataWithLagProfileStack:(NSArray<NSDictionary *> *)stackArray {
    if (stackArray == nil || [stackArray count] == 0) {
        return nil;
    }
    NSMutableDictionary *reportDictionary = [NSMutableDictionary dictionary];
    [reportDictionary setValue:stackArray forKey:@CustomStackString];
    NSData *jsonData = [WCCrashBlockJsonUtil jsonEncode:reportDictionary withError:nil];

    return jsonData;
}

@end
