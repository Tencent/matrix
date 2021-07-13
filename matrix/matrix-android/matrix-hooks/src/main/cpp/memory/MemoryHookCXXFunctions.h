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
// Created by Yves on 2019-09-18.
//

#ifndef LIBMATRIX_HOOK_MEMORYHOOKCXXFUNCTIONS_H
#define LIBMATRIX_HOOK_MEMORYHOOKCXXFUNCTIONS_H

#include <new>
#include <cstddef>
#include <common/HookCommon.h>

#ifndef __LP64__

DECLARE_HOOK_ORIG(void*, _Znwj, size_t size)

DECLARE_HOOK_ORIG(void*, _ZnwjSt11align_val_t, size_t size, std::align_val_t align_val)

DECLARE_HOOK_ORIG(void*, _ZnwjSt11align_val_tRKSt9nothrow_t, size_t size,
                    std::align_val_t align_val, std::nothrow_t const& nothrow)

DECLARE_HOOK_ORIG(void*, _ZnwjRKSt9nothrow_t, size_t size, std::nothrow_t const& nothrow)

DECLARE_HOOK_ORIG(void*, _Znaj, size_t size)

DECLARE_HOOK_ORIG(void*, _ZnajSt11align_val_t, size_t size, std::align_val_t align_val)

DECLARE_HOOK_ORIG(void*, _ZnajSt11align_val_tRKSt9nothrow_t, size_t size,
                    std::align_val_t align_val, std::nothrow_t const& nothrow)

DECLARE_HOOK_ORIG(void*, _ZnajRKSt9nothrow_t, size_t size, std::nothrow_t const& nothrow)

DECLARE_HOOK_ORIG(void, _ZdlPvj, void* ptr, size_t size)

DECLARE_HOOK_ORIG(void, _ZdlPvjSt11align_val_t, void* ptr, size_t size,
                    std::align_val_t align_val)

DECLARE_HOOK_ORIG(void, _ZdaPvj, void* ptr, size_t size)

DECLARE_HOOK_ORIG(void, _ZdaPvjSt11align_val_t, void* ptr, size_t size,
                    std::align_val_t align_val)

#else

DECLARE_HOOK_ORIG(void*, _Znwm, size_t size)

DECLARE_HOOK_ORIG(void*, _ZnwmSt11align_val_t, size_t size, std::align_val_t align_val)

DECLARE_HOOK_ORIG(void*, _ZnwmSt11align_val_tRKSt9nothrow_t, size_t size,
                 std::align_val_t align_val, std::nothrow_t const& nothrow)

DECLARE_HOOK_ORIG(void*, _ZnwmRKSt9nothrow_t, size_t size, std::nothrow_t const& nothrow)

DECLARE_HOOK_ORIG(void*, _Znam, size_t size)

DECLARE_HOOK_ORIG(void*, _ZnamSt11align_val_t, size_t size, std::align_val_t align_val)

DECLARE_HOOK_ORIG(void*, _ZnamSt11align_val_tRKSt9nothrow_t, size_t size,
                 std::align_val_t align_val, std::nothrow_t const& nothrow)

DECLARE_HOOK_ORIG(void*, _ZnamRKSt9nothrow_t, size_t size, std::nothrow_t const& nothrow)



DECLARE_HOOK_ORIG(void, _ZdlPvm, void* ptr, size_t size)

DECLARE_HOOK_ORIG(void, _ZdlPvmSt11align_val_t, void* ptr, size_t size,
                 std::align_val_t align_val)

DECLARE_HOOK_ORIG(void, _ZdaPvm, void* ptr, size_t size)

DECLARE_HOOK_ORIG(void, _ZdaPvmSt11align_val_t, void* ptr, size_t size,
                 std::align_val_t align_val)

#endif

DECLARE_HOOK_ORIG(char*, strdup, const char* str)

DECLARE_HOOK_ORIG(char*, strndup, const char* str, size_t n)

DECLARE_HOOK_ORIG(void, _ZdlPv, void* ptr)

DECLARE_HOOK_ORIG(void, _ZdlPvSt11align_val_tRKSt9nothrow_t, void* ptr,
                  std::align_val_t align_val, std::nothrow_t const& nothrow)

DECLARE_HOOK_ORIG(void, _ZdlPvSt11align_val_t, void* ptr, std::align_val_t align_val)

DECLARE_HOOK_ORIG(void, _ZdlPvRKSt9nothrow_t, void* ptr, std::nothrow_t const& nothrow)

DECLARE_HOOK_ORIG(void, _ZdaPv, void* ptr)

DECLARE_HOOK_ORIG(void, _ZdaPvSt11align_val_t, void* ptr, std::align_val_t align_val)

DECLARE_HOOK_ORIG(void, _ZdaPvSt11align_val_tRKSt9nothrow_t, void* ptr,
                  std::align_val_t align_val, std::nothrow_t const& nothrow)

DECLARE_HOOK_ORIG(void, _ZdaPvRKSt9nothrow_t, void* ptr, std::nothrow_t const& nothrow)

#endif //LIBMATRIX_HOOK_MEMORYHOOKCXXFUNCTIONS_H
