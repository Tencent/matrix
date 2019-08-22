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

#import "WCCrashBlockMonitor.h"
#import "KSCrash.h"
#import "MatrixLogDef.h"
#import "WCCrashBlockFileHandler.h"
#import "WCDumpInterface.h"
#import "WCBlockMonitorMgr.h"
#import "MatrixAppRebootAnalyzer.h"

void kscrash_innerHandleSignalCallback(siginfo_t *info)
{
    [MatrixAppRebootAnalyzer notifyAppCrashed];
}

@interface WCCrashBlockMonitor () <WCBlockMonitorDelegate>

@property (nonatomic, assign) BOOL bInstallSuccess;
@property (nonatomic, strong) WCBlockMonitorMgr *blockMonitor;

@end

@implementation WCCrashBlockMonitor

- (id)init
{
    self = [super init];
    if (self) {
        _appVersion = @"";
        _appShortVersion = @"";
        _bmConfiguration = [WCBlockMonitorConfiguration defaultConfig];
        _onHandleSignalCallBack = nil;
        _onAppendAdditionalInfoCallBack = nil;

        _bInstallSuccess = NO;
    }
    return self;
}

- (BOOL)installKSCrash:(BOOL)shouldCrash;
{
    KSCrash *handler = [KSCrash sharedInstance];
    if (shouldCrash) {
        handler.monitoring = KSCrashMonitorType(KSCrashMonitorTypeProductionSafeMinimal);
    } else {
        handler.monitoring = KSCrashMonitorType(KSCrashMonitorTypeManual);
    }
    
    if (_appVersion != nil && _appShortVersion != nil &&
        [_appVersion length] > 0 && [_appShortVersion length] > 0) {
        [KSCrash setCustomFullVersion:_appVersion shortVersion:_appShortVersion];
    }
    
    MatrixInfo(@"install kscrash, version : %@ , %@", _appVersion, _appShortVersion);
    
    if (_onHandleSignalCallBack != nil) {
        handler.onHandleSignalCallBack = _onHandleSignalCallBack;
    }

    if (_onAppendAdditionalInfoCallBack != nil) {
        handler.onCrash = _onAppendAdditionalInfoCallBack;
    }

    handler.onInnerHandleSignalCallBack = kscrash_innerHandleSignalCallback;
    handler.onWritePointThread = kscrash_pointThreadCallback;
    handler.onWritePointThreadRepeatNumber = kscrash_pointThreadRepeatNumberCallback;
    handler.onWritePointCpuHighThread = kscrash_pointCPUHighThreadCallback;
    handler.onWritePointCpuHighThreadCount = kscrash_pointCpuHighThreadCountCallback;
    handler.onWritePointCpuHighThreadValue = kscrash_pointCpuHighThreadArrayCallBack;

    BOOL ret = [handler install];
    if (!ret) {
        MatrixError(@"KSCrash Object install failed");
    } else {
        MatrixInfo(@"KSCrash install success");
        _bInstallSuccess = ret;
    }
    
    NSString *lagFilePath = [MatrixAppRebootAnalyzer lastDumpFileName];
    if (lagFilePath && lagFilePath.length > 0) {
        MatrixInfo(@"should handle oom dump file, %@", lagFilePath);
        [WCCrashBlockFileHandler handleOOMDumpFile:lagFilePath];
    }
    return ret;
}

- (void)enableBlockMonitor
{
    if (_bInstallSuccess == NO) {
        MatrixError(@"KSCrash not install success, cannot use block monitor");
    }
    MatrixInfo(@"install block monitor");
    
    _blockMonitor = [WCBlockMonitorMgr shareInstance];
    _blockMonitor.delegate = self;
    [_blockMonitor resetConfiguration:_bmConfiguration];
    [_blockMonitor start];
}

- (void)startBlockMonitor
{
    assert([NSThread mainThread]);
    if (_blockMonitor) {
        [_blockMonitor start];
    }
}

- (void)stopBlockMonitor
{
    assert([NSThread mainThread]);
    if (_blockMonitor) {
        [_blockMonitor stop];
    }
}

- (void)resetAppFullVersion:(NSString *)fullVersion shortVersion:(NSString *)shortVersion
{
    _appVersion = fullVersion;
    _appShortVersion = shortVersion;
    MatrixInfo(@"reset version : %@ , %@", fullVersion, shortVersion);
    
    if (_appVersion != nil && _appShortVersion != nil &&
        [_appVersion length] > 0 && [_appShortVersion length] > 0) {
        [KSCrash setCustomFullVersion:fullVersion shortVersion:shortVersion];
    }
}

