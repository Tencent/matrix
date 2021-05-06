package com.tencent.matrix.benchmark.test;

public class UnwindBenchmarkTest {

    static {
        System.loadLibrary("backtrace-benchmark");
    }

    public static native void nativeInit();

    public static native void nativeBenchmark();

    public static native void nativeTry();

    public static native void nativeRefreshMaps();

    public static native void nativeUnwindAdapter();

    public static void testWarmUp() {

    }
}
