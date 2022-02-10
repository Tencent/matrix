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

#import "MacMatrixManager.h"
#import <Matrix/Matrix.h>
#import <Matrix/WCCrashBlockFileHandler.h>
#import "IssueDetailViewController.h"

@interface MacMatrixManager () <MatrixAdapterDelegate, MatrixPluginListenerDelegate>

@property (nonatomic, strong) WCMemoryStatPlugin *memStatPlugin;
@end

@implementation MacMatrixManager

+ (MacMatrixManager *)sharedInstance
{
    static MacMatrixManager *g_mgr = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        g_mgr = [[MacMatrixManager alloc] init];
    });
    return g_mgr;
}

- (void)installMatrix
{    
    [MatrixAdapter sharedInstance].delegate = self;

    MatrixBuilder *curBuilder = [[MatrixBuilder alloc] init];
    curBuilder.pluginListener = self;
    
    WCCrashBlockMonitorConfig *cbConfig = [WCCrashBlockMonitorConfig defaultConfiguration];
    cbConfig.reportStrategy = EWCCrashBlockReportStrategy_All;
    
    WCBlockMonitorConfiguration *blockMonitorConfig = [WCBlockMonitorConfiguration defaultConfig];
    blockMonitorConfig.bMainThreadHandle = YES;
    blockMonitorConfig.bFilterSameStack = YES;
    blockMonitorConfig.triggerToBeFilteredCount = 10;
    blockMonitorConfig.bGetPowerConsumeStack = YES;
    cbConfig.blockMonitorConfiguration = blockMonitorConfig;

    WCCrashBlockMonitorPlugin *cbPlugin = [[WCCrashBlockMonitorPlugin alloc] init];
    cbPlugin.pluginConfig = cbConfig;
    [curBuilder addPlugin:cbPlugin];
    
    WCMemoryStatPlugin *memoryStatPlugin = [[WCMemoryStatPlugin alloc] init];
    [curBuilder addPlugin:memoryStatPlugin];
    
    [[Matrix sharedInstance] addMatrixBuilder:curBuilder];

    [cbPlugin start];
    [memoryStatPlugin start];
    
    _memStatPlugin = memoryStatPlugin;
}

- (WCMemoryStatPlugin *)getMemoryStatPlugin
{
    return _memStatPlugin;
}

// ============================================================================
#pragma mark - MatrixPluginListenerDelegate
// ============================================================================

- (void)onReportIssue:(MatrixIssue *)issue
{
    NSLog(@"get issue: %@", issue);
    
    NSString *currentTilte = @"unknown";
    if ([issue.issueTag isEqualToString:[WCCrashBlockMonitorPlugin getTag]]) {
        if (issue.reportType == EMCrashBlockReportType_Lag) {
            NSMutableString *lagTitle = [@"Lag" mutableCopy];
            if (issue.customInfo != nil) {
                NSString *dumpTypeDes = @"";
                NSNumber *dumpType = [issue.customInfo objectForKey:@"dumptype"];
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
                    case EDumpType_PowerConsume:
                        dumpTypeDes = @"Power Consume Calltree";
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

    NSStoryboard *storageBoard = [NSStoryboard storyboardWithName:@"Main" bundle:nil];
    NSWindowController *detailWindowController = [storageBoard instantiateControllerWithIdentifier:@"IssueDetailWindowController"];
    detailWindowController.window.title = currentTilte;
    IssueDetailViewController *detaiViewController = (IssueDetailViewController *)detailWindowController.contentViewController;
    

    if (issue.dataType == EMatrixIssueDataType_Data) {
        NSString *dataString = [[NSString alloc] initWithData:issue.issueData encoding:NSUTF8StringEncoding];
        [detaiViewController setWithString:dataString];
    } else {
        [detaiViewController setWithFilePath:issue.filePath];
    }
    [detailWindowController showWindow:nil];
    
    [[Matrix sharedInstance] reportIssueComplete:issue success:YES];
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
