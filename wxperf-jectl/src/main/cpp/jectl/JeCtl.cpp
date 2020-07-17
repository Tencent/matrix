//
// Created by Yves on 2020/7/15.
//

#include <jni.h>
#include "EnhanceDlsym.h"
#include "JeLog.h"

// 必须和 java 保持一致
#define JECTL_OK            0
#define ERR_SO_NOT_FOUND    1
#define ERR_SYM_MALLCTL     2
#define ERR_SYM_OPT_RETAIN  3
#define ERR_VERSION         4
#define ERR_64_BIT          5

#define TAG "Wxperf.JeCtl"

typedef int (*mallctl_t)(const char *name,
                         void *oldp,
                         size_t *oldlenp,
                         void *newp,
                         size_t newlen);

extern "C"
JNIEXPORT jint JNICALL
Java_com_tencent_wxperf_jectl_JeCtl_tryDisableRetainNative(JNIEnv *env, jclass clazz) {

#ifdef __LP64__
    return ERR_64_BIT;
#else

    void *handle = Enhance::dlopen("libc.so", 0);

    if (!handle) {
        return ERR_SO_NOT_FOUND;
    }

    auto mallctl    = (mallctl_t) Enhance::dlsym(handle, "je_mallctl");

    if (!mallctl) {
        return ERR_SYM_MALLCTL;
    }

    const char *version;
    size_t     size = 0;
    mallctl("version", &version, &size, nullptr, 0);

    LOGD(TAG, "jemalloc version: %s", version);

    if (0 != strncmp(version, "5.", 2)) {
        return ERR_VERSION;
    }

    bool *opt_retain = (bool *) Enhance::dlsym(handle, "je_opt_retain");

    if (!opt_retain) {
        return ERR_SYM_OPT_RETAIN;
    }

    *opt_retain = false;

    Enhance::dlclose(handle);

    return JECTL_OK;
#endif
}