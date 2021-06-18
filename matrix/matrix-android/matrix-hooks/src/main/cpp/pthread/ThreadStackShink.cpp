//
// Created by tomystang on 2021/2/3.
//

#include <atomic>
#include <cstdio>
#include <unistd.h>
#include <dlfcn.h>
#include <common/Log.h>
#include <xhook_ext.h>
#include <pthread.h>
#include <cerrno>
#include <cstring>
#include <utility>
#include <cstdlib>
#include <memory>
#include <common/Macros.h>
#include <vector>
#include <mutex>
#include <regex.h>
#include "ThreadStackShink.h"

#define LOG_TAG "Matrix.ThreadStackShinker"

static std::atomic_size_t sDefaultNativeStackSize(0);
static std::atomic_bool sShrinkJavaThread(false);
static std::vector<regex_t> sIgnoredCreatorSoPatterns;
static std::mutex sConfigLock;

namespace thread_stack_shink {
    static size_t GetDefaultNativeStackSize() {
        size_t stackSize = sDefaultNativeStackSize.load();
        if (UNLIKELY(stackSize == 0)) {
            pthread_attr_t defAttr = {};
            pthread_attr_init(&defAttr);
            pthread_attr_getstacksize(&defAttr, &stackSize);
            if (stackSize == 0) {
                constexpr size_t SIGNAL_STACK_SIZE_WITHOUT_GUARD = 16 * 1024;
                stackSize = 1 * 1024 * 1024 - SIGNAL_STACK_SIZE_WITHOUT_GUARD;
            }
            sDefaultNativeStackSize.store(stackSize);
        }
        return stackSize;
    }

    static int AdjustStackSize(pthread_attr_t* attr) {
        size_t origStackSize = 0;
        {
            int ret = pthread_attr_getstacksize(attr, &origStackSize);
            if (UNLIKELY(ret != 0)) {
                LOGE(LOG_TAG, "Fail to call pthread_attr_getstacksize, ret: %d", ret);
                return ret;
            }
        }
        const size_t defaultNativeStackSize = GetDefaultNativeStackSize();
        if (origStackSize != defaultNativeStackSize) {
            LOGE(LOG_TAG, "Stack size is not equal to default native stack size (%u != %u), give up adjusting.",
                 origStackSize, defaultNativeStackSize);
            return ERANGE;
        }
        if (origStackSize < 2 * PTHREAD_STACK_MIN) {
            LOGE(LOG_TAG, "Stack size is too small to reduce (%u / 2 < %u (PTHREAD_STACK_MIN)), give up adjusting.",
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

    void SetShrinkJavaThread(bool value) {
        sShrinkJavaThread.store(value);
    }

    void SetIgnoredCreatorSoPatterns(const char** patterns, size_t pattern_count) {
        std::lock_guard configLockGuard(sConfigLock);
        for (auto & pattern : sIgnoredCreatorSoPatterns) {
            auto pRegEx = const_cast<regex_t*>(&pattern);
            ::regfree(pRegEx);
        }
        sIgnoredCreatorSoPatterns.clear();
        if (patterns != nullptr) {
            for (int i = 0; i < pattern_count; ++i) {
                regex_t regex = {};
                int ret = regcomp(&regex, patterns[i], 0);
                if (ret == 0) {
                    sIgnoredCreatorSoPatterns.push_back(regex);
                } else {
                    LOGE(LOG_TAG, "Bad pattern: %s, compile error code: %d", patterns[i], ret);
                }
            }
        }
    }

    void OnPThreadCreate(const Dl_info* caller_info, pthread_t* pthread_ptr, pthread_attr_t const* attr, void* (*start_routine)(void*), void* args) {
        if (attr == nullptr) {
            LOGE(LOG_TAG, "attr is null, skip adjusting.");
            return;
        }

        if (strEndsWith(caller_info->dli_fname, "/libhwui.so")) {
            LOGE(LOG_TAG, "Inside libhwui.so, skip adjusting.");
            return;
        }
        {
            std::lock_guard configLockGuard(sConfigLock);
            for (const auto & pattern : sIgnoredCreatorSoPatterns) {
                if (::regexec(&pattern, caller_info->dli_fname, 0, nullptr, 0) == 0) {
                    LOGE(LOG_TAG, "Thread in %s was ignored according to ignored creator so patterns.", caller_info->dli_fname);
                    return;
                }
            }
        }

        int ret = AdjustStackSize(const_cast<pthread_attr_t*>(attr));
        if (UNLIKELY(ret != 0)) {
            LOGE(LOG_TAG, "Fail to adjust stack size, ret: %d", ret);
        }
    }
}