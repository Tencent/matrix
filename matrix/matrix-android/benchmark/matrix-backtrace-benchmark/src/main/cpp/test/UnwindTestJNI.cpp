//
// Created by yves on 20-4-11.
//

#include <jni.h>
#include <dlfcn.h>
#include <cinttypes>
#include <cxxabi.h>
#include <Backtrace.h>
#include "Log.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LOGD
    #undef LOGD
#endif

#define LOGD(TAG, FMT, args...) //

void func1() {
    LOGD("Yves-test", "func1");

    LOGD("Unwind-test", "unwind begin");

    // TODO
//    auto *tmp_ns = new std::vector<unwindstack::FrameData>;

//    unwindstack::do_unwind(*tmp_ns);

//    long begin = CurrentNano();
//
//    std::vector<int> vec;
//    vec.reserve(1000000);
//    for (int i = 0; i < 1000000; ++i) {
//        vec.emplace_back(i);
//    }
//
//    LOGE("Unwind-test", "vec cost = %ld", (CurrentNano() - begin));
//    begin = CurrentNano();

//    int a[1000000];
//    for (int i = 0; i < 1000000; ++i) {
//        a[i] = i;
//    }

//    LOGE("Unwind-test", "arr cost = %ld", (CurrentNano() - begin));

//    LOGI("Unwind-test", "frames = %zu", tmp_ns->size());
//
//    for (auto p_frame = tmp_ns->begin(); p_frame != tmp_ns->end(); ++p_frame) {
//        Dl_info stack_info;
//        dladdr((void *) p_frame->pc, &stack_info);
//
//        std::string so_name = std::string(stack_info.dli_fname);
//
//        int status = 0;
////        char *demangled_name = abi::__cxa_demangle(stack_info.dli_sname, nullptr, 0,
////                                                   &status);
//
//        LOGI("Unwind-test", "  #pc %llx %llu %llx %s (%s)", p_frame->rel_pc, p_frame->pc, p_frame->pc,
//        /*demangled_name ? demangled_name : "(null)",*/ stack_info.dli_sname, stack_info.dli_fname);
//    }

    LOGI("Unwind-test", "unwind done");
}

void func0f() {

    const int len = 70;

    long a[len] = {0};
    long b[len] = {9};
    long c[len] = {9};
    long d[len] = {9};
    long e[len] = {9};
    long f[len] = {9};
    long g[len] = {9};
    long h[len] = {9};

    for (int i = 0; i < len; ++i) {
        a[i] = i;
        b[i] = i * 3;
        c[i] = i + 3;
        d[i] = i * 2;
        e[i] = i * 2;
        f[i] = i + 2;
        g[i] = i + 2;
        h[i] = i + 2;
    }

    for (int i = 0; i < len; ++i) {
        a[i] = a[i] + b[i] + c[i] - d[i] + e[i] * f[i] - g[i] * 2 - h[i];
    }

    LOGD("Yves-test", "a[9] = %d", a[9]);

    func1();
}

void func0e() {

    const int len = 70;

    long a[len] = {0};
    long b[len] = {9};
    long c[len] = {9};
    long d[len] = {9};
    long e[len] = {9};
    long f[len] = {9};
    long g[len] = {9};
    long h[len] = {9};

    for (int i = 0; i < len; ++i) {
        a[i] = i;
        b[i] = i * 3;
        c[i] = i + 3;
        d[i] = i * 2;
        e[i] = i * 2;
        f[i] = i + 2;
        g[i] = i + 2;
        h[i] = i + 2;
    }

    for (int i = 0; i < len; ++i) {
        a[i] = a[i] + b[i] + c[i] - d[i] + e[i] * f[i] - g[i] * 2 - h[i];
    }

    LOGD("Yves-test", "a[9] = %d", a[9]);

    func0f();
}

