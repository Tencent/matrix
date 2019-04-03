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

#import "MatrixPlugin.h"
#import "MatrixLogDef.h"

@interface MatrixPlugin ()

@property (nonatomic, weak) id<MatrixPluginListenerDelegate> pluginListener;

@end

@implementation MatrixPlugin

@synthesize pluginConfig;

- (void)setupPluginListener:(id<MatrixPluginListenerDelegate>)pluginListener
{
    _pluginListener = pluginListener;
}

- (void)start
{
    if ([_pluginListener respondsToSelector:@selector(onStart:)]) {
        [_pluginListener onStart:self];
    }
}

- (void)stop
{
    if ([_pluginListener respondsToSelector:@selector(onStop:)]) {
        [_pluginListener onStop:self];
    }
}

- (void)destroy
{
    if ([_pluginListener respondsToSelector:@selector(onDestroy:)]) {
        [_pluginListener onDestroy:self];
    }
}

- (void)reportIssue:(MatrixIssue *)issue
{
    if (_pluginListener != nil) {
        [_pluginListener onReportIssue:issue];
    }
}

- (void)reportIssueCompleteWithIssue:(MatrixIssue *)issue success:(BOOL)bSuccess
{
    MatrixError(@"base class not deal with this");
}

+ (NSString *)getTag
{
    MatrixError(@"base class has no tag");
    return nil;
}

@end
