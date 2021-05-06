package com.tencent.matrix.benchmark.test;

public class UnwindBenckmarkTest {

    private final static String TAG = "Wxperf.UnwindBenckmarkTest";

    static {
        System.loadLibrary("matrix-hooks");
        System.loadLibrary("backtrace-benchmark");
    }

    public static native void benchmarkInitNative();

    public static native void benchmarkNative();

    public static native void debugNative();

    public static native void refreshMapsInfoNative();

    public static native void unwindAdapter();
}
