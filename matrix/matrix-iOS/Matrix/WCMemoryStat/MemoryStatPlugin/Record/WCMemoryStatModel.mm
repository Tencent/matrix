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

#import "WCMemoryStatModel.h"
#import "MatrixPathUtil.h"
#import "MatrixDeviceInfo.h"

#import "memory_logging.h"
#import "memory_report_generator.h"

@implementation MSThreadInfo

@end

@implementation MemoryRecordInfo

- (id)init
{
    self = [super init];
    if (self) {
        self.threadInfos = [[NSMutableDictionary alloc] init];
		self.userScene = @"";
		self.systemVersion = @"";
		self.appUUID = @"";
    }
    return self;
}

- (NSString *)recordID
{
    return [NSString stringWithFormat:@"%lld", self.launchTime];
}

- (NSString *)recordDataPath
{
    NSString *pathComponent = [NSString stringWithFormat:@"Data/%lld", self.launchTime];
    return [[MatrixPathUtil memoryStatPluginCachePath] stringByAppendingPathComponent:pathComponent];
}

- (NSData *)generateReportDataWithCustomInfo:(NSDictionary *)customInfo
{
    NSString *dataPath = [self recordDataPath];

	summary_report_param param;
	param.phone = [MatrixDeviceInfo platform].UTF8String;
	param.os_ver = [self systemVersion].UTF8String;
	param.launch_time = self.launchTime * 1000;
	param.report_time = [[NSDate date] timeIntervalSince1970] * 1000;
	param.app_uuid = [self appUUID].UTF8String;
	param.foom_scene = [self userScene].UTF8String;
	
    for (id key in customInfo) {
        std::string stdKey = [key UTF8String];
        std::string stdVal = [[customInfo[key] description] UTF8String];
		param.customInfo.insert(std::make_pair(stdKey, stdVal));
    }

    auto content = generate_summary_report(dataPath.UTF8String, param);
    return [NSData dataWithBytes:content->c_str() length:content->size()];
}

- (void)calculateAllThreadsMemoryUsage
{
    std::unordered_map<uint64_t, uint64_t> threadAllocSizes = thread_alloc_size([self recordDataPath].UTF8String);

    for (NSString *threadID in self.threadInfos.allKeys) {
        auto iter = threadAllocSizes.find([threadID intValue]);
        if (iter != threadAllocSizes.end()) {
            self.threadInfos[threadID].allocSize = iter->second;
        }
    }
}

@end
