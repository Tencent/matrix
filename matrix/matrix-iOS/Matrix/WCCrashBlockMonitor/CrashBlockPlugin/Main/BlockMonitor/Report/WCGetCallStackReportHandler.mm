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

static NSString* g_bundleName = nil;

static NSString* getBundleName()
{
    if (g_bundleName == nil) {
        g_bundleName = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleName"];
    }
    if (g_bundleName == nil)
    {
        g_bundleName = @"Unknown";
    }
    return g_bundleName;
}

@implementation WCGetCallStackReportHandler

+ (NSData *)getReportJsonDataWithCallStackArray:(KSStackCursor **)stackCursorArray
                                 callStackCount:(NSUInteger)stackCount
                                   withReportID:(NSString *)reportID
                                   withDumpType:(EDumpType)dumpType
{
    return [WCGetCallStackReportHandler getReportJsonDataWithCallStackArray:stackCursorArray
                                                             callStackCount:stackCount
                                                               withReportID:reportID
                                                               withDumpType:dumpType
                                                                  withScene:@""];
}

+ (NSData *)getReportJsonDataWithCallStackArray:(KSStackCursor **)stackCursorArray
                                 callStackCount:(NSUInteger)stackCount
                                   withReportID:(NSString *)reportID
                                   withDumpType:(EDumpType)dumpType
                                      withScene:(NSString *)scene
{
    NSMutableDictionary *reportDictionary = [[NSMutableDictionary alloc] init];

    NSMutableArray *topStackAddressArray = [[NSMutableArray alloc] init];
    NSMutableArray *stackArray = [[NSMutableArray alloc] init];
    for (NSUInteger index = 0; index < stackCount; index++) {
        KSStackCursor *stackCursor = stackCursorArray[index];
        NSMutableArray *framePointerArray = [[NSMutableArray alloc] init];
        NSUInteger currentDepth = 0;
        while (stackCursor->advanceCursor(stackCursor)) {
            NSMutableDictionary *pointInfoDic = [[NSMutableDictionary alloc] init];
            if (stackCursor->symbolicate(stackCursor)) {
                if (stackCursor->stackEntry.imageName != NULL) {
                    [pointInfoDic setObject:[NSString stringWithUTF8String:wxg_lastPathEntry(stackCursor->stackEntry.imageName)] forKey:@KSCrashField_ObjectName];
                }
                [pointInfoDic setValue:[NSNumber numberWithUnsignedInteger:stackCursor->stackEntry.imageAddress] forKey:@KSCrashField_ObjectAddr];
                if (stackCursor->stackEntry.symbolName != NULL) {
                    [pointInfoDic setObject:[NSString stringWithUTF8String:stackCursor->stackEntry.symbolName] forKey:@KSCrashField_SymbolName];
                }
                [pointInfoDic setValue:[NSNumber numberWithUnsignedInteger:stackCursor->stackEntry.symbolAddress] forKey:@KSCrashField_SymbolAddr];
            }
            NSNumber *stackAddress = [NSNumber numberWithUnsignedInteger:stackCursor->stackEntry.address];
            [pointInfoDic setValue:stackAddress forKey:@KSCrashField_InstructionAddr];
            if (currentDepth == 0) {
                [topStackAddressArray addObject:stackAddress];
            }
            currentDepth += 1;
            [framePointerArray addObject:pointInfoDic];
        }
        if ([framePointerArray count] > 0) {
            [stackArray addObject:framePointerArray];
        }
    }

    NSUInteger checkcount = 5;
    if ([stackArray count] > checkcount) {
        NSMutableArray *topRepeatArray = [[NSMutableArray alloc] init];
        NSUInteger maxValue = 0;
        for (NSUInteger index = 0; index < checkcount; index++) {
            if (index == (checkcount - 1)) {
                break;
            }
            NSNumber *topAddress = topStackAddressArray[index]; 
            NSUInteger repeatCount = 0;
            NSUInteger beginIndex = index + 1;
            for (NSUInteger j = beginIndex; j < checkcount; j++) {
                NSNumber *secTopAddress = topStackAddressArray[j];
                if ([topAddress unsignedIntegerValue] == [secTopAddress unsignedIntegerValue]) {
                    repeatCount++;
                } else {
                    break;
                }
            }
            if (maxValue < repeatCount) {
                maxValue = repeatCount;
            }
            [topRepeatArray addObject:[NSNumber numberWithUnsignedInteger:repeatCount]];
        }
        for (NSUInteger index = 0; index < topRepeatArray.count; index++) {
            NSUInteger currentCount = [(NSNumber *) topRepeatArray[index] unsignedIntegerValue];
            if (currentCount == maxValue) {
                NSArray *framePointArray = [stackArray[index] copy];
                if ([framePointArray count] > 10) {
                    [stackArray insertObject:framePointArray atIndex:0];
                }
                break;
            }
        }
    }

    if ([stackArray count] > 0) {
        [reportDictionary setValue:stackArray forKey:@CustomFrameStack];
    } else {
        return nil;
    }

    [reportDictionary setValue:[WCGetCallStackReportHandler getCustomReportInfoWithReportID:reportID]
                        forKey:@KSCrashField_Report];
    [reportDictionary setValue:[[WCCrashReportInfoUtil sharedInstance] getSystemInfo]
                        forKey:@KSCrashField_System];
    [reportDictionary setValue:[[WCCrashReportInfoUtil sharedInstance] getBinaryImages]
                        forKey:@KSCrashField_BinaryImages];
    
    NSDictionary *userDic = [[WCBlockMonitorMgr shareInstance] getUserInfoForCurrentDumpForDumpType:dumpType];
    [reportDictionary setValue:userDic forKey:@KSCrashExcType_User];

    if (scene != nil && [scene length] > 0) {
        [reportDictionary setValue:scene forKey:@CustomReportField];
    }

    NSData *jsonData = [WCCrashBlockJsonUtil jsonEncode:reportDictionary withError:nil];
    return jsonData;
}

