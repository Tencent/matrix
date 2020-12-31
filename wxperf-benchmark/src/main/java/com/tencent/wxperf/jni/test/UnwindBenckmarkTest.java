package com.tencent.wxperf.jni.test;

public class UnwindBenckmarkTest {

    private final static String TAG = "Wxperf.UnwindBenckmarkTest";

    static {
        System.loadLibrary("wxperf-jni");
        System.loadLibrary("wxperf-benchmark");
    }

    public static native void benchmarkInitNative();

    public static native void benchmarkNative();

    public static native void debugNative();
}
