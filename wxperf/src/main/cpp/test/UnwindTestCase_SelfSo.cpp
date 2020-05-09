#include <jni.h>
#include <dlfcn.h>
#include <cinttypes>
#include <cxxabi.h>
#include "Log.h"
#include "UnwindTestCommon.h"
#include "../external/libunwindstack/TimeUtil.h"
#include "UnwindTestCase_SelfSo.h"

#define TESTCASE_SELF_SO "self-so"

#ifdef __cplusplus
extern "C" {
#endif

void selfso_func0f() {

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

    leaf_func(TESTCASE_SELF_SO);
}

void selfso_func0e() {

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

    selfso_func0f();
}

void selfso_func0d() {

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

    selfso_func0e();
}

void selfso_func0c() {

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

    selfso_func0d();
}

void selfso_func0b() {

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
    selfso_func0c();
}

void selfso_func0a(){

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

    selfso_func0b();
}

void selfso_func09(){
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

    selfso_func0a();
}

void selfso_func08(){

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

    LOGD(UNWIND_TEST_TAG, "a[9] = %d", a[9]);

    selfso_func09();
}

void selfso_func07() {

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

    LOGD(UNWIND_TEST_TAG, "a[9] = %d", a[9]);

    selfso_func08();
}

void selfso_func06(long a1,long a2, long a3, long a4, long a5,long a6, long a7) {

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

    LOGD(UNWIND_TEST_TAG, "a[9] = %d", a[9]);

    selfso_func07();
}

void selfso_func05(long a1,long a2, long a3, long a4, long a5,long a6) {

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

    LOGD(UNWIND_TEST_TAG, "a[9] = %d", a[9]);

    int aa = a4;
    int bb = a3;
    int cc = a2;
    int ee = a1;

    for (int i = 0; i < 100; ++i) {
        aa = aa + bb++ + cc++ + ee++;
        LOGD(UNWIND_TEST_TAG, "%d,%d",aa, bb);
    }

    LOGD(UNWIND_TEST_TAG, "a[9] = %d%d%d%d%d%d%d", a[9], a1,a2,a3,a4,a5,a6);



    selfso_func06(a[0],a[1],a[2],a[3],a[4],a[5], b[6]);
}

void selfso_func04() {

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
        LOGD(UNWIND_TEST_TAG, "%d,%d",aa1, bb);
    }

    LOGD(UNWIND_TEST_TAG, "a[9] = %d", a[9]);

    selfso_func05(a[0],a[1],a[2],a[3],a[4],a[5]);
}

void selfso_func03(){

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

    LOGD(DWARF_UNWIND_TEST, "a[9] = %d", a[9]);



    LOGD(DWARF_UNWIND_TEST, "a[9] = %d", a[9]);

    selfso_func04();
}

void selfso_func02(long a0, int a1, int a2){
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


    LOGD(DWARF_UNWIND_TEST, "a[9] = %d %d%d%d", a[9], a0, a1, a2);


    selfso_func03();
}

void selfso_func01(int arg) {

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

    LOGD(DWARF_UNWIND_TEST, "a[9] = %d", a[9]);

    selfso_func02(a[0],a[2],a[2]);
}

void func_selfso() {
    LOGD(DWARF_UNWIND_TEST, "func_selfso");

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

    selfso_func01(1);
//    func1();
}

#ifdef __cplusplus
}
#endif