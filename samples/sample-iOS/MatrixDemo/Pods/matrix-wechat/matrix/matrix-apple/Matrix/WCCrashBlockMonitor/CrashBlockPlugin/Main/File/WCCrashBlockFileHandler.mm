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

#import "WCCrashBlockFileHandler.h"

#import "KSCrash.h"
#import "KSCrashReportFields.h"
#import "MatrixLogDef.h"
#import "MatrixPathUtil.h"
#import "WCCrashBlockJsonUtil.h"

@implementation WCCrashBlockFileHandler

// ============================================================================
#pragma mark - Crash File
// ============================================================================

+ (NSDictionary *)getPendingCrashReportInfo
{
    NSString *newestID = [WCCrashBlockFileHandler loadPendingCrashReportID];
    if (newestID.length == 0 || newestID == nil) {
        return nil;
    }
    NSDictionary *reportDic = [[KSCrash sharedInstance] reportWithStringID:newestID];
    NSData *jsonData = [WCCrashBlockJsonUtil jsonEncode:reportDic withError:nil];
    
    if (jsonData.length == 0 || jsonData == nil) {
        MatrixWarning(@"load pending crash report, json data is nil");
        [WCCrashBlockFileHandler deleteCrashDataWithReportID:newestID];
        return nil;
    }

    NSDictionary *reportInfoData = @{ @"reportID" : newestID,
                                      @"crashData" : jsonData };
    MatrixInfo(@"load pending crash report data %@", newestID);
    return reportInfoData;
}

+ (NSString *)loadPendingCrashReportID
{
    KSCrash *handler = [KSCrash sharedInstance];
    NSArray *reportIDSArray = [handler allReportID];

    for (NSString *reportID in reportIDSArray) {
        if ([WCCrashBlockFileHandler p_isCrashReportID:reportID]) {
            return reportID;
        }
    }
    return nil;
}

+ (NSArray *)getAllCrashReportPath
{
    NSArray *reportIDArray = [WCCrashBlockFileHandler getAllCrashReportID];
    if ([reportIDArray count] == 0) {
        return nil;
    }
    NSMutableArray *reportFileArray = [[NSMutableArray alloc] init];
    for (NSString *crashID in reportIDArray) {
        NSString *filePath = [[KSCrash sharedInstance] pathToCrashReportWithID:crashID];
        [reportFileArray addObject:filePath];
    }
    return [reportFileArray copy];
}

+ (NSArray *)getAllCrashReportID
{
    NSArray *reportIDArray = [[KSCrash sharedInstance] allReportID];
    if ([reportIDArray count] == 0) {
        return nil;
    }
    NSMutableArray *crashReportIDArray = [[NSMutableArray alloc] init];
    for (NSString *reportID in reportIDArray) {
        if ([WCCrashBlockFileHandler p_isCrashReportID:reportID]) {
            [crashReportIDArray addObject:reportID];
        }
    }
    return [crashReportIDArray copy];
}

+ (BOOL)hasCrashReport
{
    NSArray *reportIDArray = [[KSCrash sharedInstance] allReportID];
    if ([reportIDArray count] == 0) {
        return NO;
    }
    for (NSString *reportID in reportIDArray) {
        if ([WCCrashBlockFileHandler p_isCrashReportID:reportID]) {
            return YES;
        }
    }
    return NO;
}

+ (void)deleteCrashDataWithReportID:(NSString *)reportID
{
    MatrixInfo(@"delete crash report data with reportID : %@", reportID);
    KSCrash *handler = [KSCrash sharedInstance];
    [handler deleteReportWithID:reportID];
}

// 判断文件名，crash和卡顿日志的格式不同
// crash report id format: 58D5E212-165B-4CA0-909B-C86B9CEE0111
// dump report id format: 58D5E212-165B-4CA0-909B-C86B9CEE0111-2016-6-28-2016
+ (BOOL)p_isCrashReportID:(NSString *)reportID
{
    NSArray *stringArray = [reportID componentsSeparatedByString:@"-"];
    return [stringArray count] < 6;
}

// ============================================================================
#pragma mark - Lag File
// ============================================================================