void func0d() {

    const int len = 70;

    long a[len] = {0};
    long b[len] = {9};
    long c[len] = {9};
    long d[len] = {9};
    long e[len] = {9};
    long f[len] = {9};
    long g[len] = {9};
    long h[len] = {9};

    for (int i = 0; i < len; ++i) {
        a[i] = i;
        b[i] = i * 3;
        c[i] = i + 3;
        d[i] = i * 2;
        e[i] = i * 2;
        f[i] = i + 2;
        g[i] = i + 2;
        h[i] = i + 2;
    }

    for (int i = 0; i < len; ++i) {
        a[i] = a[i] + b[i] + c[i] - d[i] + e[i] * f[i] - g[i] * 2 - h[i];
    }

    LOGD("Yves-test", "a[9] = %d", a[9]);

    func0e();
}

void func0c() {

    const int len = 70;

    long a[len] = {0};
    long b[len] = {9};
    long c[len] = {9};
    long d[len] = {9};
    long e[len] = {9};
    long f[len] = {9};
    long g[len] = {9};
    long h[len] = {9};

    for (int i = 0; i < len; ++i) {
        a[i] = i;
        b[i] = i * 3;
        c[i] = i + 3;
        d[i] = i * 2;
        e[i] = i * 2;
        f[i] = i + 2;
        g[i] = i + 2;
        h[i] = i + 2;
    }

    for (int i = 0; i < len; ++i) {
        a[i] = a[i] + b[i] + c[i] - d[i] + e[i] * f[i] - g[i] * 2 - h[i];
    }

    LOGD("Yves-test", "a[9] = %d", a[9]);

    func0d();
}

void func0b() {

    const int len = 64;

    long a[len] = {0};
    long b[len] = {9};
    long c[len] = {9};
    long d[len] = {9};
    long e[len] = {9};
    long f[len] = {9};
    long g[len] = {9};
    long h[len] = {9};

    for (int i = 0; i < len; ++i) {
        a[i] = i;
        b[i] = i * 2;
        c[i] = i * 2;
        d[i] = i * 2;
        e[i] = i * 2;
        f[i] = i + 2;
        g[i] = i + 2;
        h[i] = i + 2;
    }

    for (int i = 0; i < len; ++i) {
        a[i] = a[i] + b[i] + c[i] - d[i] + e[i] * f[i] - g[i] * 2 - h[i];
    }

    LOGD("Yves-test", "a[9] = %d", a[9]);
    func0c();
}

void func0a(){

    const int len = 64;

    long a[len] = {0};
    long b[len] = {9};
    long c[len] = {9};
    long d[len] = {9};
    long e[len] = {9};
    long f[len] = {9};
    long g[len] = {9};

    for (int i = 0; i < len; ++i) {
        a[i] = i;
        b[i] = i * 2;
        c[i] = i * 2;
        d[i] = i * 2;
        e[i] = i * 2;
        f[i] = i + 2;
        g[i] = i + 2;
    }

    for (int i = 0; i < len; ++i) {
        a[i] = a[i] + b[i] + c[i] - d[i] + e[i] * f[i] - g[i] * 2;
    }

    LOGD("Yves-test", "a[9] = %d", a[9]);

    func0b();
}

void func09(){
    const int len = 70;

    long a[len] = {0};
    long b[len] = {9};
    long c[len] = {9};
    long d[len] = {9};
    long e[len] = {9};
    long f[len] = {9};
    long g[len] = {9};
    long h[len] = {9};
    long k[len] = {9};
    long j[len] = {9};

    for (int i = 0; i < len; ++i) {
        a[i] = i;
        b[i] = i * 3;
        c[i] = i + 3;
        d[i] = i * 2;
        e[i] = i * 2;
        f[i] = i + 2;
        g[i] = i + 2;
        h[i] = i + 4;
        j[i] = i * 3;
        k[i] = i / 2;
    }

    for (int i = 0; i < len; ++i) {
        a[i] = a[i] + b[i] + c[i] - d[i] + e[i] * f[i] - g[i] * 2 - h[i] - j[i] + k[i];
    }

    LOGD("Yves-test", "a[9] = %d", a[9]);

    func0a();
}

