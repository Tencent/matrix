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

#import "MatrixHandler.h"
#import <matrix-apple/WCCrashBlockFileHandler.h>
#import <UIKit/UIKit.h>
#import "AppDelegate.h"
#import "TextViewController.h"

void kscrash_crashCallback(const KSCrashReportWriter *writer)
{
    writer->beginObject(writer, "WeChat");
    writer->addUIntegerElement(writer, "uin", 21002);
    writer->endContainer(writer);
}

@interface MatrixHandler () <WCCrashBlockMonitorDelegate, MatrixAdapterDelegate, MatrixPluginListenerDelegate>
{
    WCCrashBlockMonitorPlugin *m_cbPlugin;
    WCMemoryStatPlugin *m_msPlugin;
}

@end

@implementation MatrixHandler

+ (MatrixHandler *)sharedInstance
{
    static MatrixHandler *g_handler = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        g_handler = [[MatrixHandler alloc] init];
    });
    
    return g_handler;
}

- (void)installMatrix
{
    // Get Matrix's log
    [MatrixAdapter sharedInstance].delegate = self;
    
    Matrix *matrix = [Matrix sharedInstance];

    MatrixBuilder *curBuilder = [[MatrixBuilder alloc] init];
    curBuilder.pluginListener = self;
    
    WCCrashBlockMonitorConfig *crashBlockConfig = [[WCCrashBlockMonitorConfig alloc] init];
    crashBlockConfig.enableCrash = YES;
    crashBlockConfig.enableBlockMonitor = YES;
    crashBlockConfig.blockMonitorDelegate = self;
    crashBlockConfig.onAppendAdditionalInfoCallBack = kscrash_crashCallback;
    crashBlockConfig.reportStrategy = EWCCrashBlockReportStrategy_All;
    
    WCBlockMonitorConfiguration *blockMonitorConfig = [WCBlockMonitorConfiguration defaultConfig];
    blockMonitorConfig.bMainThreadHandle = YES;
    blockMonitorConfig.bFilterSameStack = YES;
    blockMonitorConfig.triggerToBeFilteredCount = 10;
    crashBlockConfig.blockMonitorConfiguration = blockMonitorConfig;
    
    WCCrashBlockMonitorPlugin *crashBlockPlugin = [[WCCrashBlockMonitorPlugin alloc] init];
    crashBlockPlugin.pluginConfig = crashBlockConfig;
    [curBuilder addPlugin:crashBlockPlugin];
    
    WCMemoryStatPlugin *memoryStatPlugin = [[WCMemoryStatPlugin alloc] init];
    memoryStatPlugin.pluginConfig = [WCMemoryStatConfig defaultConfiguration];
    [curBuilder addPlugin:memoryStatPlugin];
    
    [matrix addMatrixBuilder:curBuilder];
    
    [crashBlockPlugin start];
    [memoryStatPlugin start];
    
    m_cbPlugin = crashBlockPlugin;
    m_msPlugin = memoryStatPlugin;
}

- (WCCrashBlockMonitorPlugin *)getCrashBlockPlugin;
{
    return m_cbPlugin;
}

- (WCMemoryStatPlugin *)getMemoryStatPlugin
{
    return m_msPlugin;
}

// ============================================================================
#pragma mark - MatrixPluginListenerDelegate
// ============================================================================

- (void)onReportIssue:(MatrixIssue *)issue
{
    NSLog(@"get issue: %@", issue);
    
    AppDelegate *appDelegate = (AppDelegate *)[[UIApplication sharedApplication] delegate];
    TextViewController *textVC = nil;
    
    NSString *currentTilte = @"unknown";
    
    if ([issue.issueTag isEqualToString:[WCCrashBlockMonitorPlugin getTag]]) {
        if (issue.reportType == EMCrashBlockReportType_Lag) {
            NSMutableString *lagTitle = [@"Lag" mutableCopy];
            if (issue.customInfo != nil) {
                NSString *dumpTypeDes = @"";
                NSNumber *dumpType = [issue.customInfo objectForKey:@g_crash_block_monitor_custom_dump_type];
                switch (EDumpType(dumpType.integerValue)) {
                    case EDumpType_MainThreadBlock:
                        dumpTypeDes = @"Foreground Main Thread Block";
                        break;
                    case EDumpType_BackgroundMainThreadBlock:
                        dumpTypeDes = @"Background Main Thread Block";
                        break;
                    case EDumpType_CPUBlock:
                        dumpTypeDes = @"CPU Too High";
                        break;
                    case EDumpType_CPUIntervalHigh:
                        dumpTypeDes = @"CPU Interval High";
                        break;
                    case EDumpType_LaunchBlock:
                        dumpTypeDes = @"Launching Main Thread Block";
                        break;
                    case EDumpType_BlockThreadTooMuch:
                        dumpTypeDes = @"Block And Thread Too Much";
                        break;
                    case EDumpType_BlockAndBeKilled:
                        dumpTypeDes = @"Main Thread Block Before Be Killed";
                        break;
                    default:
                        dumpTypeDes = [NSString stringWithFormat:@"%d", [dumpType intValue]];
                        break;
                }
                [lagTitle appendFormat:@" [%@]", dumpTypeDes];
            }
            currentTilte = [lagTitle copy];
        }
        if (issue.reportType == EMCrashBlockReportType_Crash) {
            currentTilte = @"Crash";
        }
    }
    
    if ([issue.issueTag isEqualToString:[WCMemoryStatPlugin getTag]]) {
        currentTilte = @"OOM Info";
    }
    
    if (issue.dataType == EMatrixIssueDataType_Data) {
        NSString *dataString = [[NSString alloc] initWithData:issue.issueData encoding:NSUTF8StringEncoding];
        textVC = [[TextViewController alloc] initWithString:dataString withTitle:currentTilte];
    } else {
        textVC = [[TextViewController alloc] initWithFilePath:issue.filePath withTitle:currentTilte];
    }
    [appDelegate.navigationController pushViewController:textVC animated:YES];
    
    [[Matrix sharedInstance] reportIssueComplete:issue success:YES];
}

// ============================================================================
#pragma mark - WCCrashBlockMonitorDelegate
// ============================================================================

- (void)onCrashBlockMonitorBeginDump:(EDumpType)dumpType blockTime:(uint64_t)blockTime
{
    
}

- (void)onCrashBlockMonitorEnterNextCheckWithDumpType:(EDumpType)dumpType
{
    if (dumpType != EDumpType_MainThreadBlock || dumpType != EDumpType_BackgroundMainThreadBlock) {
    }
}

- (void)onCrashBlockMonitorDumpType:(EDumpType)dumpType filter:(EFilterType)filterType
{
    NSLog(@"filtered dump type:%u, filter type: %u", (uint32_t)dumpType, (uint32_t)filterType);
}

- (void)onCrashBlockMonitorDumpFilter:(EDumpType)dumpType
{
    
}

- (NSDictionary *)onCrashBlockGetUserInfoForFPSWithDumpType:(EDumpType)dumpType
{
    return nil;
}

// ============================================================================
#pragma mark - MatrixAdapterDelegate
// ============================================================================

- (BOOL)matrixShouldLog:(MXLogLevel)level
{
    return YES;
}

- (void)matrixLog:(MXLogLevel)logLevel
           module:(const char *)module
             file:(const char *)file
             line:(int)line
         funcName:(const char *)funcName
          message:(NSString *)message
{
    NSLog(@"%@:%@:%@:%@",
          [NSString stringWithUTF8String:module],[NSString stringWithUTF8String:file],[NSString stringWithUTF8String:funcName], message);
}

@end