static NSString *g_userDumpCachePath = nil;

+ (NSString *)diretoryOfUserDump
{
    if (g_userDumpCachePath != nil && [g_userDumpCachePath length] > 0) {
        return g_userDumpCachePath;
    }

    g_userDumpCachePath = [MatrixPathUtil crashBlockPluginCachePath];
    NSFileManager *oFileMgr = [NSFileManager defaultManager];
    if (oFileMgr != nil && [oFileMgr fileExistsAtPath:g_userDumpCachePath] == NO) {
        NSError *err;
        [oFileMgr createDirectoryAtPath:g_userDumpCachePath
            withIntermediateDirectories:YES
                             attributes:nil
                                  error:&err];
    }
    return g_userDumpCachePath;
}

+ (NSString *)diretoryOfUserDumpWithType:(EDumpType)type
{
    NSString *typeDiretory = [[WCCrashBlockFileHandler diretoryOfUserDump] stringByAppendingPathComponent:[NSString stringWithFormat:@"%lu", (unsigned long) type]];

    NSFileManager *oFileMgr = [NSFileManager defaultManager];
    if (oFileMgr != nil && [oFileMgr fileExistsAtPath:typeDiretory] == NO) {
        NSError *err;
        [oFileMgr createDirectoryAtPath:typeDiretory
            withIntermediateDirectories:YES
                             attributes:nil
                                  error:&err];
    }
    return typeDiretory;
}

+ (NSArray *)getLagReportIDWithType:(EDumpType)dumpType withDate:(NSString *)limitDate
{
    NSString *fileSuffix = [WCCrashBlockFileHandler p_getFileSuffixWithType:dumpType withDate:limitDate];

    NSArray *reportIDArray = [[KSCrash sharedInstance] allReportIDWithPath:[WCCrashBlockFileHandler diretoryOfUserDumpWithType:dumpType]];

    NSMutableArray *arrResult = [[NSMutableArray alloc] init];
    for (NSString *reportID in reportIDArray) {
        if ([reportID hasSuffix:fileSuffix]) {
            [arrResult addObject:reportID];
        }
    }
    return arrResult;
}

+ (NSData *)getLagDataWithReportIDArray:(NSArray *)reportIDArray andReportType:(EDumpType)dumpType;
{
    NSString *storePath = [WCCrashBlockFileHandler diretoryOfUserDumpWithType:dumpType];
    NSMutableArray *reportArray = [[NSMutableArray alloc] init];

    for (NSString *reportID in reportIDArray) {
        NSError *error = nil;
        NSString *path = [[KSCrash sharedInstance] pathToCrashReportWithID:reportID withStorePath:storePath];
        NSData *jsonData = [NSData dataWithContentsOfFile:path];
        if (jsonData == nil) {
            MatrixError(@"get file %@ fail", path);
            [WCCrashBlockFileHandler deleteLagDataWithReportID:reportID andReportType:dumpType];
            continue;
        }

        NSMutableDictionary *report = [WCCrashBlockJsonUtil jsonDecode:jsonData withError:&error];
        if (error != nil) {
            MatrixError(@"get file JSON data from %@: %@", path, error);
            [WCCrashBlockFileHandler deleteLagDataWithReportID:reportID andReportType:dumpType];
            continue;
        }

        [reportArray addObject:report];
    }

    return [WCCrashBlockJsonUtil jsonEncode:reportArray withError:nil];
}

+ (NSData *)getLagDataWithReportID:(NSString *)reportID andReportType:(EDumpType)dumpType
{
    NSString *storePath = [WCCrashBlockFileHandler diretoryOfUserDumpWithType:dumpType];
    NSString *path = [[KSCrash sharedInstance] pathToCrashReportWithID:reportID withStorePath:storePath];
    NSData *jsonData = [NSData dataWithContentsOfFile:path];

    if (jsonData == nil) {
        MatrixError(@"get file %@ fail", path);
        [WCCrashBlockFileHandler deleteLagDataWithReportID:reportID andReportType:dumpType];
        return nil;
    }

    return jsonData;
}

