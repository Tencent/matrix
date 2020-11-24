#include <jni.h>
#include <dlfcn.h>
#include <cinttypes>
#include <cxxabi.h>
#include "Log.h"
#include "UnwindTestCommon.h"
#include "../external/libunwindstack/TimeUtil.h"
#include "UnwindTestCase_TrhoughSystemSo.h"

#define TESTCASE_THROUGH_SYS_SO "through-sys-so"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LOGD
#undef LOGD
#endif

#define LOGD(TAG, FMT, args...) //

void throughsystemso_func0f() {

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

    leaf_func(TESTCASE_THROUGH_SYS_SO);
}

void throughsystemso_func0e() {

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

    throughsystemso_func0f();
}

void func_throughsystemso() {
    LOGD(DWARF_UNWIND_TEST, "func_throughsystemso");

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
                                           * f[i] - g[i] * 2 - h[i] - j[i] + k[i] // / l[i]
               + m[i] + n[i] * o[i] + p[i] + q[i];
    }

    LOGD(UNWIND_TEST_TAG, "a[9] = %d", a[9]);

    throughsystemso_func0e();
//    func1();
}

#ifdef __cplusplus
}
#endif