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

#import "WCCrashBlockMonitorPlugin.h"
#import "MatrixLogDef.h"
#import "MatrixPathUtil.h"
#import "WCCrashBlockFileHandler.h"
#import "WCDumpReportDataProvider.h"
#import "WCCrashBlockMonitor.h"

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

#define g_matrix_crash_block_plguin_tag "MatrixCrashBlock"

#define kTodayOneTypeFilesLimit 3
#define kReportCountLimitInOneIssue 3
#define kReportMuchCountLimit 20

typedef NS_ENUM(NSUInteger, EDumpUploadType) {
    EDumpUploadType_None = 0,
    EDumpUploadType_All = 1,
    EDumpUploadType_SpecificType = 2,
    EDumpUploadType_Today = 3,
};

@interface WCCrashBlockMonitorPlugin ()

@property (nonatomic, strong) WCCrashBlockMonitor *cbMonitor;
@property (nonatomic, strong) NSMutableArray *uploadingLagFileIDArray;
@property (nonatomic, strong) NSMutableArray *uploadingCrashFileIDArray;
@property (nonatomic, strong) dispatch_queue_t pluginReportQueue;

@property (nonatomic, assign) EDumpUploadType dumpUploadType;
@property (nonatomic, assign) EDumpType currentUploadType;
@property (nonatomic, copy) NSString *currentUploadDate;

@end

@implementation WCCrashBlockMonitorPlugin

@dynamic pluginConfig;

// ============================================================================
#pragma mark - MatrixPluginProtocol
// ============================================================================

- (id)init
{
    self = [super init];
    if (self) {
        self.uploadingLagFileIDArray = [[NSMutableArray alloc] init];
        self.uploadingCrashFileIDArray = [[NSMutableArray alloc] init];
        self.pluginReportQueue = dispatch_queue_create("matrix.crashblock", DISPATCH_QUEUE_SERIAL);
        self.dumpUploadType = EDumpUploadType_None;
        self.currentUploadType = EDumpType_Unlag;
        self.currentUploadDate = @"";
    }
    return self;
}

- (void)start
{
    if (_cbMonitor != nil) {
        return;
    }
    if (self.pluginConfig == nil) {
        self.pluginConfig = [WCCrashBlockMonitorConfig defaultConfiguration];
    }
    if (self.pluginConfig != nil) {
        _cbMonitor = [[WCCrashBlockMonitor alloc] init];
        _cbMonitor.appVersion = [self.pluginConfig appVersion];
        _cbMonitor.appShortVersion = [self.pluginConfig appShortVersion];
        _cbMonitor.delegate = [self.pluginConfig blockMonitorDelegate];
        _cbMonitor.onHandleSignalCallBack = [self.pluginConfig onHandleSignalCallBack];
        _cbMonitor.onAppendAdditionalInfoCallBack = [self.pluginConfig onAppendAdditionalInfoCallBack];
        _cbMonitor.bmConfiguration = [self.pluginConfig blockMonitorConfiguration];
        [_cbMonitor installKSCrash:[self.pluginConfig enableCrash]];
        if ([self.pluginConfig enableBlockMonitor]) {
            [_cbMonitor enableBlockMonitor];
        }
    }
    [super start];

    [self delayReportCrash];
    [self autoClean];
    
#if !TARGET_OS_OSX
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(notifyAppEnterForeground)
                                                 name:UIApplicationDidBecomeActiveNotification
                                               object:nil];
#else
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(notifyAppEnterForeground)
                                                 name:NSApplicationDidBecomeActiveNotification
                                               object:nil];
#endif
}

- (void)stop
{
    [super stop];
}

- (void)setupPluginListener:(id<MatrixPluginListenerDelegate>)pluginListener
{
    [super setupPluginListener:pluginListener];
}

- (void)destroy
{
    [super destroy];
}

- (void)reportIssue:(MatrixIssue *)issue
{
    [super reportIssue:issue];
}

+ (NSString *)getTag
{
    return @g_matrix_crash_block_plguin_tag;
}

