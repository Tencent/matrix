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

#import "WCDumpInterface.h"
#import <sys/stat.h>
#import "WCCrashBlockFileHandler.h"
#import "KSCrash.h"

@implementation WCDumpInterface

+ (NSString *)dumpReportWithReportType:(EDumpType)dumpType
                     suspendAllThreads:(BOOL)suspendAllThreads
                        enableSnapshot:(BOOL)enableSnapshot {
    return [WCDumpInterface dumpReportWithReportType:dumpType
                                 withExceptionReason:@""
                                   suspendAllThreads:suspendAllThreads
                                      enableSnapshot:enableSnapshot
                                       writeCpuUsage:NO
                                     selfDefinedPath:YES];
}

+ (NSString *)dumpReportWithReportType:(EDumpType)dumpType
                   withExceptionReason:(NSString *)exceptionReason
                     suspendAllThreads:(BOOL)suspendAllThreads
                        enableSnapshot:(BOOL)enableSnapshot
                         writeCpuUsage:(BOOL)writeCpuUsage
                       selfDefinedPath:(BOOL)bSelfDefined {
    KSCrash *handler = [KSCrash sharedInstance];
    NSString *name = [NSString stringWithFormat:@"%lu", (unsigned long)dumpType];
    NSString *crashReportPath = nil;
    if (bSelfDefined) {
        crashReportPath = [WCDumpInterface genFilePathWithReportType:dumpType];
    }

    [handler reportUserException:name
                          reason:exceptionReason
                        language:@""
                      lineOfCode:NULL
                      stackTrace:NULL
                   logAllThreads:suspendAllThreads
                  enableSnapshot:enableSnapshot
                terminateProgram:false
                   writeCpuUsage:writeCpuUsage
                    dumpFilePath:crashReportPath
                        dumpType:(int)dumpType];

    return crashReportPath;
}

+ (NSString *)saveDump:(NSData *)dumpData withReportType:(EDumpType)dumpType withReportID:(NSString *)reportID {
    NSString *crashReportPath = [WCDumpInterface genFilePathWithReportType:dumpType withReportID:reportID];
    [dumpData writeToFile:crashReportPath atomically:YES];
    return crashReportPath;
}

+ (NSString *)genFilePathWithReportType:(EDumpType)dumpType {
    NSString *reportID = [[NSUUID UUID] UUIDString];
    return [WCDumpInterface genFilePathWithReportType:dumpType withReportID:reportID];
}

+ (NSString *)genFilePathWithReportType:(EDumpType)dumpType withReportID:(NSString *)reportIDString {
    static NSDateFormatter *formatter = nil;
    if (!formatter) {
        formatter = [[NSDateFormatter alloc] init];
        formatter.dateFormat = @"yyyy-MM-dd";
    }

    __weak NSDateFormatter *weakFormatter = formatter;
    if (!weakFormatter) {
        return nil;
    }

    NSDate *date = [NSDate date];
    NSString *formateDate = [weakFormatter stringFromDate:date];

    NSString *name = [NSString stringWithFormat:@"%lu", (unsigned long)dumpType];
    NSString *reportTypeDumpDir = [WCCrashBlockFileHandler diretoryOfUserDumpWithType:dumpType];
    NSString *crashReportFilePath = [[KSCrash sharedInstance] pathToCrashReportWithID:reportIDString withStorePath:reportTypeDumpDir];
    NSString *appendStr = [NSString stringWithFormat:@"-%@-%@.%@", formateDate, name, [crashReportFilePath pathExtension]];
    NSString *preStr = [crashReportFilePath stringByDeletingPathExtension];
    NSString *true_crashReportLastPathComponent = [preStr stringByAppendingString:appendStr];

    return true_crashReportLastPathComponent;
}

@end
