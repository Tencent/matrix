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

#import "WCCrashBlockMonitorConfig.h"

@implementation WCCrashBlockMonitorConfig

+ (WCCrashBlockMonitorConfig *)defaultConfiguration;
{
    WCCrashBlockMonitorConfig *config = [[WCCrashBlockMonitorConfig alloc] init];
    return config;
}

- (id)init
{
    self = [super init];
    if (self) {
        self.appShortVersion = self.appVersion = @"";
        self.enableCrash = YES;
        self.enableBlockMonitor = YES;
        self.blockMonitorDelegate = nil;
        self.onAppendAdditionalInfoCallBack = NULL;
        self.onHandleSignalCallBack = NULL;
        self.blockMonitorConfiguration= [WCBlockMonitorConfiguration defaultConfig];
        self.reportStrategy = EWCCrashBlockReportStrategy_All;
    }
    return self;
}

@end
