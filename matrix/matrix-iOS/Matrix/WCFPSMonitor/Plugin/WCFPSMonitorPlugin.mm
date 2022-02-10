/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
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

#import "WCFPSMonitorPlugin.h"
#import "MatrixLogDef.h"
#import "WCFPSLogger.h"

#import <UIKit/UIApplication.h>
#import <pthread/pthread.h>

#define g_matrix_fps_monitor_plugin_tag @"FPSMonitor"

@interface WCFPSMonitorPlugin () <WCFPSWriterDelegate> {
    int m_nextRecordID;
    WCFPSRecorder *m_currRecorder;

    double m_beginTime;
    double m_lastTime;
    double m_dropTime;
    CADisplayLink *m_displayLink;
    NSString *m_scene;

    NSMutableDictionary<NSString *, NSNumber *> *m_sceneMaxTimeDict; //
    dispatch_queue_t m_pluginReportQueue;
}

@end

@implementation WCFPSMonitorPlugin

@dynamic pluginConfig;

- (id)init {
    self = [super init];
    if (self) {
        m_sceneMaxTimeDict = [NSMutableDictionary new];
        m_pluginReportQueue = dispatch_queue_create("matrix.fpsmonitor", DISPATCH_QUEUE_SERIAL);
    }
    return self;
}

- (BOOL)start {
    if ([super start] == NO) {
        return NO;
    }

    assert([NSThread isMainThread]);
    init_fps_monitor(self);
    return YES;
}

- (void)stop {
    [super stop];
}

+ (NSString *)getTag {
    return g_matrix_fps_monitor_plugin_tag;
}

- (void)startDisplayLink:(NSString *)scene {
    FPSInfo(@"startDisplayLink");

    m_displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(onFrameCallback:)];
    [m_displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSRunLoopCommonModes];

    m_beginTime = CFAbsoluteTimeGetCurrent();
    m_lastTime = m_beginTime;
    m_dropTime = 0;
    m_scene = scene;

    [self createFPSThread];
}

- (void)stopDisplayLink {
    FPSInfo(@"stopDisplayLink");

    double nowTime = CFAbsoluteTimeGetCurrent();
    double diff = (nowTime - m_lastTime) * 1000;

    if (diff > self.pluginConfig.maxFrameInterval) {
        m_currRecorder.dumpTimeTotal += diff;
        m_dropTime += self.pluginConfig.maxFrameInterval * pow(diff / self.pluginConfig.maxFrameInterval, self.pluginConfig.powFactor);
    }

    // 连续 N 个掉帧总耗时超过阈值
    if (m_currRecorder.dumpTimeTotal > self.pluginConfig.maxDumpTimestamp) {
        FPSInfo(@"diff %lf exceed, begin: %lf, end: %lf, scene: %@, you can see more detail in record id: %d",
                m_currRecorder.dumpTimeTotal,
                m_currRecorder.dumpTimeBegin,
                m_currRecorder.dumpTimeBegin + m_currRecorder.dumpTimeTotal / 1000.0,
                m_scene,
                m_currRecorder.recordID);

        m_currRecorder.shouldDump = YES;
        if (self.pluginConfig.trackingType == MMFPSTrackingType_Transition) {
            m_currRecorder.shouldDumpCpuUsage = YES;
        }
    }

    m_currRecorder.shouldExit = YES;

    [m_displayLink invalidate];
    m_displayLink = nil;

    [self.delegate onFPSMonitorPluginReport:self totalTime:(nowTime - m_beginTime) * 1000 dropFrameTime:m_dropTime scene:m_scene];
}