- (void)reportIssueCompleteWithIssue:(MatrixIssue *)issue success:(BOOL)bSuccess
{
    dispatch_async(self.pluginReportQueue, ^{
        if (bSuccess) {
            MatrixInfo(@"report issuse success: %@", issue);
        } else {
            MatrixInfo(@"report issuse failed: %@", issue);
        }
        if ([issue.issueTag isEqualToString:[WCCrashBlockMonitorPlugin getTag]]) {
            if (issue.reportType == EMCrashBlockReportType_Crash) {
                if (bSuccess) {
                    MatrixInfo(@"delete crash: reportID: %@", issue.issueID);
                    [WCCrashBlockFileHandler deleteCrashDataWithReportID:issue.issueID];
                }
                [self removeCrashFromUploadingArray:issue.issueID];
                if (bSuccess) {
                    if ([self.uploadingCrashFileIDArray count] < 1) {
                        [self delayReportCrash];
                    }
                }
            } else {
                NSDictionary *customInfo = [issue customInfo];
                if (customInfo == nil) {
                    MatrixError(@"no custom info, have error");
                }
                NSNumber *dumpType = customInfo[@g_crash_block_monitor_custom_dump_type];
                NSArray *reportIDS = customInfo[@g_crash_block_monitor_custom_report_id];

                if (dumpType != nil && reportIDS != nil && [reportIDS count] > 0) {
                    for (NSString *reportID in reportIDS) {
                        if (bSuccess) {
                            [WCCrashBlockFileHandler deleteLagDataWithReportID:reportID andReportType:EDumpType([dumpType intValue])];
                        }
                        [self removeLagFromUploadingArray:reportID];
                    }
                }
                if (bSuccess) {
                    MatrixInfo(@"delete lag: dumpType: %d, reportIDS: %@", [dumpType intValue], reportIDS)
                    if ([self.uploadingLagFileIDArray count] < 1) {
                        if (self.dumpUploadType == EDumpUploadType_All) {
                            self.dumpUploadType = EDumpUploadType_None;
                            [self reportAllLagFileOnDate:self.currentUploadDate];
                        }
                        if (self.dumpUploadType == EDumpUploadType_SpecificType) {
                            self.dumpUploadType = EDumpUploadType_None;
                            [self reportOneTypeLag:self.currentUploadType onDate:self.currentUploadDate];
                        }
                        if (self.dumpUploadType == EDumpUploadType_Today) {
                            self.dumpUploadType = EDumpUploadType_None;
                            // just autoUpload will use today
                        }

                    }
                }
            }
        } else {
            MatrixError(@"the issue is not my duty");
        }
    });
}

// ============================================================================
#pragma mark - Utility
// ============================================================================

- (void)resetAppFullVersion:(NSString *)fullVersion shortVersion:(NSString *)shortVersion
{
    [_cbMonitor resetAppFullVersion:fullVersion shortVersion:shortVersion];
}

// ============================================================================
#pragma mark - Optimization
// ============================================================================

#if !TARGET_OS_OSX

- (void)handleBackgroundLaunch
{
    [_cbMonitor handleBackgroundLaunch];
}

- (void)handleSuspend
{
    [_cbMonitor handleSuspend];
}

#endif

// ============================================================================
#pragma mark - Control of Block Monitor
// ============================================================================

- (void)startBlockMonitor
{
    [_cbMonitor startBlockMonitor];
}

- (void)stopBlockMonitor
{
    [_cbMonitor stopBlockMonitor];
}

// ============================================================================
#pragma mark - CPU
// ============================================================================

- (void)startTrackCPU
{
    [_cbMonitor startTrackCPU];
}

- (void)stopTrackCPU
{
    [_cbMonitor stopTrackCPU];
}

- (BOOL)isBackgroundCPUTooSmall
{
    return [_cbMonitor isBackgroundCPUTooSmall];
}

// ============================================================================
#pragma mark - Custom Dump
// ============================================================================

- (void)generateLiveReportWithDumpType:(EDumpType)dumpType
                            withReason:(NSString *)reason
                       selfDefinedPath:(BOOL)bSelfDefined
{
    [_cbMonitor generateLiveReportWithDumpType:dumpType withReason:reason selfDefinedPath:bSelfDefined];
}

// ============================================================================
#pragma mark - ReportID Uploading
// ============================================================================

- (BOOL)isLagUploading:(NSString *)reportID
{
    for (NSString *exReportID in self.uploadingLagFileIDArray) {
        if ([reportID isEqualToString:exReportID]) {
            return YES;
        }
    }
    return NO;
}

