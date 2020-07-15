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
Java_com_tencent_wxperf_fd_FDDumpBridge_getFdPathNameNative(JNIEnv *__env, jclass __clazz,
                                                            jstring __path) {
    const char *path = __env->GetStringUTFChars(__path, 0);

    char returnValue[PATH_MAX + 1] = {'\0'};
    readlink(path, returnValue, PATH_MAX + 1);

    __env->ReleaseStringUTFChars(__path, path);

    return __env->NewStringUTF(returnValue);
}

#ifdef __cplusplus
}
#endif