- (void)onFrameCallback:(id)sender {
    double nowTime = CFAbsoluteTimeGetCurrent();
    double diff = (nowTime - m_lastTime) * 1000;

    if (diff > self.pluginConfig.maxFrameInterval) {
        m_currRecorder.dumpTimeTotal += diff;
        m_dropTime += self.pluginConfig.maxFrameInterval * pow(diff / self.pluginConfig.maxFrameInterval, self.pluginConfig.powFactor);

        // 超过了捕获堆栈总个数
        if (m_currRecorder.dumpTimeTotal > self.pluginConfig.dumpInterval * self.pluginConfig.dumpMaxCount) {
            FPSInfo(@"diff %lf exceed, begin: %lf, end: %lf, scene: %@, you can see more detail in record id: %d",
                    m_currRecorder.dumpTimeTotal,
                    m_currRecorder.dumpTimeBegin,
                    m_currRecorder.dumpTimeBegin + m_currRecorder.dumpTimeTotal / 1000.0,
                    m_scene,
                    m_currRecorder.recordID);

            m_currRecorder.shouldDump = YES;
            if (self.pluginConfig.trackingType == MMFPSTrackingType_Transition) {
                m_currRecorder.shouldDumpCpuUsage = YES;
            }

            m_currRecorder.shouldExit = YES;

            [self createFPSThread];
        }
    } else {
        // 连续 N 个掉帧总耗时超过阈值
        if (m_currRecorder.dumpTimeTotal > self.pluginConfig.maxDumpTimestamp) {
            FPSInfo(@"diff %lf exceed, begin: %lf, end: %lf, scene: %@, you can see more detail in record id: %d",
                    m_currRecorder.dumpTimeTotal,
                    m_currRecorder.dumpTimeBegin,
                    m_currRecorder.dumpTimeBegin + m_currRecorder.dumpTimeTotal / 1000.0,
                    m_scene,
                    m_currRecorder.recordID);

            m_currRecorder.shouldDump = YES;
            if (self.pluginConfig.trackingType == MMFPSTrackingType_Transition) {
                m_currRecorder.shouldDumpCpuUsage = YES;
            }

            m_currRecorder.shouldExit = YES;

            [self createFPSThread];
        } else {
            m_currRecorder.dumpTimeTotal = 0;
            m_currRecorder.dumpTimeBegin = nowTime + 0.0001;
        }
    }

    m_lastTime = nowTime;
}

- (void)createFPSThread {
    m_currRecorder = [[WCFPSRecorder alloc] initWithMaxStackTraceCount:self.pluginConfig.dumpMaxCount timeInterval:self.pluginConfig.dumpInterval];
    m_currRecorder.scene = m_scene;
    m_currRecorder.recordID = m_nextRecordID++;
    m_currRecorder.autoUpload = self.pluginConfig.autoUpload;
    m_currRecorder.dumpInterval = self.pluginConfig.dumpInterval;
    m_currRecorder.dumpTimeTotal = 0;
    m_currRecorder.dumpTimeBegin = CFAbsoluteTimeGetCurrent() + 0.0001;

    create_fps_dumping_thread(m_currRecorder);
}

#pragma mark - WCFPSWriterDelegate

- (void)onFPSReportSaved:(WCFPSRecorder *)recorder {
    dispatch_async(m_pluginReportQueue, ^{
        [self uploadReportOnStrategy:recorder];
    });
}

#pragma mark - Upload Logic

- (void)uploadReportOnStrategy:(WCFPSRecorder *)recorder {
    if (recorder.autoUpload == NO) {
        return;
    }

    if (self.reportDelegate) {
        if ([self.reportDelegate isReportLagLimit:self]) {
            FPSInfo(@"report lag limit");
            return;
        }
        if ([self.reportDelegate isCanAutoReportLag:self] == NO) {
            FPSInfo(@"can not auto report lag");
            return;
        }
        if ([self.reportDelegate isNetworkAllowAutoReportLag:self] == NO) {
            FPSInfo(@"report lag, network not allow");
            return;
        }
    }

    double sceneMaxTime = [m_sceneMaxTimeDict[recorder.scene] doubleValue];
    if (recorder.dumpTimeTotal < sceneMaxTime) {
        FPSInfo(@"no upload for dumpTimestamp < sceneMaxTime, record id: %d, scene: %@", recorder.recordID, recorder.scene);
        return;
    }

    m_sceneMaxTimeDict[recorder.scene] = @(recorder.dumpTimeTotal);
    [self uploadReport:recorder];
}

- (void)uploadReport:(WCFPSRecorder *)recorder {
    MatrixIssue *issue = [[MatrixIssue alloc] init];
    issue.issueID = recorder.reportID;
    issue.issueTag = [WCFPSMonitorPlugin getTag];
    issue.filePath = recorder.reportPath;
    issue.dataType = EMatrixIssueDataType_FilePath;
    issue.reportType = EMCrashBlockReportType_Lag;
    issue.customInfo = @{
        @g_crash_block_monitor_custom_file_count : @(1),
        @g_crash_block_monitor_custom_dump_type : @(EDumpType_FPS),
        @g_crash_block_monitor_custom_report_id : @[ issue.issueID ]
    };
    FPSInfo(@"uploadReport: %@, scene: %@", issue, recorder.scene);
    dispatch_async(dispatch_get_main_queue(), ^{
        [self reportIssue:issue];
    });
}

- (void)reportIssueCompleteWithIssue:(MatrixIssue *)issue success:(BOOL)bSuccess {
    if ([issue.issueTag isEqualToString:[WCFPSMonitorPlugin getTag]]) {
        if (bSuccess) {
            FPSInfo(@"report issue success: %@", issue);

            [[NSFileManager defaultManager] removeItemAtPath:issue.filePath error:nil];
        } else {
            FPSInfo(@"report issue failed: %@", issue);
        }
    } else {
        FPSError(@"the issue is not my duty");
    }
}

@end
