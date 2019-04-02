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

#import <Foundation/Foundation.h>

typedef enum {
    MXLogLevelAll = 0,
    MXLogLevelVerbose = 0,
    MXLogLevelDebug,
    MXLogLevelInfo,
    MXLogLevelWarn,
    MXLogLevelError,
    MXLogLevelFatal,
    MXLogLevelNone,
} MXLogLevel;

#ifdef __cplusplus
extern "C" {
#endif

BOOL matrix_shouldlog(MXLogLevel level);
void matrix_log(MXLogLevel log_level,
                const char *module,
                const char *file,
                int line,
                const char *funcname,
                NSString *message);

#ifdef __cplusplus
}
#endif

@protocol MatrixAdapterDelegate <NSObject>

- (BOOL)matrixShouldLog:(MXLogLevel)level;

- (void)matrixLog:(MXLogLevel)logLevel
           module:(const char *)module
             file:(const char *)file
             line:(int)line
         funcName:(const char *)funcName
          message:(NSString *)message;

@end

@interface MatrixAdapter : NSObject

+ (MatrixAdapter *)sharedInstance;

@property (nonatomic, weak) id<MatrixAdapterDelegate> delegate;

@end

