#include <jni.h>
#include <malloc.h>

#include <android/log.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <vector>
#include <sys/mman.h>
#include <pthread.h>
#include <iostream>
#include <sstream>
#include <map>
#include <unordered_map>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include "MallocTest.h"
#include <android/log.h>

#define LOGD(TAG, FMT, args...) __android_log_print(ANDROID_LOG_DEBUG, TAG, FMT, ##args)
#define TAG "Matrix.Hooks.sample"

#ifdef __cplusplus
extern "C" {
#endif

using namespace std;

int count = 0;

JNIEXPORT void JNICALL
Java_sample_tencent_matrix_hooks_JNIObj_doMmap(JNIEnv *env, jobject instance) {

    void *p_mmap = mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    LOGD(TAG, "map ptr = %p", p_mmap);
    munmap(p_mmap, 1024);

    p_mmap = mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    LOGD(TAG, "map ptr = %p", p_mmap);

    munmap(p_mmap, 1024);

    p_mmap = mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    LOGD(TAG, "map ptr = %p", p_mmap);

    p_mmap = mremap(p_mmap, 1024, 2048, MREMAP_MAYMOVE);
    LOGD(TAG, "remap ptr = %p", p_mmap);

    p_mmap = mremap(p_mmap, 1024, 20480, MREMAP_MAYMOVE);
    LOGD(TAG, "remap ptr = %p", p_mmap);
}

JNIEXPORT void JNICALL
Java_sample_tencent_matrix_hooks_JNIObj_reallocTest(JNIEnv *env, jobject instance) {
    void *p = malloc(1024);
    p = realloc(p, 2048);
    // leak p

    p = malloc(1024);
    p = realloc(p, 512);
    // leak p

    p = malloc(4096);
    p = realloc(p, 0);
    // free p

}

JNIEXPORT void JNICALL
Java_sample_tencent_matrix_hooks_JNIObj_mallocTest(JNIEnv *env, jclass clazz) {
//    malloc_test();
#define LEN 20
    void *p = malloc(300 * 1024 * 1024);
    LOGD(TAG, "p = %p", p);
    free(p);

    p = new int[1024]; // leak

#undef LEN
}

pthread_key_t exit_key;
void test_destructor(void *arg) {
    LOGD(TAG, "exit");
}

#define THREAD_NAME_LEN 16
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
        LOGD(TAG, "buf overflowed %zu", n);
        abort();
    }

    if (n > 0 && buf[n - 1] == '\n') {
        LOGD(TAG, "read_thread_name: end with \\0");
        buf[n - 1] = '\0';
    }

    LOGD(TAG, "read_thread_name: %d -> name %s, len %zu, n = %zu", tid, buf, strlen(buf), n);

    return 0;
}

void *thread_test(void *) {
    pthread_attr_t attr{};
    pthread_getattr_np(pthread_self(), &attr);
    char thread_name[THREAD_NAME_LEN];
    read_thread_name(pthread_self(), thread_name, THREAD_NAME_LEN);

    LOGD(TAG, "thread_run: name %s stack %p-%p", thread_name, attr.stack_base, (void *) ((uint64_t)attr.stack_base - attr.stack_size));
    return nullptr;
}

#define PTHREAD_COUNT 10 //3000

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hooks_sample_JNIObj_threadTest(JNIEnv *env, jclass clazz) {
//    pthread_key_create(&exit_key, test_destructor);

    pthread_t pthreads[PTHREAD_COUNT];
    for (int i = 0; i < PTHREAD_COUNT; ++i) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
//        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        int ret = pthread_create(&pthreads[i], nullptr, thread_test, nullptr);
//        pthread_detach(pthreads[i]);

//        pthread_getattr_np(pthreads[i], &attr);
//        int state = PTHREAD_CREATE_JOINABLE;
//        pthread_attr_getdetachstate(&attr, &state);
//        LOGD(TAG, "detach state = %d", state);
        if (i % 500 == 0) {
            LOGD(TAG, "sleep");
            sleep(1);
        }
        if (ret != 0) {
            LOGD(TAG, "pthread_create error[%d]: %d", i, ret);
            break;
        }
    }
}

#ifdef __cplusplus
}
#endif