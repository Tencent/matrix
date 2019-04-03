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

#import "NSApplicationEvent.h"

#if TARGET_OS_OSX

#import "WCBlockMonitorMgr.h"
#import <objc/runtime.h>

@implementation NSApplicationEvent

- (void)wc_sendEvent:(NSEvent *)event
{    
    [WCBlockMonitorMgr signalEventStart];
    [self wc_sendEvent:event];
    [WCBlockMonitorMgr signalEventEnd];
}

#pragma mark - Swizzle
- (void)tryswizzle
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        Class originalClass = NSClassFromString(@"NSApplication");
        Class swizzledClass = [self class];
        SEL originalSelector = NSSelectorFromString(@"sendEvent:");
        SEL swizzledSelector = @selector(wc_sendEvent:);
        
        Method originalMethod = class_getInstanceMethod(originalClass, originalSelector);
        Method swizzledMethod = class_getInstanceMethod(swizzledClass, swizzledSelector);
        
        BOOL registerMethod = class_addMethod(originalClass,
                                              swizzledSelector,
                                              method_getImplementation(swizzledMethod),
                                              method_getTypeEncoding(swizzledMethod));
        if (!registerMethod) {
            return;
        }
        
        swizzledMethod = class_getInstanceMethod(originalClass, swizzledSelector);
        if (!swizzledMethod) {
            return;
        }
        
        BOOL didAddMethod = class_addMethod(originalClass,
                                            originalSelector,
                                            method_getImplementation(swizzledMethod),
                                            method_getTypeEncoding(swizzledMethod));
        if (didAddMethod) {
            class_replaceMethod(originalClass,
                                swizzledSelector,
                                method_getImplementation(originalMethod),
                                method_getTypeEncoding(originalMethod));
        } else {
            method_exchangeImplementations(originalMethod, swizzledMethod);
        }
    });
}

@end

#endif