- (void)addLagToUploadingArray:(NSString *)reportID
{
    if (reportID == nil || [reportID length] == 0) {
        return;
    }
    if ([self isLagUploading:reportID] == NO) {
        MatrixInfo(@"add file report id uploading: %@", reportID);
        [self.uploadingLagFileIDArray addObject:reportID];
    }
}

- (void)removeLagFromUploadingArray:(NSString *)reportID
{
    if (reportID == nil || [reportID length] == 0) {
        return;
    }
    MatrixInfo(@"remove file report id from uploading array: %@", reportID);
    [self.uploadingLagFileIDArray removeObject:reportID];
}

- (BOOL)isCrashUploading:(NSString *)reportID
{
    for (NSString *exReportID in self.uploadingCrashFileIDArray) {
        if ([reportID isEqualToString:exReportID]) {
            return YES;
        }
    }
    return NO;
}

- (void)addCrashToUploadingArray:(NSString *)reportID
{
    if (reportID == nil || [reportID length] == 0) {
        return;
    }
    if ([self isCrashUploading:reportID] == NO) {
        MatrixInfo(@"add file report id uploading: %@", reportID);
        [self.uploadingCrashFileIDArray addObject:reportID];
    }
}

- (void)removeCrashFromUploadingArray:(NSString *)reportID
{
    if (reportID == nil || [reportID length] == 0) {
        return;
    }
    MatrixInfo(@"remove file report id from uploading array: %@", reportID);
    [self.uploadingCrashFileIDArray removeObject:reportID];
}


// ============================================================================
#pragma mark - Report Issue
// ============================================================================

- (void)notifyAppEnterForeground
{
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(3 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        switch (self.pluginConfig.reportStrategy) {
            case EWCCrashBlockReportStrategy_Auto:
                [self autoReportLag];
                break;
            case EWCCrashBlockReportStrategy_All:
                [self reportAllLagFile];
                break;
            case EWCCrashBlockReportStrategy_Manual:
                // do nothing
                break;
            default:
                break;
        }
    });
}

- (void)delayReportCrash
{
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        switch (self.pluginConfig.reportStrategy) {
            case EWCCrashBlockReportStrategy_Auto:
                [self autoReportCrash];
                break;
            case EWCCrashBlockReportStrategy_All:
                [self reportCrash];
                break;
            case EWCCrashBlockReportStrategy_Manual:
                // do nothing
                break;
            default:
                break;
        }
    });
}

- (void)autoClean
{
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(10 * NSEC_PER_SEC)), self.pluginReportQueue, ^{
        // 自动清理 10天 以外的 crash 和 lag 文件
        [MatrixPathUtil autoCleanDiretory:[MatrixPathUtil crashBlockPluginCachePath] withTimeout:10 * 86400];
    });
}

// ============================================================================
#pragma mark - Crash
// ============================================================================

- (void)autoReportCrash
{
    dispatch_async(self.pluginReportQueue, ^{
        if ([WCCrashBlockFileHandler hasCrashReport]) {
            if (self.reportDelegate) {
                if ([self.reportDelegate isReportCrashLimit:self]) {
                    MatrixInfo(@"report crash limit");
                    return;
                }
            }
            [self reportCrash];
        }
    });
}

- (void)reportCrash
{
    dispatch_async(self.pluginReportQueue, ^{
        if ([WCCrashBlockFileHandler hasCrashReport]) {
            NSDictionary *crashDataDic = [WCCrashBlockFileHandler getPendingCrashReportInfo];
            if (crashDataDic == nil) {
                MatrixInfo(@"get crash data nil");
                return;
            }
            NSString *newestReportID = crashDataDic[@"reportID"];
            if ([self isCrashUploading:newestReportID]) {
                MatrixInfo(@"report id uploading: %@", newestReportID);
                return;
            }
            [self addCrashToUploadingArray:newestReportID];
            NSData *crashData = crashDataDic[@"crashData"];
            MatrixIssue *issue = [[MatrixIssue alloc] init];
            issue.issueTag = [WCCrashBlockMonitorPlugin getTag];
            issue.issueID = newestReportID;
            issue.reportType = EMCrashBlockReportType_Crash;
            issue.dataType = EMatrixIssueDataType_Data;
            issue.issueData = crashData;
            MatrixInfo(@"report crash : %@", issue);
            dispatch_async(dispatch_get_main_queue(), ^{
                [self reportIssue:issue];
            });
        }
    });
}