void func08(){

    const int len = 32;

    long a[len] = {0};
    long b[len] = {9};
    long c[len] = {9};
    long d[len] = {9};
    long e[len] = {9};

    for (int i = 0; i < len; ++i) {
        a[i] = i;
        b[i] = i * 2;
        c[i] = i * 2;
        d[i] = i * 2;
        e[i] = i * 2;
    }


    for (int i = 0; i < len; ++i) {
        a[i] = a[i] + b[i] + c[i] - d[i] + e[i];
    }

    LOGD("Yves-test", "a[9] = %d", a[9]);

    func09();
}

void func07() {

    const int len = 70;

    long a[len] = {0};
    long b[len] = {9};
    long c[len] = {9};
    long d[len] = {9};
    long e[len] = {9};
    long f[len] = {9};
    long g[len] = {9};
    long h[len] = {9};
    long k[len] = {9};
    long j[len] = {9};
    long l[len] = {9};

    for (int i = 0; i < len; ++i) {
        a[i] = i;
        b[i] = i * 3;
        c[i] = i + 3;
        d[i] = i * 2;
        e[i] = i * 2;
        f[i] = i + 2;
        g[i] = i + 2;
        h[i] = i + 4;
        j[i] = i * 3;
        k[i] = i / 2;
        l[i] = i % 10;
    }

    for (int i = 0; i < len; ++i) {
        a[i] = a[i] + b[i] + c[i] - d[i] + e[i] * f[i] - g[i] * 2 - h[i] - j[i] + k[i] / l[i];
    }

    LOGD("Yves-test", "a[9] = %d", a[9]);

    func08();
}

void func06(long a1,long a2, long a3, long a4, long a5,long a6, long a7) {

    const int len = 70;

    long a[len] = {0};
    long b[len] = {9};
    long c[len] = {9};
    long d[len] = {9};
    long e[len] = {9};
    long f[len] = {9};
    long g[len] = {9};
    long h[len] = {9};
    long k[len] = {9};
    long j[len] = {9};
    long l[len] = {9};
    long m[len] = {9};

    for (int i = 0; i < len; ++i) {
        a[i] = i;
        b[i] = i * 3;
        c[i] = i + 3;
        d[i] = i * 2;
        e[i] = i * 2;
        f[i] = i + a7;
        g[i] = i + a6;
        h[i] = i + a5;
        j[i] = i * a4;
        k[i] = i / a3;
        l[i] = i % a2;
        m[i] = i + a1;
    }

    for (int i = 0; i < len; ++i) {
        a[i] = a[i] + b[i] + c[i] - d[i] + e[i]
                * f[i] - g[i] * 2 - h[i] - j[i] + k[i] / l[i]
                + m[i];
    }

    LOGD("Yves-test", "a[9] = %d", a[9]);

    func07();
}

void func05(long a1,long a2, long a3, long a4, long a5,long a6) {

    const int len = 70;

    long a[len] = {0};
    long b[len] = {9};
    long c[len] = {9};
    long d[len] = {9};
    long e[len] = {9};
    long f[len] = {9};
    long g[len] = {9};
    long h[len] = {9};
    long k[len] = {9};
    long j[len] = {9};
    long l[len] = {9};
    long m[len] = {9};
    long n [len] = {9};

    for (int i = 0; i < len; ++i) {
        a[i] = i;
        b[i] = i * 3;
        c[i] = i + 3;
        d[i] = i * 2;
        e[i] = i * 2;
        f[i] = i + 20;
        g[i] = i + a6;
        h[i] = i + a5;
        j[i] = i * a4;
        k[i] = i / a3;
        l[i] = i % a2;
        m[i] = i + a1;
        n[i] = i + 100;
    }

    for (int i = 0; i < len; ++i) {
        a[i] = a[i] + b[i] + c[i] - d[i] + e[i]
                                           * f[i] - g[i] * 2 - h[i] - j[i] + k[i] / l[i]
               + m[i] + n[i];
    }

    LOGD("Yves-test", "a[9] = %d", a[9]);

    int aa = a4;
    int bb = a3;
    int cc = a2;
    int ee = a1;

    for (int i = 0; i < 100; ++i) {
        aa = aa + bb++ + cc++ + ee++;
        LOGD("Yves-test", "%d,%d",aa, bb);
    }

    LOGD("Yves-test", "a[9] = %d%d%d%d%d%d%d", a[9], a1,a2,a3,a4,a5,a6);



    func06(a[0],a[1],a[2],a[3],a[4],a[5], b[6]);
}

