#include <jni.h>
#include <dlfcn.h>
#include <cinttypes>
#include <cxxabi.h>
#include "Log.h"
#include "UnwindTestCommon.h"
#include "../external/libunwindstack/TimeUtil.h"
#include "UnwindTestCase_ThroughJNI.h"

#define TESTCASE_THROUGH_JNI "throught-jni"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LOGD
#undef LOGD
#endif

#define LOGD(TAG, FMT, args...) //

void throughjni_func0f() {

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

    LOGD(UNWIND_TEST_TAG, "a[9] = %d", a[9]);

    leaf_func(TESTCASE_THROUGH_JNI);
}

void throughjni_func0e() {

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

    LOGD(UNWIND_TEST_TAG, "a[9] = %d", a[9]);

    throughjni_func0f();
}

void throughjni_func0d() {

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

    LOGD(UNWIND_TEST_TAG, "a[9] = %d", a[9]);

    throughjni_func0e();
}

void throughjni_func0c() {

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

    LOGD(UNWIND_TEST_TAG, "a[9] = %d", a[9]);

    throughjni_func0d();
}

void throughjni_func0b() {

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

    LOGD(UNWIND_TEST_TAG, "a[9] = %d", a[9]);
    throughjni_func0c();
}

void throughjni_func0a(){

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

    LOGD(UNWIND_TEST_TAG, "a[9] = %d", a[9]);

    throughjni_func0b();
}

void throughjni_func09(){
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

    LOGD(UNWIND_TEST_TAG, "a[9] = %d", a[9]);

    throughjni_func0a();
}

void func_throughjni() {
    LOGD(DWARF_UNWIND_TEST, "func_throughjni");

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

    LOGD(UNWIND_TEST_TAG, "a[9] = %d", a[9]);

    throughjni_func09();
//    func1();
}

#ifdef __cplusplus
}
#endif