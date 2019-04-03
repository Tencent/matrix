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

#import "WCMemoryStatPlugin.h"
#import "WCMemoryStatConfig.h"
#import "WCMemoryRecordManager.h"
#import "MatrixLogDef.h"
#import "MatrixAppRebootAnalyzer.h"
#import "MatrixDeviceInfo.h"

#import "memory_logging.h"
#import "memory_report_generator.h"
#import "logger_internal.h"
#import "dyld_image_info.h"

#define g_matrix_memory_stat_plguin_tag "MemoryStat"

@interface WCMemoryStatPlugin () {
    WCMemoryRecordManager *m_recordManager;

    MemoryRecordInfo *m_lastRecord;
    MemoryRecordInfo *m_currRecord;
}

@property (nonatomic, strong) dispatch_queue_t pluginReportQueue;

@end

@implementation WCMemoryStatPlugin

@dynamic pluginConfig;

- (id)init
{
    self = [super init];
    if (self) {
        m_recordManager = [[WCMemoryRecordManager alloc] init];
        // set the user scene of last record
        m_lastRecord = [m_recordManager getRecordByLaunchTime:[MatrixAppRebootAnalyzer lastAppLaunchTime]];
        if (m_lastRecord) {
            m_lastRecord.userScene = [MatrixAppRebootAnalyzer userSceneOfLastRun];
            [m_recordManager updateRecord:m_lastRecord];
        }
        
        self.pluginReportQueue = dispatch_queue_create("matrix.memorystat", DISPATCH_QUEUE_SERIAL);

        [self deplayTryReportOOMInfo];
    }
    return self;
}

// ============================================================================
#pragma mark - Report
// ============================================================================

- (void)deplayTryReportOOMInfo
{
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        NSDictionary *customInfo = nil;
        if (self.delegate != nil && [self.delegate respondsToSelector:@selector(onMemoryStatPluginGetCustomInfo:)]) {
            customInfo = [self.delegate onMemoryStatPluginGetCustomInfo:self];
        }
        dispatch_async(self.pluginReportQueue, ^{
            if ([MatrixAppRebootAnalyzer lastRebootType] == MatrixAppRebootTypeAppForegroundOOM) {
                MemoryRecordInfo *lastInfo = [self recordOfLastRun];
                if (lastInfo != nil) {
                    NSData *reportData = [lastInfo generateReportDataWithCustomInfo:customInfo];
                    if (reportData != nil) {
                        MatrixIssue *issue = [[MatrixIssue alloc] init];
                        issue.issueTag = [WCMemoryStatPlugin getTag];
                        issue.issueID = [lastInfo recordID];
                        issue.dataType = EMatrixIssueDataType_Data;
                        issue.issueData = reportData;
                        MatrixInfo(@"report memory record : %@", issue);
                        dispatch_async(dispatch_get_main_queue(), ^{
                            [self reportIssue:issue];
                        });
                    }
                }
            }
        });
    });
}

- (MatrixIssue *)uploadReport:(MemoryRecordInfo *)record withCustomInfo:(NSDictionary *)customInfo
{
    if (record == nil) {
        return nil;
    }
    
    NSData *reportData = [record generateReportDataWithCustomInfo:customInfo];
    if (reportData == nil) {
        return nil;
    }
    
    MatrixIssue *issue = [[MatrixIssue alloc] init];
    issue.issueTag = [WCMemoryStatPlugin getTag];
    issue.issueID = [record recordID];
    issue.dataType = EMatrixIssueDataType_Data;
    issue.issueData = reportData;
    MatrixInfo(@"memory record : %@", issue);
    [self reportIssue:issue];
    
    return issue;
}

// ============================================================================
#pragma mark - Current Thread
// ============================================================================

- (void)addTagToCurrentThread:(NSString *)tagString
{
    if (m_currRecord == nil) {
        return;
    }
    MSThreadInfo *threadInfo = [[MSThreadInfo alloc] init];
    threadInfo.threadName = tagString;
    [m_currRecord.threadInfos setObject:threadInfo forKey:[NSString stringWithFormat:@"%u", current_thread_id()]];
    [m_recordManager updateRecord:m_currRecord];
}

- (void)removeTagOfCurrentThread
{
    if (m_currRecord == nil) {
        return;
    }
    [m_currRecord.threadInfos removeObjectForKey:[NSString stringWithFormat:@"%u", current_thread_id()]];
    [m_recordManager updateRecord:m_currRecord];
}

