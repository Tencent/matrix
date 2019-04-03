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

#ifndef MatrixLogDef_h
#define MatrixLogDef_h

#import "MatrixAdapter.h"

#define MatrixError(format, ...) MatrixErrorWithModule("Matrix", format, ##__VA_ARGS__)
#define MatrixWarning(format, ...) MatrixWarningWithModule("Matrix", format, ##__VA_ARGS__)
#define MatrixInfo(format, ...) MatrixInfoWithModule("Matrix", format, ##__VA_ARGS__)
#define MatrixDebug(format, ...) MatrixDebugWithModule("Matrix", format, ##__VA_ARGS__)

#define __FILENAME__ (strrchr(__FILE__, '/') + 1)

#define MatrixLogInternal(LOGLEVEL, MODULE, FILE, LINE, FUNC, PREFIX, FORMAT, ...)\
    if (matrix_shouldlog(LOGLEVEL)) {\
        @autoreleasepool {\
            NSString *__log_message = [[NSString alloc] initWithFormat:@"%@%@", PREFIX, [NSString stringWithFormat:FORMAT, ##__VA_ARGS__, nil]]; \
            matrix_log(LOGLEVEL, MODULE, FILE, LINE, FUNC, __log_message);\
        }\
    }

#define MatrixErrorWithModule(MODULE, FORMAT, ...) \
    MatrixLogInternal(MXLogLevelError, MODULE, __FILENAME__, __LINE__, __FUNCTION__, @"ERROR: ", FORMAT, ##__VA_ARGS__)

#define MatrixWarningWithModule(MODULE, FORMAT, ...) \
    MatrixLogInternal(MXLogLevelWarn, MODULE, __FILENAME__, __LINE__, __FUNCTION__, @"WARNING: ", FORMAT, ##__VA_ARGS__)

#define MatrixInfoWithModule(MODULE, FORMAT, ...) \
    MatrixLogInternal(MXLogLevelInfo, MODULE, __FILENAME__, __LINE__, __FUNCTION__, @"INFO: ", FORMAT, ##__VA_ARGS__)

#define MatrixDebugWithModule(MODULE, FORMAT, ...) \
    MatrixLogInternal(MXLogLevelDebug, MODULE, __FILENAME__, __LINE__, __FUNCTION__, @"DEBUG: ", FORMAT, ##__VA_ARGS__)

#endif /* MatrixLogDef_h */
