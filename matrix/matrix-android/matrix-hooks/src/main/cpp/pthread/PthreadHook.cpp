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
// Created by YinSheng Tang on 2021/4/29.
//

#include <sys/types.h>
#include <unistd.h>
#include <common/Log.h>
#include <xhook.h>
#include <xhook_ext.h>
#include <common/HookCommon.h>
#include <common/SoLoadMonitor.h>
#include <common/Macros.h>
#include "PthreadHook.h"
#include "ThreadTrace.h"
#include "ThreadStackShink.h"

#define LOG_TAG "Matrix.PthreadHook"
#define ORIGINAL_LIB "libc.so"
static void *pthread_handle = nullptr;

static volatile bool sThreadTraceEnabled = false;
static volatile bool sThreadStackShrinkEnabled = false;

DECLARE_HOOK_ORIG(int, pthread_create, pthread_t*, pthread_attr_t const*, pthread_hook::pthread_routine_t, void*);
DECLARE_HOOK_ORIG(int, pthread_setname_np, pthread_t, const char*);
DECLARE_HOOK_ORIG(int, pthread_detach, pthread_t);
DECLARE_HOOK_ORIG(int, pthread_join, pthread_t, void**);

DEFINE_HOOK_FUN(int, pthread_create,
        pthread_t* pthread, pthread_attr_t const* attr, pthread_hook::pthread_routine_t start_routine, void* args) {
    Dl_info callerInfo = {};
    bool callerInfoOk = true;
    if (dladdr(__builtin_return_address(0), &callerInfo) == 0) {
        LOGE(LOG_TAG, "%d >> Fail to get caller info.", ::getpid());
        callerInfoOk = false;
    }

    pthread_attr_t tmpAttr;
    if (LIKELY(attr == nullptr)) {
        int ret = pthread_attr_init(&tmpAttr);
        if (UNLIKELY(ret != 0)) {
            LOGE(LOG_TAG, "Fail to init new attr, ret: %d", ret);
        }
    } else {
        tmpAttr = *attr;
    }

    if (callerInfoOk && sThreadStackShrinkEnabled) {
        thread_stack_shink::OnPThreadCreate(&callerInfo, pthread, &tmpAttr, start_routine, args);
    }

    int ret = 0;
    if (sThreadTraceEnabled) {
        auto *routine_wrapper = thread_trace::wrap_pthread_routine(start_routine, args);
        CALL_ORIGIN_FUNC_RET(pthread_handle, int, tmpRet, pthread_create, pthread, &tmpAttr, routine_wrapper->wrapped_func,
                             routine_wrapper);
        ret = tmpRet;
    } else {
        CALL_ORIGIN_FUNC_RET(pthread_handle, int, tmpRet, pthread_create, pthread, &tmpAttr, start_routine, args);
        ret = tmpRet;
    }

    if (LIKELY(ret == 0) && sThreadTraceEnabled) {
        thread_trace::handle_pthread_create(*pthread);
    }

    if (LIKELY(attr == nullptr)) {
        pthread_attr_destroy(&tmpAttr);
    }

    return ret;
}

DEFINE_HOOK_FUN(int, pthread_setname_np, pthread_t pthread, const char* name) {
    CALL_ORIGIN_FUNC_RET(pthread_handle, int, ret, pthread_setname_np, pthread, name);
    if (LIKELY(ret == 0) && sThreadTraceEnabled) {
        thread_trace::handle_pthread_setname_np(pthread, name);
    }
    return ret;
}

DEFINE_HOOK_FUN(int, pthread_detach, pthread_t pthread) {
    CALL_ORIGIN_FUNC_RET(pthread_handle, int, ret, pthread_detach, pthread);
    LOGD(LOG_TAG, "pthread_detach : %d", ret);
    if (LIKELY(ret == 0) && sThreadTraceEnabled) {
        thread_trace::handle_pthread_release(pthread);
    }
    return ret;
}

DEFINE_HOOK_FUN(int, pthread_join, pthread_t pthread, void** return_value_ptr) {
    CALL_ORIGIN_FUNC_RET(pthread_handle, int, ret, pthread_join, pthread, return_value_ptr);
    LOGD(LOG_TAG, "pthread_join : %d", ret);
    if (LIKELY(ret == 0) && sThreadTraceEnabled) {
        thread_trace::handle_pthread_release(pthread);
    }
    return ret;
}