+ (BOOL)hasCrashReport
{
    return [WCCrashBlockFileHandler hasCrashReport];
}

// ============================================================================
#pragma mark - Lag & Lag File
// ============================================================================

+ (BOOL)haveLagFiles
{
    return [WCCrashBlockFileHandler haveLagFiles];
}

+ (BOOL)haveLagFilesOnDate:(NSString *)nsDate
{
    return [WCCrashBlockFileHandler haveLagFilesOnDate:nsDate];
}

+ (BOOL)haveLagFilesOnType:(EDumpType)dumpType
{
    return [WCCrashBlockFileHandler haveLagFilesOnType:dumpType];
}

+ (BOOL)haveLagFilesOnDate:(NSString *)nsDate onType:(EDumpType)dumpType
{
    return [WCCrashBlockFileHandler haveLagFilesOnDate:nsDate onType:dumpType];
}

- (void)autoReportLag
{
    dispatch_async(self.pluginReportQueue, ^{
        if (self.reportDelegate) {
            if ([self.reportDelegate isReportLagLimit:self]) {
                MatrixInfo(@"report lag limit");
                return;
            }
            if ([self.reportDelegate isCanAutoReportLag:self] == NO) {
                MatrixInfo(@"can not auto report lag");
                return;
            }
            if ([self.reportDelegate isNetworkAllowAutoReportLag:self] == NO) {
                MatrixInfo(@"report lag, network not allow");
                return;
            }
        }
        [self reportTodayOneTypeLag];
    });
}

- (void)reportAllLagFile
{
    [self reportAllLagFileOnDate:@""];
}

- (void)reportAllLagFileOnDate:(NSString *)nsDate
{
    dispatch_async(self.pluginReportQueue, ^{
        if (self.dumpUploadType != EDumpUploadType_None) {
            MatrixInfo(@"uploading lag, return");
            return;
        }
        NSArray *reportDataArray = [WCDumpReportDataProvider getAllTypeReportDataWithDate:nsDate];
        if (reportDataArray == nil || [reportDataArray count] <= 0) {
            MatrixInfo(@"no lag can upload");
            return;
        }
        MatrixInfo(@"report lag on date: %@,  report data:\n %@", nsDate, reportDataArray);

        NSMutableArray *matrixIssueArray = [[NSMutableArray alloc] init];
        for (WCDumpReportTaskData *taskData in reportDataArray) {
            if (taskData == nil || [taskData.m_uploadFilesArray count] == 0) {
                continue;
            }
            if ([taskData.m_uploadFilesArray count] >= kReportMuchCountLimit) { // prevent from out-of-memory
                NSMutableArray *trimFilesArray = [[NSMutableArray alloc] init];
                for (NSUInteger i = 0; i < kReportMuchCountLimit; i++) {
                    [trimFilesArray addObject:taskData.m_uploadFilesArray[i]];
                }
                taskData.m_uploadFilesArray = trimFilesArray;
            }
            NSArray *matrixIssue = [self getMatrixIssueFromReportTaskData:taskData
                                                           withReportType:EMCrashBlockReportType_Lag
                                                              quickUpload:NO];
            [matrixIssueArray addObjectsFromArray:matrixIssue];
        }
        
        self.dumpUploadType = EDumpUploadType_All;
        self.currentUploadDate = nsDate;
        
        NSArray *retIssueArray = [matrixIssueArray copy];
        for (MatrixIssue *issue in retIssueArray) {
            MatrixInfo(@"report dump : %@", issue);
            dispatch_async(dispatch_get_main_queue(), ^{
                [self reportIssue:issue];
            });
        }
    });
}