+ (NSDictionary *)getCustomReportInfoWithReportID:(NSString *)reportID
{
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
                                       withScene:(NSString *)scene
{
    if (callStackString == nil || [callStackString length] == 0) {
        return nil;
    }
    NSMutableDictionary *reportDictionary = [NSMutableDictionary dictionary];
    [reportDictionary setValue:[WCGetCallStackReportHandler getCustomReportInfoWithReportID:reportID]
                        forKey:@KSCrashField_Report];
    [reportDictionary setValue:[[WCCrashReportInfoUtil sharedInstance] getSystemInfo]
                        forKey:@KSCrashField_System];
    if (scene != nil && [scene length] > 0) {
        [reportDictionary setValue:scene forKey:@CustomReportField];
    }
    NSDictionary *userDic = [[WCBlockMonitorMgr shareInstance] getUserInfoForCurrentDumpForDumpType:dumpType];
    [reportDictionary setValue:userDic forKey:@KSCrashExcType_User];
    [reportDictionary setValue:callStackString forKey:@CustomStackString];
    NSData *jsonData = [WCCrashBlockJsonUtil jsonEncode:reportDictionary withError:nil];
    return jsonData;
}

+ (NSData *)getReportJsonDataWithPowerConsumeStack:(NSArray <NSDictionary *> *)PowerConsumeStackArray
                                     withReportID:(NSString *)reportID
                                     withDumpType:(EDumpType)dumpType;
{
    if (PowerConsumeStackArray == nil || [PowerConsumeStackArray count] == 0) {
        return nil;
    }
    NSMutableDictionary *reportDictionary = [NSMutableDictionary dictionary];
    [reportDictionary setValue:[WCGetCallStackReportHandler getCustomReportInfoWithReportID:reportID]
                        forKey:@KSCrashField_Report];
    [reportDictionary setValue:[[WCCrashReportInfoUtil sharedInstance] getBinaryImages]
                        forKey:@KSCrashField_BinaryImages];
    [reportDictionary setValue:[[WCCrashReportInfoUtil sharedInstance] getSystemInfo]
                        forKey:@KSCrashField_System];
    NSDictionary *userDic = [[WCBlockMonitorMgr shareInstance] getUserInfoForCurrentDumpForDumpType:dumpType];
    [reportDictionary setValue:userDic forKey:@KSCrashExcType_User];
    [reportDictionary setValue:[NSNumber numberWithUnsignedInteger:dumpType] forKey:@KSCrashField_DumpType];
    [reportDictionary setValue:PowerConsumeStackArray forKey:@CustomStackString];
    NSData *jsonData = [WCCrashBlockJsonUtil jsonEncode:reportDictionary withError:nil];
    return jsonData;
}

@end