namespace pthread_hook {
    void SetThreadTraceEnabled(bool enabled) {
        LOGD(LOG_TAG, "[*] Calling SetThreadTraceEnabled, enabled: %d", enabled);
        sThreadTraceEnabled = enabled;
    }

    void SetThreadStackShrinkEnabled(bool enabled) {
        LOGD(LOG_TAG, "[*] Calling SetThreadStackShrinkEnabled, enabled: %d", enabled);
        sThreadStackShrinkEnabled = enabled;
    }

    void SetThreadStackShrinkIgnoredCreatorSoPatterns(const char** patterns, size_t pattern_count) {
        LOGD(LOG_TAG, "[*] Calling SetThreadStackShrinkIgnoredCreatorSoPatterns, patterns: %p, count: %d",
                patterns, pattern_count);
        thread_stack_shink::SetIgnoredCreatorSoPatterns(patterns, pattern_count);
    }

    void InstallHooks(bool enable_debug) {
        LOGI(LOG_TAG, "[+] Calling InstallHooks, sThreadTraceEnabled: %d, sThreadStackShinkEnabled: %d",
             sThreadTraceEnabled, sThreadStackShrinkEnabled);
        if (!sThreadTraceEnabled && !sThreadStackShrinkEnabled) {
            LOGD(LOG_TAG, "[*] InstallHooks was ignored.");
            return;
        }

        FETCH_ORIGIN_FUNC(pthread_create)
        FETCH_ORIGIN_FUNC(pthread_setname_np)
        FETCH_ORIGIN_FUNC(pthread_detach)
        FETCH_ORIGIN_FUNC(pthread_join)

        if (sThreadTraceEnabled) {
            thread_trace::thread_trace_init();
        }

        matrix::PauseLoadSo();
        xhook_block_refresh();
        {
            int ret = xhook_export_symtable_hook("libc.so", "pthread_create",
                                                 (void *) HANDLER_FUNC_NAME(pthread_create), nullptr);
            LOGD(LOG_TAG, "export table hook sym: pthread_create, ret: %d", ret);

            ret = xhook_export_symtable_hook("libc.so", "pthread_setname_np",
                                             (void *) HANDLER_FUNC_NAME(pthread_setname_np), nullptr);
            LOGD(LOG_TAG, "export table hook sym: pthread_setname_np, ret: %d", ret);
            xhook_grouped_register(HOOK_REQUEST_GROUPID_PTHREAD, ".*/.*\\.so$", "pthread_create",
                                   (void *) HANDLER_FUNC_NAME(pthread_create), nullptr);
            xhook_grouped_register(HOOK_REQUEST_GROUPID_PTHREAD, ".*/.*\\.so$", "pthread_setname_np",
                                   (void *) HANDLER_FUNC_NAME(pthread_setname_np), nullptr);

            xhook_grouped_register(HOOK_REQUEST_GROUPID_PTHREAD, ".*/.*\\.so$", "pthread_detach",
                           (void *) HANDLER_FUNC_NAME(pthread_detach),nullptr);
            xhook_grouped_register(HOOK_REQUEST_GROUPID_PTHREAD, ".*/.*\\.so$", "pthread_join",
                           (void *) HANDLER_FUNC_NAME(pthread_join), nullptr);

            ret = xhook_export_symtable_hook("libc.so", "pthread_detach",
                                             (void *) HANDLER_FUNC_NAME(pthread_detach), nullptr);
            LOGD(LOG_TAG, "export table hook sym: pthread_detach, ret: %d", ret);

            ret = xhook_export_symtable_hook("libc.so", "pthread_join",
                                             (void *) HANDLER_FUNC_NAME(pthread_join), nullptr);
            LOGD(LOG_TAG, "export table hook sym: pthread_join, ret: %d", ret);

            xhook_enable_debug(enable_debug ? 1 : 0);
            xhook_enable_sigsegv_protection(0);
            xhook_refresh(0);
        }
        xhook_unblock_refresh();
        matrix::ResumeLoadSo();
    }
}