void func04() {

    const int len = 32;

    int a[len] = {0};
    int b[len] = {9};
    int c[len] = {9};
    int d[len] = {9};
    int e[len] = {9};

    for (int i = 0; i < len; ++i) {
        a[i] = i;
        b[i] = i * 2;
        c[i] = i * 2;
        d[i] = i * 2;
        e[i] = i * 2;
    }

    for (int i = 0; i < len; ++i) {
        a[i] = a[i] + b[i] + c[i] - d[i] + e[i];
    }

    int aa[len] = {0};
    int ab[len] = {9};
    int ac[len] = {9};
    int ad[len] = {9};
    int ae[len] = {9};

    for (int i = 0; i < len; ++i) {
        aa[i] = i;
        ab[i] = i * 2;
        ac[i] = i * 2;
        ad[i] = i * 2;
        ae[i] = i * 2;
    }

    for (int i = 0; i < len; ++i) {
        aa[i] = aa[i] + ab[i] + ac[i] - ad[i] + ae[i];
    }

    int aa1 = 0;
    int bb = 0;
    int cc = 0;

    for (int i = 0; i < 100; ++i) {
        aa1 = aa1 + bb++ + cc++;
        LOGD("Yves-test", "%d,%d",aa1, bb);
    }

    LOGD("Yves-test", "a[9] = %d", a[9]);

    func05(a[0],a[1],a[2],a[3],a[4],a[5]);
}

void func03(){

    const int len = 70;

    long a[len] = {0};
    long b[len] = {9};
    long c[len] = {9};
    long d[len] = {9};
    long e[len] = {9};
    long f[len] = {9};
    long g[len] = {9};
    long h[len] = {9};
    long k[len] = {9};
    long j[len] = {9};
    long l[len] = {9};
    long m[len] = {9};
    long n [len] = {9};
    long o[len] = {9};

    for (int i = 0; i < len; ++i) {
        a[i] = i;
        b[i] = i * 3;
        c[i] = i + 3;
        d[i] = i * 2;
        e[i] = i * 2;
        f[i] = i + 20;
        g[i] = i + 12;
        h[i] = i + 3;
        j[i] = i * 4;
        k[i] = i / 6;
        l[i] = i % 7;
        m[i] = i + 8;
        n[i] = i + 100;
        o[i] = i - 10;
    }

    for (int i = 0; i < len; ++i) {
        a[i] = a[i] + b[i] + c[i] - d[i] + e[i]
                                           * f[i] - g[i] * 2 - h[i] - j[i] + k[i] / l[i]
               + m[i] + n[i] * o[i];
    }

    LOGD("Yves-test", "a[9] = %d", a[9]);



    LOGD("Yves-test", "a[9] = %d", a[9]);

    func04();
}

void func02(long a0, int a1, int a2){
    const int len = 11;

    uint64_t a[len] = {0};
    uint64_t c[len] = {0};
    uint64_t b[len] = {0};
    uint64_t d[len] = {0};

    for (int i = 0; i < len; ++i) {
        if (i == 0) {
            a[i] = 0;
            continue;
        }
        if (i == 1) {
            a[i] = 1;
            continue;
        }

        a[i] = a[i - 1] + a [i - 2];
    }

    for (int i = 0; i < len; ++i) {
        if (i == 0) {
            c[i] = 0;
            continue;
        }
        if (i == 1) {
            c[i] = 1;
            continue;
        }

        c[i] = c[i - 1] + c [i - 2];
    }

    for (int i = 0; i < len; ++i) {
        if (i == 0) {
            b[i] = 0;
            continue;
        }
        if (i == 1) {
            b[i] = 1;
            continue;
        }

       b[i] = b[i - 1] + b [i - 2];
    }

    for (int i = 0; i < len; ++i) {
        if (i == 0) {
            d[i] = 0;
            continue;
        }
        if (i == 1) {
            d[i] = 1;
            continue;
        }

        d[i] = d[i - 1] + d [i - 2];
    }


    LOGD("Yves-test", "a[9] = %d %d%d%d", a[9], a0, a1, a2);


    func03();
}

