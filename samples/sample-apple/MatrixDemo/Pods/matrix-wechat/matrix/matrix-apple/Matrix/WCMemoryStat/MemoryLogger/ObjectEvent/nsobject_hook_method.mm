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

#import "nsobject_hook_method.h"
#import <Foundation/Foundation.h>
#import <objc/runtime.h>

void nsobject_hook_alloc_method()
{
    Class cls = object_getClass([NSObject class]);
    SEL origSel = @selector(alloc);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundeclared-selector"
    SEL altSel = @selector(event_logging_alloc);
#pragma clang diagnostic pop

    Method originMethod = class_getInstanceMethod(cls, origSel);
    Method newMethod = class_getInstanceMethod(cls, altSel);
    
    if (originMethod && newMethod) {
        if (class_addMethod(cls, origSel, method_getImplementation(newMethod), method_getTypeEncoding(newMethod))) {
            class_replaceMethod(cls, altSel, method_getImplementation(originMethod), method_getTypeEncoding(originMethod));
        } else {
            method_exchangeImplementations(originMethod, newMethod);
        }
    }
}
