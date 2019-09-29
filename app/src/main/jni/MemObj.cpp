#include <jni.h>
#include <malloc.h>

#include <android/log.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <vector>

#define LOGD(TAG, FMT, args...) __android_log_print(ANDROID_LOG_DEBUG, TAG, FMT, ##args)

#ifdef __cplusplus
extern "C" {
#endif

int count = 0;

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
        LOGD("Yves.debug", "ss is null");
    } else {
        LOGD("Yves.debug", "ss is not null");
    }

}

JNIEXPORT void JNICALL
Java_com_tencent_mm_libwxperf_JNIObj_dump(JNIEnv *env, jobject instance, jstring path_) {

    const char *path = env->GetStringUTFChars(path_, 0);

    LOGD("Yves.debug", "path = %s", path);
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

#ifdef __cplusplus
}
#endif