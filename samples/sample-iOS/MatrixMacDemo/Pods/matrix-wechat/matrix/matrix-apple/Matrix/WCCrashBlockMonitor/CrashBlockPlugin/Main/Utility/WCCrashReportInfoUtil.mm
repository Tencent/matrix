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

#import "WCCrashReportInfoUtil.h"
#import <mach-o/dyld.h>
#import <map>
#import "KSCrash.h"

#ifdef __LP64__
typedef mach_header_64 wxg_mach_header;
typedef segment_command_64 wxg_mach_segment_command;

#define LC_SEGMENT_ARCH LC_SEGMENT_64
#else
typedef mach_header wxg_mach_header;
typedef segment_command wxg_mach_segment_command;
#define LC_SEGMENT_ARCH LC_SEGMENT
#endif

/** Used for writing hex string values. */
static const char g_hexNybbles[] =
    {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

NSString *wxg_uuidToString(const unsigned char *const value)
{
    if (value == NULL) {
        return @"";
    } else {
        char uuidBuffer[37] = {0};
        const unsigned char *src = value;
        char *dst = uuidBuffer;
        for (int i = 0; i < 4; i++) {
            *dst++ = g_hexNybbles[(*src >> 4) & 15];
            *dst++ = g_hexNybbles[(*src++) & 15];
        }
        *dst++ = '-';
        for (int i = 0; i < 2; i++) {
            *dst++ = g_hexNybbles[(*src >> 4) & 15];
            *dst++ = g_hexNybbles[(*src++) & 15];
        }
        *dst++ = '-';
        for (int i = 0; i < 2; i++) {
            *dst++ = g_hexNybbles[(*src >> 4) & 15];
            *dst++ = g_hexNybbles[(*src++) & 15];
        }
        *dst++ = '-';
        for (int i = 0; i < 2; i++) {
            *dst++ = g_hexNybbles[(*src >> 4) & 15];
            *dst++ = g_hexNybbles[(*src++) & 15];
        }
        *dst++ = '-';
        for (int i = 0; i < 6; i++) {
            *dst++ = g_hexNybbles[(*src >> 4) & 15];
            *dst++ = g_hexNybbles[(*src++) & 15];
        }
        return [NSString stringWithUTF8String:uuidBuffer];
    }
}

const char *wxg_lastPathEntry(const char *const path)
{
    if (path == NULL) {
        return NULL;
    }

    const char *lastFile = (const char *) strrchr(path, '/');
    return lastFile == NULL ? path : lastFile + 1;
}

@interface WCCrashReportInfoUtil ()

@property (nonatomic, strong) NSMutableArray *binaryImages;
@property (nonatomic, strong) NSDictionary *systemInfo;

@end

@implementation WCCrashReportInfoUtil

+ (id)sharedInstance
{
    static WCCrashReportInfoUtil *sharedInstance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedInstance = [[WCCrashReportInfoUtil alloc] init];
    });
    return sharedInstance;
}

- (id)init
{
    self = [super init];
    if (self) {
        [self setupSystemInfo];
        [self setupBinaryImages];
    }
    return self;
}

- (void)setupSystemInfo
{
    _systemInfo = [[KSCrash sharedInstance] systemInfo];
}

- (void)setupBinaryImages
{
    _binaryImages = [[NSMutableArray alloc] init];

    // This extract the starting slide of every module in the app
    // This is used to know which module an instruction pointer belongs to.

    // These operations is NOT thread-safe according to Apple docs
    // Do not call this multiple times
    int images = _dyld_image_count();

    for (int i = 0; i < images; i++) {
        // Here we extract the module name from the full path
        // Typically it looks something like: /path/to/lib/UIKit
        // And I just extract UIKit

        NSString *fullName = [NSString stringWithUTF8String:_dyld_get_image_name(i)];
        NSRange range = [fullName rangeOfString:@"/" options:NSBackwardsSearch];

        NSUInteger startP = (range.location != NSNotFound) ? range.location + 1 : 0;
        NSString *imageName = [fullName substringFromIndex:startP];

        wxg_mach_header *header = (wxg_mach_header *) _dyld_get_image_header(i);
        if (!header) {
            continue;
        }

        NSInteger cpuType = header->cputype;
        NSInteger cpuSubType = header->cpusubtype;
        uint8_t *uuid = NULL;
        uint64_t imageSize = 0;
        uint64_t imageVmAddr = 0;
        uint64_t imageAddr = (uintptr_t) header;

        // This is parsing the mach header in order to extract the slide.
        // See https://developer.apple.com/library/mac/documentation/DeveloperTools/Conceptual/MachORuntime/index.html
        // For the structure of mach headers
        const struct load_command *cmd = reinterpret_cast<const struct load_command *>((header + 1));

        for (unsigned int c = 0; cmd && (c < header->ncmds); c++) {
            if (cmd->cmd == LC_SEGMENT_ARCH) {
                const wxg_mach_segment_command *seg = reinterpret_cast<const wxg_mach_segment_command *>(cmd);

                if (!strcmp(seg->segname, "__TEXT")) {
                    imageSize = seg->vmsize;
                    imageVmAddr = seg->vmaddr;
                }
            }
            if (cmd->cmd == LC_UUID) {
                struct uuid_command *uuidCmd = (struct uuid_command *) cmd;
                uuid = uuidCmd->uuid;
            }

            cmd = reinterpret_cast<struct load_command *>((char *) cmd + cmd->cmdsize);
        }

        NSString *uuidString = wxg_uuidToString(uuid);
        NSNumber *iAddrNum = [NSNumber numberWithUnsignedLongLong:imageAddr];
        NSNumber *iSizeNum = [NSNumber numberWithUnsignedLongLong:imageSize];
        NSNumber *iVMAddrNum = [NSNumber numberWithUnsignedLongLong:imageVmAddr];
        NSNumber *cpuTypeNum = [NSNumber numberWithInteger:cpuType];
        NSNumber *cpuSubtypeNum = [NSNumber numberWithInteger:cpuSubType];

        if (imageName != nil && uuidString != nil && iAddrNum != nil &&
            iSizeNum != nil && iVMAddrNum != nil && cpuTypeNum != nil &&
            cpuSubtypeNum != nil) {
            NSDictionary *imageInfoDict = @{ @"image_addr" : iAddrNum,
                                             @"image_size" : iSizeNum,
                                             @"image_vmaddr" : iVMAddrNum,
                                             @"name" : imageName,
                                             @"uuid" : uuidString,
                                             @"cpu_type" : cpuTypeNum,
                                             @"cpu_subtype" : cpuSubtypeNum };
            [_binaryImages addObject:imageInfoDict];
        }
    }
}

- (NSArray *)getBinaryImages
{
    return [_binaryImages copy];
}

- (NSDictionary *)getSystemInfo
{
    return _systemInfo;
}

@end
