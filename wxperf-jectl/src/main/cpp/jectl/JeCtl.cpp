//
// Created by Yves on 2020/7/15.
//

#include <jni.h>
#include <dlfcn.h>
#include "EnhanceDlsym.h"
#include "JeLog.h"

// 必须和 java 保持一致
#define JECTL_OK            0
#define ERR_INIT_FAILED     1
#define ERR_VERSION         2
#define ERR_64_BIT          3

#define TAG "Wxperf.JeCtl"

typedef int (*mallctl_t)(const char *name,
                         void *oldp,
                         size_t *oldlenp,
                         void *newp,
                         size_t newlen);

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_RETRY_TIMES 10

bool initialized = false;
size_t failed_time = 0;

mallctl_t mallctl = nullptr;

static inline bool end_with(std::string const &value, std::string const &ending) {
    if (ending.size() > value.size()) {
        return false;
    }
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

static bool init() {

    void *handle = enhance::dlopen("libc.so", 0);

    if (handle == nullptr) {
        return false;
    }

    mallctl = (mallctl_t) enhance::dlsym(handle, "je_mallctl");

    if (!mallctl) {
        return false;
    }

    Dl_info mallctl_info{};
    if (0 == dladdr((void *)mallctl, &mallctl_info)
        || !end_with(mallctl_info.dli_fname, "/libc.so")) {
        LOGD(TAG, "mallctl = %p, is a fault address, fname = %s, sname = %s", mallctl, mallctl_info.dli_fname, mallctl_info.dli_sname);
        mallctl = nullptr;
        return false;
    }

    enhance::dlclose(handle);

    return true;
}

static void flush_decay_purge() {
    assert(mallctl != nullptr);
    mallctl("thread.tcache.flush", nullptr, nullptr, nullptr, 0);
    mallctl("arena.0.decay", nullptr, nullptr, nullptr, 0);
    mallctl("arena.1.decay", nullptr, nullptr, nullptr, 0);
    mallctl("arena.0.purge", nullptr, nullptr, nullptr, 0);
    mallctl("arena.1.purge", nullptr, nullptr, nullptr, 0);
}

JNIEXPORT jint JNICALL
Java_com_tencent_wxperf_jectl_JeCtl_compactNative(JNIEnv *env, jclass clazz) {

#ifdef __LP64__
    return ERR_64_BIT;
#else

    if (!initialized) {
        if (!(initialized = init() || (failed_time++ > MAX_RETRY_TIMES))) {
            return ERR_INIT_FAILED;
        }
    }
    assert(mallctl != nullptr);

    const char *version;
    size_t     size = sizeof(version);
    mallctl("version", &version, &size, nullptr, 0);
    LOGD(TAG, "jemalloc version: %s", version);

    if (0 != strncmp(version, "5.1.0", 5)) {
        return ERR_VERSION;
    }

    flush_decay_purge();

    return JECTL_OK;
#endif
}

#ifdef __cplusplus
}
#endif