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

#import "Matrix.h"
#import "MatrixPlugin.h"
#import "MatrixLogDef.h"
#import "MatrixAppRebootAnalyzer.h"
#import "WCCrashBlockMonitorPlugin+Private.h"

// ============================================================================
#pragma mark - MatrixBuilder
// ============================================================================

@interface MatrixBuilder ()

@property (nonatomic, strong) NSMutableSet *plugins;

@end

@implementation MatrixBuilder

- (id)init
{
    self = [super init];
    if (self) {
        self.pluginListener = nil;
        self.plugins = [[NSMutableSet alloc] init];
    }
    return self;
}

- (void)addPlugin:(MatrixPlugin *)plugin
{
    if (nil == plugin) {
        MatrixError(@"plugin is nil");
        return;
    }

    [_plugins addObject:plugin];
}

- (NSMutableSet *)getPlugins
{
    return _plugins;
}

@end

// ============================================================================
#pragma mark - Matrix
// ============================================================================

@interface Matrix ()

@property (nonatomic, strong) MatrixBuilder *builder;

@end

@implementation Matrix

+ (id)sharedInstance
{
    static Matrix *sg_sharedInstance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sg_sharedInstance = [[Matrix alloc] init];
    });
    return sg_sharedInstance;
}

- (id)init
{
    self = [super init];
    if (self) {
        [MatrixAppRebootAnalyzer installAppRebootAnalyzer];
        [MatrixAppRebootAnalyzer checkRebootType];
    }
    return self;
}

- (void)addMatrixBuilder:(MatrixBuilder *)builder
{
    self.builder = builder;
    NSMutableSet *buildPlugins = [builder getPlugins];
    for (MatrixPlugin *plugin in buildPlugins) {
        [plugin setupPluginListener:[builder pluginListener]];
    }
}

- (void)startPlugins
{
    NSMutableSet *plugins = [self.builder getPlugins];
    for (MatrixPlugin *plugin in plugins) {
        [plugin start];
    }
}

- (void)stopPlugins
{
    NSMutableSet *plugins = [self.builder getPlugins];
    for (MatrixPlugin *plugin in plugins) {
        [plugin stop];
    }
}

- (NSMutableSet *)getPlugins
{
    return [self.builder getPlugins];
}

- (MatrixPlugin *)getPluginWithTag:(NSString *)tag
{
    NSMutableSet *plugins = [self.builder getPlugins];
    MatrixPlugin *retPlugin = nil;
    for (MatrixPlugin *plugin in plugins) {
        if ([[[plugin class] getTag] isEqualToString:tag]) {
            retPlugin = plugin;
            break;
        }
    }
    if (retPlugin == nil) {
        MatrixError(@"Plugin[%@] does not be inited.", tag);
    }
    return retPlugin;
}

- (void)reportIssueComplete:(MatrixIssue *)matrixIssue success:(BOOL)bSuccess
{
    MatrixInfo(@"report issue complete: %@, success: %d", matrixIssue, bSuccess);
    MatrixPlugin *plugin = [self getPluginWithTag:matrixIssue.issueTag];
    [plugin reportIssueCompleteWithIssue:matrixIssue success:bSuccess];
}

// ============================================================================
#pragma mark - AppReboot
// ============================================================================

- (MatrixAppRebootType)lastRebootType
{
    return [MatrixAppRebootAnalyzer lastRebootType];
}

- (uint64_t)appLaunchTime
{
    return [MatrixAppRebootAnalyzer appLaunchTime];
}

- (uint64_t)lastAppLaunchTime
{
    return [MatrixAppRebootAnalyzer lastAppLaunchTime];
}

- (void)setUserScene:(NSString *)scene
{
    [MatrixAppRebootAnalyzer setUserScene:scene];
}

- (NSString *)userSceneOfLastRun
{
    return [MatrixAppRebootAnalyzer userSceneOfLastRun];
}

#if !TARGET_OS_OSX
- (void)notifyAppBackgroundFetch
{
    [MatrixAppRebootAnalyzer notifyAppBackgroundFetch];

    WCCrashBlockMonitorPlugin *cbPlugin = (WCCrashBlockMonitorPlugin *) [self getPluginWithTag:[WCCrashBlockMonitorPlugin getTag]];
    if (cbPlugin) {
        [cbPlugin handleBackgroundLaunch];
    }
}

- (void)notifyAppWillSuspend
{
    [MatrixAppRebootAnalyzer notifyAppWillSuspend];
    WCCrashBlockMonitorPlugin *cbPlugin = (WCCrashBlockMonitorPlugin *) [self getPluginWithTag:[WCCrashBlockMonitorPlugin getTag]];
    if (cbPlugin) {
        [cbPlugin handleSuspend];
    }
}
#endif

@end
