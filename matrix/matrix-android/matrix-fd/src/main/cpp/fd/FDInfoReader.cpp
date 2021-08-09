//
// Created by Yves on 2019-07-22.
//

#include <jni.h>
#include <unistd.h>
#include <limits>
#include <sys/time.h>
#include <sys/resource.h>
#include <android/log.h>
#include <cstdio>
#include <sstream>
#include <sys/mman.h>


#define LOGD(TAG, FMT, args...) //__android_log_print(ANDROID_LOG_DEBUG, TAG, FMT, ##args)

#define TAG "Matrix.FD"

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jstring JNICALL
Java_com_tencent_matrix_fd_FDDumpBridge_getFdPathNameNative(JNIEnv *__env, jclass __clazz,
                                                            jstring __path) {
    const char *path = __env->GetStringUTFChars(__path, 0);

    char returnValue[PATH_MAX + 1] = {'\0'};
    readlink(path, returnValue, PATH_MAX + 1);

    __env->ReleaseStringUTFChars(__path, path);

    return __env->NewStringUTF(returnValue);
}

#define DEFAULT_FD_LIMIT 1024

JNIEXPORT jint JNICALL
Java_com_tencent_matrix_fd_FDDumpBridge_getFDLimit(JNIEnv *env, jclass clazz) {

    struct rlimit limit{};

    int ret = getrlimit(RLIMIT_NOFILE, &limit);

    if (ret == 0) {
        LOGD(TAG, "RLIMIT_NOFILE = cur = %ld, max = %ld", limit.rlim_cur, limit.rlim_max);
        return limit.rlim_cur;
    }

    return DEFAULT_FD_LIMIT;
}

#ifdef __cplusplus
}
#endif