void func01(int arg) {

    const int len = 70;

    long a[len] = {0};
    long b[len] = {9};
    long c[len] = {9};
    long d[len] = {9};
    long e[len] = {9};
    long f[len] = {9};
    long g[len] = {9};
    long h[len] = {9};
    long k[len] = {9};
    long j[len] = {9};
    long l[len] = {9};
    long m[len] = {9};
    long n [len] = {9};
    long o[len] = {9};
    long p[len] = {9};

    for (int i = 0; i < len; ++i) {
        a[i] = i;
        b[i] = i * 3;
        c[i] = i + 3;
        d[i] = i * 2;
        e[i] = i * 2;
        f[i] = i + 20;
        g[i] = i + 12;
        h[i] = i + 3;
        j[i] = i * 4;
        k[i] = i / 6;
        l[i] = i % 7;
        m[i] = i + 8;
        n[i] = i + 100;
        o[i] = i - 10;
        p[i] = i * 10;
    }

    for (int i = 0; i < len; ++i) {
        a[i] = a[i] + b[i] + c[i] - d[i] + e[i]
                                           * f[i] - g[i] * 2 - h[i] - j[i] + k[i] / l[i]
               + m[i] + n[i] * o[i] + p[i];
    }

    // if else

    LOGD("Yves-test", "a[9] = %d", a[9]);

    func02(a[0],a[2],a[2]);
}

void func0() {
    LOGD("Yves-test", "func0");

    const int len = 70;

    long a[len] = {0};
    long b[len] = {9};
    long c[len] = {9};
    long d[len] = {9};
    long e[len] = {9};
    long f[len] = {9};
    long g[len] = {9};
    long h[len] = {9};
    long k[len] = {9};
    long j[len] = {9};
    long l[len] = {9};
    long m[len] = {9};
    long n [len] = {9};
    long o[len] = {9};
    long p[len] = {9};
    long q[len] = {9};

    for (int i = 0; i < len; ++i) {
        a[i] = i;
        b[i] = i * 3;
        c[i] = i + 3;
        d[i] = i * 2;
        e[i] = i * 2;
        f[i] = i + 20;
        g[i] = i + 12;
        h[i] = i + 3;
        j[i] = i * 4;
        k[i] = i / 6;
        l[i] = i % 7;
        m[i] = i + 8;
        n[i] = i + 100;
        o[i] = i - 10;
        p[i] = i * 10;
        q[i] = i * 10;
    }

    for (int i = 0; i < len; ++i) {
        a[i] = a[i] + b[i] + c[i] - d[i] + e[i]
                                           * f[i] - g[i] * 2 - h[i] - j[i] + k[i] / l[i]
               + m[i] + n[i] * o[i] + p[i] + q[i];
    }

    LOGD("Yves-test", "a[9] = %d", a[9]);

    func01(1);
//    func1();
}


JNIEXPORT void JNICALL
Java_com_tencent_matrix_benchmark_test_UnwindTest_testNative(JNIEnv *env, jclass clazz) {
    func0();
}

void func3(JNIEnv *env, jclass clazz) {
    LOGD("Yves-test", "func3");
    jmethodID j_test = env->GetStaticMethodID(clazz, "test", "()V");
    env->CallStaticVoidMethod(clazz, j_test);
}

void func2(JNIEnv *env, jclass clazz) {
    LOGD("Yves-test", "func2");
    func3(env, clazz);
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_benchmark_test_UnwindTest_testNative2(JNIEnv *env, jclass clazz) {
    func2(env, clazz);
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_benchmark_test_UnwindTest_initNative(JNIEnv *env, jclass clazz) {
    LOGD("Yves-test", "Unwind init");
    wechat_backtrace::BACKTRACE_FUNC_WRAPPER(notify_maps_changed)();
}

#ifdef __cplusplus
}
#endif