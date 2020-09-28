//
// Created by Yves on 2020/7/9.
//

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include "PthreadExt.h"
#include "Log.h"

#define TAG "PthreadExt"

static pthread_key_t m_attr_key;
static std::mutex    m_init_mutex;

static void attr_destructor(void *__attr) {
    if (__attr) {
        free(__attr);
    }
}

void pthread_ext_init() {
    std::lock_guard<std::mutex> lock(m_init_mutex);
    if (!m_attr_key) {
        pthread_key_create(&m_attr_key, attr_destructor);
    }
}

static int read_thread_name(pthread_t __pthread, char *__buf, size_t __buf_size) {
    if (!__buf || __buf_size < THREAD_NAME_LEN) {
        LOGD(TAG, "read_thread_name: buffer error");
        return ERANGE;
    }

    char  proc_path[64];
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


int pthread_getattr_ext(pthread_t __pthread, pthread_attr_t *__attr) {

    int ret = 0;
    auto attr = (pthread_attr_t *)(pthread_getspecific(m_attr_key));

    if (!attr) {
        attr = (pthread_attr_t *) malloc(sizeof(pthread_attr_t));
        ret = pthread_getattr_np(__pthread, attr);
        pthread_setspecific(m_attr_key, attr);
    }

    if (ret == 0) {
        *__attr = *attr;
    }

    return ret;
}

#undef TAG