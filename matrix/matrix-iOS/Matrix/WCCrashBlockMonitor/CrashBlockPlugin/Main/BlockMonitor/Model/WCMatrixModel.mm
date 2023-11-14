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

#import "WCMatrixModel.h"
#import "KSSymbolicator.h"

#include <dlfcn.h>

#define SYMBOL_ADDRESS "object_address"
#define SYMBOL_NAME "object_name"
#define IMAGE_ADDRESS "image_address"
#define IMAGE_NAME "image_name"
#define INSTRUCTION_ADDRESS "instruction_address"
#define SAMPLE_COUNT "sample"
#define SAMPLE_COUNT_BACKGROUND "sample_background"

@interface WCAddressFrame () {
    const char *m_symbolName;
}

@end

@implementation WCAddressFrame

- (id)initWithAddress:(uintptr_t)address withRepeatCount:(uint32_t)count isInBackground:(BOOL)isInBackground {
    self = [super init];
    if (self) {
        _address = address;
        _repeatCount = count;
        if (isInBackground) {
            _repeatCountInBackground = count;
        }
        _childAddressFrame = [[NSMutableArray alloc] init];

        m_symbolName = NULL;
    }
    return self;
}

- (void)addChildFrame:(WCAddressFrame *)addressFrame {
    [_childAddressFrame addObject:addressFrame];
}

- (NSDictionary *)getInfoDict {
    if (m_symbolName == NULL) {
        return @{
            @INSTRUCTION_ADDRESS : [NSNumber numberWithUnsignedInteger:self.address],
            @SAMPLE_COUNT : [NSNumber numberWithInt:self.repeatCount],
            @SAMPLE_COUNT_BACKGROUND : [NSNumber numberWithInt:self.repeatCountInBackground]
        };
    } else {
        return @{
            @INSTRUCTION_ADDRESS : [NSNumber numberWithUnsignedInteger:self.address],
            @SYMBOL_NAME : [NSString stringWithUTF8String:m_symbolName],
            @SAMPLE_COUNT : [NSNumber numberWithInt:self.repeatCount],
            @SAMPLE_COUNT_BACKGROUND : [NSNumber numberWithInt:self.repeatCountInBackground]
        };
    }
}

- (void)symbolicate {
    Dl_info symbolsBuffer;

    if (ksdl_dladdr_use_cache(CALL_INSTRUCTION_FROM_RETURN_ADDRESS(self.address), &symbolsBuffer)) {
        m_symbolName = symbolsBuffer.dli_sname;
    }

    if (_childAddressFrame == nil || [_childAddressFrame count] == 0) {
        return;
    }
    for (WCAddressFrame *addressFrame in _childAddressFrame) {
        [addressFrame symbolicate];
    }
}

- (WCAddressFrame *)tryFoundAddressFrameWithAddress:(uintptr_t)toFindAddress {
    if (self.address == toFindAddress) {
        return self;
    } else {
        if (self.childAddressFrame == nil || [self.childAddressFrame count] == 0) {
            return nil;
        } else {
            for (WCAddressFrame *frame in self.childAddressFrame) {
                WCAddressFrame *foundFrame = [frame tryFoundAddressFrameWithAddress:toFindAddress];
                if (foundFrame != nil) {
                    return foundFrame;
                }
            }
            return nil;
        }
    }
}

- (NSString *)description {
    return [self descriptionWithDeep:0];
}

- (NSString *)descriptionWithDeep:(int)deep {
    NSMutableString *retStr = [NSMutableString new];

    for (int i = 0; i < deep; ++i) {
        [retStr appendString:@"\t"];
    }

    if (m_symbolName) {
        [retStr appendFormat:@"%d(BG:%d) %s\n", self.repeatCount, self.repeatCountInBackground, m_symbolName];
    } else {
        [retStr appendFormat:@"%d(BG:%d) 0x%p\n", self.repeatCount, self.repeatCountInBackground, (void *)self.address];
    }

    for (int i = 0; i < self.childAddressFrame.count; ++i) {
        WCAddressFrame *childFrame = self.childAddressFrame[i];
        [retStr appendString:[childFrame descriptionWithDeep:deep + 1]];
    }

    return retStr;
}

@end

@implementation WCMatrixModel

@end
