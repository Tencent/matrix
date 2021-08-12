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

#ifdef __cplusplus
}
#endif