//
// Created by Yves on 2019-07-22.
//

#include <jni.h>
#include <unistd.h>
#include <limits>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jstring JNICALL
Java_com_tencent_mm_performance_jni_fd_FDDumpBridge_getFdPathNameNative(JNIEnv
                                                              *env, jclass type,
                                                              jstring path_) {
    const char *path = env->GetStringUTFChars(path_, 0);

    char returnValue[PATH_MAX + 1] = {'\0'};
    readlink(path, returnValue, PATH_MAX + 1);

    env->ReleaseStringUTFChars(path_, path);

    return env->NewStringUTF(returnValue);
}

#ifdef __cplusplus
}
#endif
