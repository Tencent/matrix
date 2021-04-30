//
// Created by tomystang on 2021/2/3.
//

#include <dlfcn.h>
#include <Log.h>
#include <xhook_ext.h>
#include <pthread.h>
#include <cerrno>
#include <cstring>
#include <utility>
#include <cstdlib>
#include <memory>
#include <Conditions.h>
#include "ThreadStackShink.h"

#define LOG_TAG "Matrix.ThreadStackShinker"

static size_t sDefaultStackSize = 1 * 1024 * 1024 + 512 * 1024;

namespace thread_stack_shink {
    static int AdjustStackSize(pthread_attr_t* attr) {
        size_t origStackSize = 0;
        {
            int ret = pthread_attr_getstacksize(attr, &origStackSize);
            if (UNLIKELY(ret != 0)) {
                LOGE(LOG_TAG, "Fail to call pthread_attr_getstacksize, ret: %d", ret);
                return ret;
            }
        }
        if (origStackSize > sDefaultStackSize) {
            LOGW(LOG_TAG, "Stack size is larger than default stack size (%u > %u), give up adjusting.",
                  origStackSize, sDefaultStackSize);
            return ERANGE;
        }
        if (origStackSize < 2 * PTHREAD_STACK_MIN) {
            LOGW(LOG_TAG, "Stack size is too small to reduce (%u / 2 < %u (PTHREAD_STACK_MIN)), give up adjusting.",
                 origStackSize, PTHREAD_STACK_MIN);
            return ERANGE;
        }
        int ret = pthread_attr_setstacksize(attr, origStackSize >> 1U);
        if (LIKELY(ret == 0)) {
            LOGD(LOG_TAG, "Succesfully shrink thread stack size from %u to %u.", origStackSize, origStackSize >> 1U);
        } else {
            LOGE(LOG_TAG, "Fail to call pthread_attr_setstacksize, ret: %d", ret);
        }
        return ret;
    }

    static bool strEndsWith(const char* str, const char* suffix) {
        if (str == nullptr) {
            return false;
        }
        if (suffix == nullptr) {
            return false;
        }
        size_t strLen = ::strlen(str);
        size_t suffixLen = ::strlen(suffix);
        if (strLen < suffixLen) {
            return false;
        }
        return ::strncmp(str + strLen - suffixLen, suffix, suffixLen) == 0;
    }

    void OnPThreadCreate(pthread_t* pthread_ptr, pthread_attr_t const* attr, void* (*start_routine)(void*), void* args) {
        if (attr == nullptr) {
            LOGE(LOG_TAG, "attr is null, skip adjusting.");
            return;
        }
        Dl_info callerDlInfo = {};
        if (dladdr(__builtin_return_address(0), &callerDlInfo) == 0) {
            LOGE(LOG_TAG, "%d >> Fail to get caller info.", ::getpid());
            return;
        }
        if (strstr(callerDlInfo.dli_fname, "/app_tbs/") != nullptr) {
            LOGW(LOG_TAG, "Inside %s, skip adjusting.", callerDlInfo.dli_fname);
            return;
        }
        if (strEndsWith(callerDlInfo.dli_fname, "/libmttwebview.so")) {
            LOGW(LOG_TAG, "Inside libmttwebview.so, skip adjusting.");
            return;
        }
        if (strEndsWith(callerDlInfo.dli_fname, "/libmtticu.so")) {
            LOGW(LOG_TAG, "Inside libmtticu.so, skip adjusting.");
            return;
        }
        if (strEndsWith(callerDlInfo.dli_fname, "/libtbs_v8.so")) {
            LOGW(LOG_TAG, "Inside libtbs_v8.so, skip adjusting.");
            return;
        }
        if (strEndsWith(callerDlInfo.dli_fname, "/libhwui.so")) {
            LOGW(LOG_TAG, "Inside libhwui.so, skip adjusting.");
            return;
        }
        if (strEndsWith(callerDlInfo.dli_fname, "/libmagicbrush.so")) {
            LOGW(LOG_TAG, "Inside libmagicbrush.so, skip adjusting.");
            return;
        }
        int ret = AdjustStackSize(const_cast<pthread_attr_t*>(attr));
        if (UNLIKELY(ret != 0)) {
            LOGE(LOG_TAG, "Fail to adjust stack size, ret: %d", ret);
        }
    }
}