+ (void)deleteLagDataWithReportID:(NSString *)reportID andReportType:(EDumpType)dumpType
{
    NSString *storePath = [WCCrashBlockFileHandler diretoryOfUserDumpWithType:dumpType];
    [[KSCrash sharedInstance] deleteReportWithID:reportID withStorePath:storePath];
}

+ (NSString *)p_getFileSuffixWithType:(EDumpType)type withDate:(NSString *)limitDate
{
    if (limitDate.length == 0 || !limitDate) {
        return [NSString stringWithFormat:@"-%lu", (unsigned long) type];
    }
    return [NSString stringWithFormat:@"%@-%lu", limitDate, (unsigned long) type];
}

// ============================================================================
#pragma mark - Launch Lag Info
// ============================================================================

+ (NSString *)getLaunchBlockRecordFilePath
{
    NSString *ret = [WCCrashBlockFileHandler diretoryOfUserDump];
    ret = [ret stringByAppendingPathComponent:@"LaunchBlockRecord.dat"];
    return ret;
}

// ============================================================================
#pragma mark - Stack Feat
// ============================================================================

+ (NSString *)getStackFeatFilePath
{
    NSString *ret = [WCCrashBlockFileHandler diretoryOfUserDump];
    ret = [ret stringByAppendingPathComponent:@"stackfeat.dat"];
    return ret;
}

// ============================================================================
#pragma mark - Handle OOM
// ============================================================================

+ (void)handleOOMDumpFile:(NSString *)lagFilePath
{
    if (lagFilePath == nil || lagFilePath.length == 0) {
        return;
    }

    NSArray *stringComponentArray = [lagFilePath componentsSeparatedByString:@"/"];
    if ([stringComponentArray count] <= 2) {
        MatrixWarning(@"%@ path is illegal", lagFilePath);
        return;
    }

    NSString *lagLastPathComponent = [stringComponentArray lastObject];
    NSString *dumpTypeString = [stringComponentArray objectAtIndex:([stringComponentArray count] - 2)];
    EDumpType dumpType = (EDumpType)[dumpTypeString intValue];
    NSString *trueLagFilePath = [[WCCrashBlockFileHandler diretoryOfUserDumpWithType:dumpType] stringByAppendingPathComponent:lagLastPathComponent];

    NSFileManager *fileMgr = [NSFileManager defaultManager];
    BOOL bExist = [fileMgr fileExistsAtPath:trueLagFilePath];
    if (bExist == NO) {
        MatrixWarning(@"%@ is not exist", trueLagFilePath);
        return;
    }
    
    MatrixInfo(@"last oom dump file path: %@", trueLagFilePath);
    
    NSData *jsonData = [NSData dataWithContentsOfFile:trueLagFilePath];
    NSError *error = nil;
    NSMutableDictionary* report = [WCCrashBlockJsonUtil jsonDecode:jsonData withError:&error];
    
    if (error != nil) {
        MatrixWarning(@"Error decoding JSON data from %@: %@", trueLagFilePath, error);
        return;
    }
    
    report[@KSCrashField_Crash][@KSCrashField_Error][@KSCrashField_UserReported][@KSCrashField_DumpType] = [NSNumber numberWithUnsignedInteger:EDumpType_BlockAndBeKilled];
    
    jsonData = [WCCrashBlockJsonUtil jsonEncode:report withError:nil];
    
    NSString *newLagLastPathComponent = [lagLastPathComponent stringByReplacingOccurrencesOfString:dumpTypeString withString:[NSString stringWithFormat:@"%lu", (unsigned long)EDumpType_BlockAndBeKilled]];
    NSString *newLagFilePath = [[WCCrashBlockFileHandler diretoryOfUserDumpWithType:EDumpType_BlockAndBeKilled] stringByAppendingPathComponent:newLagLastPathComponent];

    if ([jsonData writeToFile:newLagFilePath atomically:YES]) {
        [fileMgr removeItemAtPath:trueLagFilePath error:nil];
        MatrixInfo(@"Get a oom dump file");
    } else {
        MatrixError(@"save json data to %@ fail", newLagFilePath);
    }
}

@end
