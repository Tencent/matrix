//
// Created by Yves on 2020/7/9.
//

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "PthreadExt.h"
#include "Log.h"

#define TAG "PthreadExt"

static int read_thread_name(pthread_t __pthread, char *__buf, size_t __buf_size) {
    if (!__buf || __buf_size < THREAD_NAME_LEN) {
        LOGD(TAG, "read_thread_name: buffer error");
        return ERANGE;
    }

    char proc_path[64];
    pid_t tid = pthread_gettid_np(__pthread);

    snprintf(proc_path, sizeof(proc_path), "/proc/self/task/%d/comm", tid);

    FILE *file = fopen(proc_path, "r");

    if (!file) {
        LOGD(TAG, "read_thread_name: file not found: %s", proc_path);
        return errno;
    }

    size_t n = fread(__buf, sizeof(char), __buf_size, file);

    fclose(file);

    if (n > THREAD_NAME_LEN) {
        LOGE(TAG, "buf overflowed %zu", n);
        abort();
    }

    if (n > 0 && __buf[n - 1] == '\n') {
        LOGD(TAG, "read_thread_name: end with \\0");
        __buf[n - 1] = '\0';
    }

    LOGD(TAG, "read_thread_name: %d -> name %s, len %zu, n = %zu", tid, __buf, strlen(__buf), n);

    return 0;
}

int pthread_getname_ext(pthread_t __pthread, char *__buf, size_t __n) {
#if __ANDROID_API__ >= 26
    return pthread_getname_np(__pthread, __buf, __n);
#else
    return read_thread_name(__pthread, __buf, __n);
#endif
}

#undef TAG