- (uint32_t)getMemoryUsageOfCurrentThread;
{
    return get_current_thread_memory_usage();
}

// ============================================================================
#pragma mark - Record
// ============================================================================

- (NSArray *)recordList
{
    return [m_recordManager recordList];
}

- (MemoryRecordInfo *)recordOfLastRun
{
    return m_lastRecord;
}

- (MemoryRecordInfo *)recordByLaunchTime:(uint64_t)launchTime
{
    return [m_recordManager getRecordByLaunchTime:launchTime];
}

- (void)deleteRecord:(MemoryRecordInfo *)record
{
    [m_recordManager deleteRecord:record];
}

- (void)deleteAllRecords
{
    [m_recordManager deleteAllRecords];
}

// ============================================================================
#pragma mark - Private
// ============================================================================

- (void)setCurrentRecordInvalid
{
    if (m_currRecord == nil) {
        return;
    }
    
    [m_recordManager deleteRecord:m_currRecord];
    m_currRecord = nil;
}

- (void)reportError:(int)errorCode
{
    [self.delegate onMemoryStatPlugin:self hasError:errorCode];
}

// ============================================================================
#pragma mark - MatrixPluginProtocol
// ============================================================================

- (void)start
{
    if ([MatrixDeviceInfo isBeingDebugged]) {
        MatrixDebug(@"app is being debugged, cannot start memstat");
        return;
    }
    
    if (m_currRecord != nil) {
        return;
    }
    
    [super start];
    
    int ret = MS_ERRC_SUCCESS;

    if (self.pluginConfig == nil) {
        self.pluginConfig = [WCMemoryStatConfig defaultConfiguration];
    }
    if (self.pluginConfig) {
        skip_max_stack_depth = self.pluginConfig.skipMaxStackDepth;
        skip_min_malloc_size = self.pluginConfig.skipMinMallocSize;
    }

    m_currRecord = [[MemoryRecordInfo alloc] init];
    m_currRecord.launchTime = [MatrixAppRebootAnalyzer appLaunchTime];
    m_currRecord.systemVersion = [MatrixDeviceInfo systemVersion];
    m_currRecord.appUUID = @(app_uuid());

    NSString *dataPath = [m_currRecord recordDataPath];
    [[NSFileManager defaultManager] removeItemAtPath:dataPath error:nil];
    [[NSFileManager defaultManager] createDirectoryAtPath:dataPath withIntermediateDirectories:YES attributes:nil error:nil];

    if ((ret = enable_memory_logging(dataPath.UTF8String)) == MS_ERRC_SUCCESS) {
        [m_recordManager insertNewRecord:m_currRecord];
    } else {
        MatrixError(@"MemStatPlugin start error: %d", ret);
        disable_memory_logging();
        [self.delegate onMemoryStatPlugin:self hasError:ret];
        [[NSFileManager defaultManager] removeItemAtPath:dataPath error:nil];
        m_currRecord = nil;
    }
}

- (void)stop
{
    [super stop];
    if (m_currRecord == nil) {
        return;
    }
    [self deleteRecord:m_currRecord];
    m_currRecord = nil;
    disable_memory_logging();
}

- (void)destroy
{
    [super destroy];
}

- (void)setupPluginListener:(id<MatrixPluginListenerDelegate>)pluginListener
{
    [super setupPluginListener:pluginListener];
}

- (void)reportIssue:(MatrixIssue *)issue
{
    [super reportIssue:issue];
}

- (void)reportIssueCompleteWithIssue:(MatrixIssue *)issue success:(BOOL)bSuccess
{
    if (bSuccess) {
        MatrixInfo(@"report issuse success: %@", issue);
    } else {
        MatrixInfo(@"report issuse failed: %@", issue);
    }
    if ([issue.issueTag isEqualToString:[WCMemoryStatPlugin getTag]]) {
        if (bSuccess) {
            // do nothing
        } else {
            MatrixError(@"report issue failed, do not delete, %@", [WCMemoryStatPlugin getTag]);
        }
    } else {
        MatrixInfo(@"the issue is not my duty");
    }
}

+ (NSString *)getTag
{
    return @g_matrix_memory_stat_plguin_tag;
}

@end
