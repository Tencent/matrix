/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
// Created by Yves on 2021/5/10.
//
#include <cstdio>
#include "Log.h"

int flogger(FILE *fp, const char *fmt, ...) {
    if (!fp) {
        return 0;
    }
    va_list args;
    va_start(args, fmt);
    int ret = vfprintf(fp, fmt, args);
    va_end(args);
    return ret;
}