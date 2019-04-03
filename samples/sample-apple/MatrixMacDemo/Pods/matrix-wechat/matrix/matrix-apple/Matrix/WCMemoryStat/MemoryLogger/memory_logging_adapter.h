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

#ifndef memory_logging_adapter_hpp
#define memory_logging_adapter_hpp

#include <stdio.h>
#include "logger_internal.h"

void set_memory_logging_invalid(void);
void log_internal(const char *file, int line, const char *funcname, char *msg);
void log_internal_without_this_thread(thread_id tid);
void report_error(int error);

#endif /* memory_logging_adapter_hpp */
