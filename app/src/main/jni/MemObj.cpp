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

    char proc_path[128];

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

    int *b = new int;
    *b = 1024;

    pthread_setspecific(key, b);


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

struct Obj {
    int a;
    int b;
};

void copy(std::vector<Obj> *dest) {

    std::vector<Obj> src;

    src.push_back({11,22});
    src.push_back({22,22});
    src.push_back({3,22});
    src.push_back({4,22});
    src.push_back({5,22});
    src.push_back({6,22});

    *dest = src;
}

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

JNIEXPORT void JNICALL
Java_com_tencent_mm_libwxperf_JNIObj_testJNICall(JNIEnv *env, jclass clazz) {

//    pthread_mutex_lock(&mutex);
//
//    jclass j_jniobj = env->FindClass("com/tencent/mm/libwxperf/JNIObj");
//    if (j_jniobj) {
//        jmethodID method = env->GetStaticMethodID(j_jniobj, "calledByJNI", "()Ljava/lang/String;");
//        if (method) {
//            LOGD("Yves-debug", "before call java");
//            jstring jstr = (jstring)env->CallStaticObjectMethod(j_jniobj, method);
//            LOGD("Yves-debug", "after call java");
//
//            const char *stack = env->GetStringUTFChars(jstr, NULL);
//
//            LOGD("Yves-debug", " stack = %s", stack);
//
//            env->ReleaseStringUTFChars(jstr, stack);
//        }
//    } else {
//        LOGD("Yves-debug", "class not found");
//    }
//
//    pthread_mutex_unlock(&mutex);

//    std::atomic<std::vector<Obj> *> atomic_vec_ptr;
//
//    atomic_vec_ptr = new std::vector<Obj>;
//
//    copy(atomic_vec_ptr);
//
//    for (auto obj : *atomic_vec_ptr.load()) {
//        LOGD("Vector-test", "a=%d, b=%d", obj.a, obj.b);
//    }

    char * ch = static_cast<char *>(malloc(64));

    std::atomic<char *> atomic_ch;
    atomic_ch.store(ch);

    LOGD("Yves-debug", "atomic test: %p -> %p", ch, atomic_ch.load());

    char * ch2 = static_cast<char *>(malloc(64));

    std::atomic<char *> atomic_ch2;
    atomic_ch2.store(ch, std::memory_order_release);

    LOGD("Yves-debug", "atomic test 2 : %p -> %p", ch2, atomic_ch2.load(std::memory_order_acquire));

}

void *thread_rountine(void *arg) {
    free(arg);
    LOGD("Yves-debug", "free thread %d", pthread_gettid_np(pthread_self()));
    return 0;
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_libwxperf_JNIObj_testPthreadFree(JNIEnv *env, jclass clazz) {
    LOGD("Yves-debug", "malloc thread %d", pthread_gettid_np(pthread_self()));

    pthread_t thread;

    void *p = malloc(128 * 1024);
    LOGD("Yves-debug", "p1 = %p", p);
    pthread_create(&thread, NULL, thread_rountine, p);
    pthread_join(thread, NULL);
//    free(p);

    p = malloc(128 * 1024);
    LOGD("Yves-debug", "p2 = %p", p);
    pthread_create(&thread, NULL, thread_rountine, p);
    pthread_join(thread, NULL);
//    free(p);

    p = malloc(128 * 1024);
    LOGD("Yves-debug", "p3 = %p", p);
    pthread_create(&thread, NULL, thread_rountine, p);
    pthread_join(thread, NULL);
//    free(p);

    p = malloc(128 * 1024);
    LOGD("Yves-debug", "p4 = %p", p);
    pthread_create(&thread, NULL, thread_rountine, p);
    pthread_join(thread, NULL);
//    free(p);

    p = malloc(128 * 1024);
    LOGD("Yves-debug", "p5 = %p", p);
    pthread_create(&thread, NULL, thread_rountine, p);
    pthread_join(thread, NULL);
//    free(p);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_libwxperf_JNIObj_mallocTest(JNIEnv *env, jclass clazz) {
//    LOGD("Yves-debug", "mallocTest");
    Obj *p = new Obj;
    LOGD("Yves-debug", "mallocTest: new %p", p);
    delete p;
//    p = new int[2];
//    LOGD("Yves-debug", "mallocTest: new int %p", p);
//    delete p;
//
//    p = malloc(sizeof(Obj));
//    LOGD("Yves-debug", "mallocTest: malloc %p", p);
//    free(p);

}

JNIEXPORT void JNICALL
Java_com_tencent_mm_libwxperf_JNIObj_tlsTest(JNIEnv *env, jclass clazz) {

    if (!key) {
        pthread_key_create(&key, dest);
    }

    pthread_t pthread;
    int *a = new int;
    *a = 10086;
    pthread_create(&pthread, NULL, threadfunc2, a);
}

#ifdef __cplusplus
}
#endif