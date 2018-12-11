/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
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

//
// Created by wuzhiwen on 2017/2/16.
//

#ifndef SQLITELINT_ANDROID_SLOG_H
#define SQLITELINT_ANDROID_SLOG_H
#include "comm/log/logger.h"
#include <stdio.h>
#include <android/log.h>
#include "loader.h"
#include <assert.h>
#include <stdlib.h>

namespace sqlitelint {
    static const char *const SqliteLintTAG = "SqliteLint.Native";

    #define LOGV(fmt, args...)     SLog(ANDROID_LOG_VERBOSE, (fmt), ##args)
    #define LOGD(fmt, args...)     SLog(ANDROID_LOG_DEBUG, (fmt), ##args)
    #define LOGI(fmt, args...)     SLog(ANDROID_LOG_INFO, (fmt), ##args)
    #define LOGW(fmt, args...)     SLog(ANDROID_LOG_WARN, (fmt), ##args)
    #define LOGE(fmt, args...)     SLog(ANDROID_LOG_ERROR, (fmt), ##args)
}

#endif //SQLITELINT_SQLITELINT_H