- (void)reportOneTypeLag:(EDumpType)dumpType
                  onDate:(NSString *)nsDate
{
    dispatch_async(self.pluginReportQueue, ^{
        if (self.dumpUploadType != EDumpUploadType_None) {
            MatrixInfo(@"uploading lag, return");
            return;
        }
        
        NSArray *reportDataArray = [WCDumpReportDataProvider getReportDataWithType:dumpType onDate:nsDate];
        if (reportDataArray == nil || [reportDataArray count] <= 0) {
            MatrixInfo(@"no lag can upload");
            return;
        }
        
        MatrixInfo(@"report lag one type %lu on date %@ ", (unsigned long) dumpType, nsDate);

        NSMutableArray *matrixIssueArray = [[NSMutableArray alloc] init];
        for (WCDumpReportTaskData *taskData in reportDataArray) {
            if (taskData == nil || [taskData.m_uploadFilesArray count] == 0) {
                continue;
            }
            if ([taskData.m_uploadFilesArray count] >= kReportMuchCountLimit) { // prevent from out-of-memory
                NSMutableArray *trimFilesArray = [[NSMutableArray alloc] init];
                for (NSUInteger i = 0; i < kReportMuchCountLimit; i++) {
                    [trimFilesArray addObject:taskData.m_uploadFilesArray[i]];
                }
                taskData.m_uploadFilesArray = trimFilesArray;
            }
            NSArray *matrixIssue = [self getMatrixIssueFromReportTaskData:taskData
                                                           withReportType:EMCrashBlockReportType_Lag
                                                              quickUpload:NO];
            [matrixIssueArray addObjectsFromArray:matrixIssue];
        }
        
        self.dumpUploadType = EDumpUploadType_SpecificType;
        self.currentUploadType = dumpType;
        self.currentUploadDate = nsDate;

        NSArray *retIssueArray = [matrixIssueArray copy];
        for (MatrixIssue *issue in retIssueArray) {
            MatrixInfo(@"report dump : %@", issue);
            dispatch_async(dispatch_get_main_queue(), ^{
                [self reportIssue:issue];
            });
        }
    });
}

- (void)reportTodayOneTypeLag
{
    [self reportTodayOneTypeLagWithlimitUploadConfig:@""];
}

- (void)reportTodayOneTypeLagWithlimitUploadConfig:(NSString *)limitConfigStr
{
    [self reportTodayOneTypeLagWithlimitUploadConfig:limitConfigStr
                                withLimitReportCount:kTodayOneTypeFilesLimit];
}

- (void)reportTodayOneTypeLagWithlimitUploadConfig:(NSString *)limitConfigStr
                              withLimitReportCount:(NSUInteger)reportCntLimit
{
    dispatch_async(self.pluginReportQueue, ^{
        if (self.dumpUploadType != EDumpUploadType_None) {
            MatrixInfo(@"uploading lag, return");
            return;
        }
        
        WCDumpReportTaskData *reportData = [WCDumpReportDataProvider getTodayOneReportDataWithLimitType:limitConfigStr];
        if (reportData == nil || [reportData.m_uploadFilesArray count] == 0) {
            MatrixInfo(@"no lag can upload");
            return;
        }
        NSMutableArray *limitFilesArray = [[NSMutableArray alloc] init];
        uint32_t currentCount = 0;
        for (NSString *currentFileName in reportData.m_uploadFilesArray) {
            if (currentCount < reportCntLimit) {
                [limitFilesArray addObject:currentFileName];
            }
            currentCount += 1;
        }
        MatrixInfo(@"report today one type: %@,limit: %lu, limitFilesCount: %lu, filesCount: %lu",
                   limitConfigStr, (unsigned long) reportCntLimit, (unsigned long) [limitFilesArray count], (unsigned long) [reportData.m_uploadFilesArray count]);
        reportData.m_uploadFilesArray = limitFilesArray;
        NSArray *matrixIssueArray = [self getMatrixIssueFromReportTaskData:reportData
                                                            withReportType:EMCrashBlockReportType_Lag
                                                               quickUpload:NO];
        
        self.dumpUploadType = EDumpUploadType_Today;

        for (MatrixIssue *issue in matrixIssueArray) {
            MatrixInfo(@"report dump : %@", issue);
            dispatch_async(dispatch_get_main_queue(), ^{
                [self reportIssue:issue];
            });
        }
    });
}

// ============================================================================
#pragma mark - DumpReportData To Matrix Issue
// ============================================================================

