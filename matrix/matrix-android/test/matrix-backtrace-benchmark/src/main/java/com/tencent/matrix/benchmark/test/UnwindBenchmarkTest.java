package com.tencent.matrix.benchmark.test;

public class UnwindBenchmarkTest {

    static {
        System.loadLibrary("backtrace-benchmark");
    }

    public static native void nativeInit();

    public static native void nativeBenchmark();

    public static native void nativeBenchmarkJavaStack();

    public static native void nativeTry();

    public static native void nativeRefreshMaps();

}
