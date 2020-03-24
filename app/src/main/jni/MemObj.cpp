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

#define LOGD(TAG, FMT, args...) __android_log_print(ANDROID_LOG_DEBUG, TAG, FMT, ##args)

#ifdef __cplusplus
extern "C" {
#endif

int count = 0;

JNIEXPORT void JNICALL
Java_com_tencent_mm_libwxperf_JNIObj_doMmap(JNIEnv *env, jobject instance) {

    void *p_mmap = mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    LOGD("Yves-debug", "map ptr = %p", p_mmap);
    munmap(p_mmap, 1024);

    p_mmap = mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    LOGD("Yves-debug", "map ptr = %p", p_mmap);

    munmap(p_mmap, 1024);

    p_mmap = mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    LOGD("Yves-debug", "map ptr = %p", p_mmap);

    p_mmap = mremap(p_mmap, 1024, 2048, MREMAP_MAYMOVE);
    LOGD("Yves-debug", "remap ptr = %p", p_mmap);

    p_mmap = mremap(p_mmap, 1024, 20480, MREMAP_MAYMOVE);
    LOGD("Yves-debug", "remap ptr = %p", p_mmap);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_libwxperf_JNIObj_reallocTest(JNIEnv *env, jobject instance) {
    void *p = malloc(8);
    LOGD("Yves-debug", "malloc p = %p", p);

    p = realloc(p, 4);
    LOGD("Yves-debug", "realloc p = %p", p);
}

/*
 * Class:     com_example_meminfo_JNIObj
 * Method:    allocMem
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_com_tencent_mm_libwxperf_JNIObj_doSomeThing(JNIEnv *env, jobject instance) {
    LOGD("Yves-sample", ">>>>>>>>>>>>>>>>>>begin");
//    LOGD("Yves-sample", "malloc size = 9012111");
//    malloc(9012);
//    int *p = (int *)malloc(10 * sizeof(int));
//    for (int i = 0; i < 10; ++i) {
//        p[i] = 11;
//    }
//    for (int i = 0; i < 10; ++i) {
//        LOGD("Yves-sample", "----- %d", p[i]);
//    }
//    count++;
//    if (count % 2) {
//        free(p);
//    }

    int * pi = new int;
//    pi = new int;
//    pi = new int;
//    pi = new int;

    int LEN = 10000;

    int * p_arr[LEN];

    for (int i = 0; i < LEN; ++i) {
        p_arr[i] = NULL;
    }

    void *p_mmap = mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

#define ALLOC \
    arr = new int[1 * 10];\
    memset(arr, 1, 1 * 10);\
    p_arr[i++] = arr;

    for (int i = 0; i < LEN - 200;) {
        int *
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC

        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC

        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC

        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC

        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC

        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC

        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC

        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC

        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC

        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC

        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC

        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC

        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC

        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC

        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC

        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC

        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC

        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC

        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC

        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC
        ALLOC


    }

//    LOGD("Yves-sample", "sleep");
//    sleep(3);
//    LOGD("Yves-sample", "awake");


    for (int i = 0; i < LEN; ++i) {
        int *arr = p_arr[i];
//        LOGD("Yves-sample", "aar = %p", arr);
        if (arr) {
            delete[] arr;
        }
    }

    *pi = count++;
    LOGD("Yves-sample", " pi = %d", *pi);

    LOGD("Yves-sample", "<<<<<<<<<<<<<<<<<<<<end");

}

JNIEXPORT void JNICALL
Java_com_tencent_mm_libwxperf_JNIObj_nullptr(JNIEnv *env, jobject instance, jobjectArray ss) {

    if (!ss) {
        LOGD("Yves-debug", "ss is null");
    } else {
        LOGD("Yves-debug", "ss is not null");
    }

}

JNIEXPORT void JNICALL
Java_com_tencent_mm_libwxperf_JNIObj_dump(JNIEnv *env, jobject instance, jstring path_) {

    const char *path = env->GetStringUTFChars(path_, 0);

    LOGD("Yves-debug", "path = %s", path);
//    dump();

    env->ReleaseStringUTFChars(path_, path);

    LOGD("Yves-sample", ">>>>>>>>>>>>>>>>>>begin dump");
//    typedef void (*FN_DUMP)();
//    const char * path = env->GetStringUTFChars(libPath, 0);
//    void *handle = dlopen(path, RTLD_LAZY);
//    env->ReleaseStringUTFChars(libPath, path);
//    if (handle != nullptr) {
//        LOGD("Yves-sample","so not null");
//        auto p = (FN_DUMP)dlsym(handle, "dump");
//        if (p != nullptr) {
//            LOGD("Yves-sample","dump func not null");
//            p();
//        }
//    }
//    int err;
//    errno = 0;
//    FILE *file = fopen("/sdcard/memory_hook_fopen.log", "w+");
//    err = errno;
//    LOGD("Yves-sample", "fopen: errno = %s", strerror(err));
//    if (file) {
//        fputs("fputs\n", file);
//        err = errno;
//        LOGD("Yves-sample", "fopen: errno = %s", strerror(err));
//        fflush(file);
//        fclose(file);
//    }
////
////
////
//    errno = 0;
//
//    int fd = TEMP_FAILURE_RETRY(open("/sdcard/memory_hook.log", O_RDWR | O_CREAT));
//    err = errno;
//    LOGD("Yves-sample", "open fd = %d, errno = %s %d", fd, strerror(err),err);
//    if (fd != -1) {
//        write(fd, "abbbbbbbb\n",10);
//        close(fd);
//    }
//    err = errno;
//    LOGD("Yves-sample", "write fd = %d, errno = %s", fd, strerror(err));
//
////    int fd2 = open("/data/data/com.tencent.mm.libwxperf/cache/memory_hook.log", O_RDWR | O_CREAT);
////    err = errno;
////    LOGD("Yves-sample", "open fd = %d, errno = %s", fd2, strerror(err));
////    if (fd2 != -1) {
////        write(fd2, "aaaaaa\n", 7);
////    }
////    err = errno;
////    LOGD("Yves-sample", "write fd = %d, errno = %s", fd2, strerror(err));
////    close(fd2);
//
//
//    LOGD("Yves-sample", "<<<<<<<<<<<<<<<<<<<<end dump");
}

#define NAMELEN 16

static void *
threadfunc(void *parm)
{
    LOGD("Yves-debug", ">>>>new thread=%ld, tid=%d", pthread_self(), pthread_gettid_np(pthread_self()));
    sleep(5);          // allow main program to set the thread name
    return NULL;
}


#include <sys/prctl.h>

static int read_thread_name(pthread_t __pthread, char *__buf, size_t __n) {
    if (!__buf) {
        return -1;
    }

    char proc_path[__n];

    sprintf(proc_path, "/proc/self/task/%d/stat", pthread_gettid_np(__pthread));

    FILE *file = fopen(proc_path, "r");

    if (!file) {
        LOGD("Yves-debug", "file not found: %s", proc_path);
        return -1;
    }

    fscanf(file, "%*d (%[^)]", __buf);

    fclose(file);

    return 0;
}

static int wrap_pthread_getname_np(pthread_t __pthread, char* __buf, size_t __n) {
#if __ANDROID_API__ >= 26
    return pthread_getname_np(__pthread, __buf, __n);
#else
    return read_thread_name(__pthread, __buf, __n);
#endif
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_libwxperf_JNIObj_testThread(JNIEnv *env, jclass clazz) {

    pthread_t thread;
    int rc;
    char thread_name[NAMELEN];

    rc = pthread_create(&thread, NULL, threadfunc, NULL);

    pthread_setname_np(thread, "123456789ABCDEF");

    if (rc != 0)
        LOGD("Yves-debug","pthread_create: %d", rc);
    LOGD("Yves-debug", "origin thread=%ld, tid=%d", pthread_self(), pthread_gettid_np(pthread_self()));

    wrap_pthread_getname_np(thread, thread_name, 16);

    LOGD("Yves-debug","Created a thread. Default name is: %s", thread_name);
}

pthread_key_t key;

void dest(void *arg) {
    LOGD("Yves-debug", "dest is running on %ld arg=%p, *arg=%d", pthread_self(), arg, *(int *)arg);
}

void *threadfunc2(void *arg) {
    LOGD("Yves-debug", "setting specific");
    pthread_t pthread = pthread_self();
    pthread_setspecific(key, arg);

    int *pa = static_cast<int *>(pthread_getspecific(key));

    LOGD("Yves-debug", "in thread arg = %p, pa=%p, *pa=%d", arg, pa, *pa);
    return NULL;
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_libwxperf_JNIObj_testThreadSpecific(JNIEnv *env, jclass clazz) {
    if (!key) {
        pthread_key_create(&key, dest);
    }
    pthread_t thread1;

    int *a = new int;
    *a = 10086;

    LOGD("Yves-debug", "origin a = %p", a);

    pthread_create(&thread1, NULL, threadfunc2, a);
    LOGD("Yves-debug", "creating thread thread1 %ld", thread1);
//    pthread_join(thread1, NULL);
}

#ifdef __cplusplus
}
#endif