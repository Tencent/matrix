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

#import "MatrixAdapter.h"

@implementation MatrixAdapter

+ (MatrixAdapter *)sharedInstance
{
    static MatrixAdapter *s_instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        s_instance = [[MatrixAdapter alloc] init];
    });
    return s_instance;
}

@end

BOOL matrix_shouldlog(MXLogLevel level)
{
    id<MatrixAdapterDelegate> delegate = [MatrixAdapter sharedInstance].delegate;
    if (delegate) {
        return [delegate matrixShouldLog:level];
    }
    return NO;
}

void matrix_log(MXLogLevel log_level,
                const char *module,
                const char *file,
                int line,
                const char *funcname,
                NSString *message)
{
    id<MatrixAdapterDelegate> delegate = [MatrixAdapter sharedInstance].delegate;
    if (delegate) {
        [delegate matrixLog:log_level
                     module:module
                       file:file
                       line:line
                   funcName:funcname
                    message:message];
    }
}