#if !TARGET_OS_OSX

- (void)handleBackgroundLaunch
{
    MatrixDebug(@"handle background launch");
    [_blockMonitor handleBackgroundLaunch];
}

- (void)handleSuspend
{
    MatrixDebug(@"handle suspend");
    [_blockMonitor handleSuspend];
}

#endif

- (void)startTrackCPU
{
    MatrixDebug(@"start track CPU");
    [_blockMonitor startTrackCPU];
}

- (void)stopTrackCPU
{
    MatrixDebug(@"stop track CPU");
    [_blockMonitor stopTrackCPU];
}

- (BOOL)isBackgroundCPUTooSmall
{
    return [_blockMonitor isBackgroundCPUTooSmall];
}

- (void)generateLiveReportWithDumpType:(EDumpType)dumpType
                            withReason:(NSString *)reason
                       selfDefinedPath:(BOOL)bSelfDefined
{
    [WCDumpInterface dumpReportWithReportType:dumpType
                                withBlockTime:0
                          withExceptionReason:reason
                              selfDefinedPath:bSelfDefined];
}

// ============================================================================
#pragma mark - WCBlockMonitorDelegate
// ============================================================================

+ (BOOL)p_isDumpTypeRelatedToFOOM:(EDumpType)dumType
{
    return dumType == EDumpType_MainThreadBlock || dumType == EDumpType_CPUBlock;
}

- (void)onBlockMonitor:(WCBlockMonitorMgr *)bmMgr beginDump:(EDumpType)dumpType blockTime:(uint64_t)blockTime
{
    if (_delegate != nil && [_delegate respondsToSelector:@selector(onCrashBlockMonitorBeginDump:blockTime:)]) {
        [_delegate onCrashBlockMonitorBeginDump:dumpType blockTime:blockTime];
    }
}

- (void)onBlockMonitor:(WCBlockMonitorMgr *)bmMgr enterNextCheckWithDumpType:(EDumpType)dumpType
{
    [MatrixAppRebootAnalyzer isForegroundMainThreadBlock:[WCCrashBlockMonitor p_isDumpTypeRelatedToFOOM:dumpType]];

    if (_delegate != nil && [_delegate respondsToSelector:@selector(onCrashBlockMonitorEnterNextCheckWithDumpType:)]) {
        [_delegate onCrashBlockMonitorEnterNextCheckWithDumpType:dumpType];
    }
}

- (void)onBlockMonitor:(WCBlockMonitorMgr *)bmMgr getDumpFile:(NSString *)dumpFile withDumpType:(EDumpType)dumpType
{
    if ([WCCrashBlockMonitor p_isDumpTypeRelatedToFOOM:dumpType]) {
        [MatrixAppRebootAnalyzer setDumpFileName:dumpFile];
    }

    if (_delegate != nil && [_delegate respondsToSelector:@selector(onCrashBlockMonitorGetDumpFile:withDumpType:)]) {
        [_delegate onCrashBlockMonitorGetDumpFile:dumpFile withDumpType:dumpType];
    }
}

- (void)onBlockMonitor:(WCBlockMonitorMgr *)bmMgr dumpType:(EDumpType)dumpType filter:(EFilterType)filterType
{
    if (_delegate != nil && [_delegate respondsToSelector:@selector(onCrashBlockMonitorDumpType:filter:)]) {
        [_delegate onCrashBlockMonitorDumpType:dumpType filter:filterType];
    }
}

- (NSDictionary *)onBlockMonitor:(WCBlockMonitorMgr *)bmMgr getCustomUserInfoForDumpType:(EDumpType)dumpType
{
    if (_delegate != nil && [_delegate respondsToSelector:@selector(onCrashBlockMonitorGetCustomUserInfoForDumpType:)]) {
        return [_delegate onCrashBlockMonitorGetCustomUserInfoForDumpType:dumpType];
    }
    return nil;
}

- (void)onBlockMonitorCurrentCPUTooHigh:(WCBlockMonitorMgr *)bmMgr
{
    if (_delegate != nil && [_delegate respondsToSelector:@selector(onCrashBlockMonitorCurrentCPUTooHigh)]) {
        [_delegate onCrashBlockMonitorCurrentCPUTooHigh];
    }
}

- (void)onBlockMonitorIntervalCPUTooHigh:(WCBlockMonitorMgr *)bmMgr
{
    if (_delegate != nil && [_delegate respondsToSelector:@selector(onCrashBlockMonitorIntervalCPUTooHigh)]) {
        [_delegate onCrashBlockMonitorIntervalCPUTooHigh];
    }
}

@end
