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
// Created by Yves on 2019-12-16.
//
// must define ORIGINAL_LIB to use CALL_ORIGIN_FUNC_RET and CALL_ORIGIN_FUNC_VOID
//

#ifndef LIBMATRIX_JNI_HOOKCOMMON_H
#define LIBMATRIX_JNI_HOOKCOMMON_H

#include <dlfcn.h>
#include "JNICommon.h"
#include "Macros.h"

#define GET_CALLER_ADDR(__caller_addr) \
    void * __caller_addr = __builtin_return_address(0)

#define HANDLER_FUNC_NAME(fn_name) h_##fn_name
#define ORIGINAL_FUNC_NAME(fn_name) orig_##fn_name

#define FUNC_TYPE(sym) fn_##sym##_t
#define ORIGINAL_FUNC_PTR(sym) FUNC_TYPE(sym) ORIGINAL_FUNC_NAME(sym)

#define DECLARE_HOOK_ORIG(ret, sym, params...) \
    typedef ret (*FUNC_TYPE(sym))(params); \
    extern ORIGINAL_FUNC_PTR(sym); \
    ret HANDLER_FUNC_NAME(sym)(params);

#define DEFINE_HOOK_FUN(ret, sym, params...) \
    ORIGINAL_FUNC_PTR(sym); \
    ret HANDLER_FUNC_NAME(sym)(params)

#define FETCH_ORIGIN_FUNC(sym) \
    if (!ORIGINAL_FUNC_NAME(sym)) { \
        void *handle = dlopen(ORIGINAL_LIB, RTLD_LAZY); \
        if (handle) { \
            ORIGINAL_FUNC_NAME(sym) = (FUNC_TYPE(sym))dlsym(handle, #sym); \
        } \
    }

#define CALL_ORIGIN_FUNC_RET(retType, ret, sym, params...) \
    if (!ORIGINAL_FUNC_NAME(sym)) { \
        void *handle = dlopen(ORIGINAL_LIB, RTLD_LAZY); \
        if (handle) { \
            ORIGINAL_FUNC_NAME(sym) = (FUNC_TYPE(sym))dlsym(handle, #sym); \
        } \
    } \
    retType ret = ORIGINAL_FUNC_NAME(sym)(params)

#define CALL_ORIGIN_FUNC_VOID(sym, params...) \
    if (!ORIGINAL_FUNC_NAME(sym)) { \
        void *handle = dlopen(ORIGINAL_LIB, RTLD_LAZY); \
        if (handle) { \
            ORIGINAL_FUNC_NAME(sym) = (FUNC_TYPE(sym))dlsym(handle, #sym); \
        } \
    } \
    ORIGINAL_FUNC_NAME(sym)(params)

#define NOTIFY_COMMON_IGNORE_LIBS() \
    do { \
      xhook_ignore(".*libwechatbacktrace\\.so$", NULL); \
      xhook_ignore(".*libtrace-canary\\.so$", NULL); \
      xhook_ignore(".*libwechatcrash\\.so$", NULL); \
      xhook_ignore(".*libmemguard\\.so$", NULL); \
      xhook_ignore(".*libmemmisc\\.so$", NULL); \
      xhook_ignore(".*liblog\\.so$", NULL); \
      xhook_ignore(".*libc\\.so$", NULL); \
      xhook_ignore(".*libm\\.so$", NULL); \
      xhook_ignore(".*libc\\+\\+\\.so$", NULL); \
      xhook_ignore(".*libc\\+\\+_shared\\.so$", NULL); \
      xhook_ignore(".*libstdc\\+\\+.so\\.so$", NULL); \
      xhook_ignore(".*libstlport_shared\\.so$", NULL); \
      xhook_ignore(".*/libwebviewchromium_loader\\.so$", NULL); \
      xhook_ignore(".*/libmatrix-hookcommon\\.so$", nullptr); \
      xhook_ignore(".*/libmatrix-memoryhook\\.so$", nullptr); \
      xhook_ignore(".*/libmatrix-pthreadhook\\.so$", nullptr); \
    } while (0)

#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;
    void       *handler_ptr;
    void       **origin_ptr;
} HookFunction;

typedef void (*hook_init_callback_t)();

EXPORT bool get_java_stacktrace(char *stack_dst, size_t size);

#ifdef __cplusplus
}
#endif

#endif //LIBMATRIX_JNI_HOOKCOMMON_H
