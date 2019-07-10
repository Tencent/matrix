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

#import "MatrixPathUtil.h"
#import "MatrixLogDef.h"
#import <sys/stat.h>
#import <sys/time.h>

@implementation MatrixPathUtil

+ (NSString *)matrixCacheRootPath
{
    static NSString *s_rootPath;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
        s_rootPath = [paths[0] stringByAppendingString:@"/Matrix"];
        if ([[NSFileManager defaultManager] fileExistsAtPath:s_rootPath] == NO) {
            [[NSFileManager defaultManager] createDirectoryAtPath:s_rootPath withIntermediateDirectories:YES attributes:nil error:nil];
        }
    });
    return s_rootPath;
}

+ (NSString *)crashBlockPluginCachePath
{
    static NSString *s_rootPath;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        s_rootPath = [[self matrixCacheRootPath] stringByAppendingPathComponent:@"CrashBlock"];
        if ([[NSFileManager defaultManager] fileExistsAtPath:s_rootPath] == NO) {
            [[NSFileManager defaultManager] createDirectoryAtPath:s_rootPath withIntermediateDirectories:YES attributes:nil error:nil];
        }
    });
    return s_rootPath;
}

+ (NSString *)appRebootAnalyzerCachePath
{
    static NSString *s_rootPath;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        s_rootPath = [[self matrixCacheRootPath] stringByAppendingPathComponent:@"AppReboot"];
        if ([[NSFileManager defaultManager] fileExistsAtPath:s_rootPath] == NO) {
            [[NSFileManager defaultManager] createDirectoryAtPath:s_rootPath withIntermediateDirectories:YES attributes:nil error:nil];
        }
    });
    return s_rootPath;
}

+ (NSString *)memoryStatPluginCachePath
{
    static NSString *s_rootPath;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        s_rootPath = [[self matrixCacheRootPath] stringByAppendingPathComponent:@"MemoryStat"];
        if ([[NSFileManager defaultManager] fileExistsAtPath:s_rootPath] == NO) {
            [[NSFileManager defaultManager] createDirectoryAtPath:s_rootPath withIntermediateDirectories:YES attributes:nil error:nil];
        }
    });
    return s_rootPath;
}

// ============================================================================
#pragma mark - Auto Clean
// ============================================================================

+ (void)autoCleanDiretory:(NSString *)folderPath withTimeout:(NSTimeInterval)timeoutSecond
{
    NSArray *allFiles = [[NSFileManager defaultManager] subpathsAtPath:folderPath];
    
    time_t latestTime = 0U;
    time_t curTime = 0U;
    struct timeval curTimeVal;
    if (0 == gettimeofday(&curTimeVal, NULL)) {
        curTime = curTimeVal.tv_sec;
    }

    for (NSString *file in allFiles) {
        NSString *filePath = [folderPath stringByAppendingPathComponent:file];
        struct stat st;
        if (0 == lstat([filePath UTF8String], &st)) {
            if (S_ISREG(st.st_mode)) {
                time_t atime = st.st_atime;
                time_t mtime = st.st_mtime;
                time_t ctime = st.st_ctime;
                time_t btime = st.st_birthtime;
                latestTime = [MatrixPathUtil latestTimeWithCurTime:curTime accessTime:atime modifyTime:mtime changeTime:ctime birthTime:btime];
                
                if ((curTime - latestTime) >= timeoutSecond) {
                    NSError *removeError = nil;
                    [[NSFileManager defaultManager] removeItemAtPath:filePath error:&removeError];
                    if (removeError != nil) {
                        MatrixError(@"remove file: %@ error: %@", filePath, removeError);
                    } else {
                        MatrixInfo(@"remove file: %@", filePath);
                    }
                }
            }
        }
    }
}

+ (time_t)latestTimeWithCurTime:(time_t)curTime
                     accessTime:(time_t)atime
                     modifyTime:(time_t)mtime
                     changeTime:(time_t)ctime
                      birthTime:(time_t)btime
{
    time_t latestTime = 0;
    time_t times[] = {atime, mtime, ctime, btime};
    int len = sizeof(times) / sizeof(times[0]);
    for (int i = 0; i < len; i++) {
        time_t t = times[i];
        if (t <= curTime && t > latestTime) {
            latestTime = t;
        }
    }
    return latestTime;
}

@end
