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
// Created by YinSheng Tang on 2021/5/11.
//

#ifndef LIBMATRIX_JNI_MACROS_H
#define LIBMATRIX_JNI_MACROS_H


#define LIKELY(cond) (__builtin_expect(!!(cond), 1))

#define UNLIKELY(cond) (__builtin_expect(!!(cond), 0))

#define EXPORT extern __attribute__ ((visibility ("default")))

#define EXPORT_C extern "C" __attribute__ ((visibility ("default")))

#define NELEM(...) ((sizeof(__VA_ARGS__)) / (sizeof(__VA_ARGS__[0])))

#define HOOK_CHECK(assertion)                              \
  if (__builtin_expect(!(assertion), false)) {             \
    abort();                                               \
  }


#endif //LIBMATRIX_JNI_MACROS_H
