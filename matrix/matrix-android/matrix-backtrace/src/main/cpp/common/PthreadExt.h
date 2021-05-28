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

#ifndef LIBMATRIX_JNI_PTHREADEXT_H
#define LIBMATRIX_JNI_PTHREADEXT_H

#include <pthread.h>
#include "Predefined.h"

#define THREAD_NAME_LEN 16

int BACKTRACE_FUNC_WRAPPER(pthread_getname_ext)(pthread_t __pthread, char *__buf, size_t __n);

void BACKTRACE_FUNC_WRAPPER(pthread_ext_init)();

int BACKTRACE_FUNC_WRAPPER(pthread_getattr_ext)(pthread_t pthread, pthread_attr_t* attr);

#endif //LIBMATRIX_JNI_PTHREADEXT_H
