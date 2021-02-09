//
// Created by Yves on 2020/7/9.
//

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <BacktraceDefine.h>
#include "PthreadExt.h"
#include "Log.h"

#define TAG "PthreadExt"

static pthread_key_t m_attr_key;

DEFINE_STATIC_LOCAL(std::mutex, m_init_mutex,);

static void attr_destructor(void *attr) {
    if (attr) {
        free(attr);
    }
}

BACKTRACE_EXPORT
void BACKTRACE_FUNC_WRAPPER(pthread_ext_init)() {
    std::lock_guard<std::mutex> lock(m_init_mutex);
    if (!m_attr_key) {
        pthread_key_create(&m_attr_key, attr_destructor);
    }
}

static int read_thread_name(pthread_t pthread, char *buf, size_t buf_size) {
    if (!buf || buf_size < THREAD_NAME_LEN) {
        LOGD(TAG, "read_thread_name: buffer error");
        return ERANGE;
    }

    char proc_path[64];
    pid_t tid = pthread_gettid_np(pthread);

    snprintf(proc_path, sizeof(proc_path), "/proc/self/task/%d/comm", tid);

    FILE *file = fopen(proc_path, "r");

    if (!file) {
        LOGD(TAG, "read_thread_name: file not found: %s", proc_path);
        return errno;
    }

    size_t n = fread(buf, sizeof(char), buf_size, file);

    fclose(file);

    if (n > THREAD_NAME_LEN) {
        LOGE(TAG, "buf overflowed %zu", n);
        abort();
    }

    if (n > 0 && buf[n - 1] == '\n') {
        LOGD(TAG, "read_thread_name: end with \\0");
        buf[n - 1] = '\0';
    }

    LOGD(TAG, "read_thread_name: %d -> name %s, len %zu, n = %zu", tid, buf, strlen(buf), n);

    return 0;
}

BACKTRACE_EXPORT
int BACKTRACE_FUNC_WRAPPER(pthread_getname_ext)(pthread_t pthread, char *buf, size_t n) {
#if __ANDROID_API__ >= 26
    return pthread_getname_np(pthread, __buf, __n);
#else
    return read_thread_name(pthread, buf, n);
#endif
}


BACKTRACE_EXPORT
int BACKTRACE_FUNC_WRAPPER(pthread_getattr_ext)(pthread_t pthread, pthread_attr_t *attr) {

    int ret = 0;
    auto local_attr = (pthread_attr_t *) (pthread_getspecific(m_attr_key));

    if (!local_attr) {
        local_attr = (pthread_attr_t *) malloc(sizeof(pthread_attr_t));
        ret = pthread_getattr_np(pthread, local_attr);
        pthread_setspecific(m_attr_key, local_attr);
    }

    if (ret == 0) {
        *attr = *local_attr;
    }

    return ret;
}

#undef TAG