- (NSArray *)getMatrixIssueFromReportTaskData:(WCDumpReportTaskData *)taskData
                               withReportType:(int)reportType
                                  quickUpload:(BOOL)bQuick
{
    NSMutableArray *issueArray = [[NSMutableArray alloc] init];

    NSArray *dumpItems = taskData.m_uploadFilesArray;
    if ([dumpItems count] > 0) {
        if (bQuick) {
            for (NSUInteger index = 0; index < dumpItems.count; index++) {
                NSString *fileReportID = dumpItems[index];
                if (fileReportID != nil && [fileReportID length] > 0) {
                    if ([self isLagUploading:fileReportID]) {
                        MatrixInfo(@"report id uploading: %@", fileReportID);
                        continue;
                    }
                    [self addLagToUploadingArray:fileReportID];
                    MatrixIssue *currentIssue = [[MatrixIssue alloc] init];
                    currentIssue.reportType = reportType;
                    currentIssue.issueTag = [WCCrashBlockMonitorPlugin getTag];
                    currentIssue.issueID = fileReportID;
                    currentIssue.dataType = EMatrixIssueDataType_Data;
                    currentIssue.issueData = [WCCrashBlockFileHandler getLagDataWithReportID:fileReportID andReportType:taskData.m_dumpType];
                    currentIssue.customInfo = @{ @g_crash_block_monitor_custom_file_count : @1,
                                                 @g_crash_block_monitor_custom_dump_type : [NSNumber numberWithUnsignedInteger:taskData.m_dumpType],
                                                 @g_crash_block_monitor_custom_report_id : @[ fileReportID ]
                    };

                    [issueArray addObject:currentIssue];
                }
            }
        } else {
            NSMutableArray *currentUploadingItems = [[NSMutableArray alloc] init];
            for (NSUInteger index = 0; index < dumpItems.count; index++) {
                if (currentUploadingItems.count >= kReportCountLimitInOneIssue) {
                    MatrixIssue *currentIssue = [[MatrixIssue alloc] init];
                    currentIssue.reportType = reportType;
                    currentIssue.issueID = [currentUploadingItems firstObject];
                    currentIssue.issueTag = [WCCrashBlockMonitorPlugin getTag];
                    currentIssue.dataType = EMatrixIssueDataType_Data;
                    currentIssue.issueData = [WCCrashBlockFileHandler getLagDataWithReportIDArray:[currentUploadingItems copy]
                                                                                    andReportType:taskData.m_dumpType];
                    currentIssue.customInfo = @{ @g_crash_block_monitor_custom_file_count : [NSNumber numberWithUnsignedInteger:currentUploadingItems.count],
                                                 @g_crash_block_monitor_custom_dump_type : [NSNumber numberWithUnsignedInteger:taskData.m_dumpType],
                                                 @g_crash_block_monitor_custom_report_id : [currentUploadingItems copy]
                    };
                    [issueArray addObject:currentIssue];
                    [currentUploadingItems removeAllObjects];
                    currentUploadingItems = [[NSMutableArray alloc] init];
                }
                NSString *fileReportID = dumpItems[index];
                if (fileReportID != nil && [fileReportID length] > 0) {
                    if ([self isLagUploading:fileReportID]) {
                        MatrixInfo(@"report id uploading: %@", fileReportID);
                        continue;
                    }
                    [self addLagToUploadingArray:fileReportID];
                    [currentUploadingItems addObject:dumpItems[index]];
                }
            }
            if ([currentUploadingItems count] > 0) {
                MatrixIssue *currentIssue = [[MatrixIssue alloc] init];
                currentIssue.reportType = reportType;
                currentIssue.issueTag = [WCCrashBlockMonitorPlugin getTag];
                currentIssue.issueID = [currentUploadingItems firstObject];
                currentIssue.dataType = EMatrixIssueDataType_Data;
                currentIssue.issueData = [WCCrashBlockFileHandler getLagDataWithReportIDArray:[currentUploadingItems copy]
                                                                                andReportType:taskData.m_dumpType];
                currentIssue.customInfo = @{ @g_crash_block_monitor_custom_file_count : [NSNumber numberWithUnsignedInteger:currentUploadingItems.count],
                                             @g_crash_block_monitor_custom_dump_type : [NSNumber numberWithUnsignedInteger:taskData.m_dumpType],
                                             @g_crash_block_monitor_custom_report_id : [currentUploadingItems copy]
                };
                [issueArray addObject:currentIssue];
            }
        }
    }
    return [issueArray copy];
}

@end
