package com.tencent.wxperf.jni.test;

import com.tencent.stubs.logger.Log;

public class UnwindBenckmarkTest {

    private final static String TAG = "Wxperf.UnwindBenckmarkTest";

    static {
//        LibWxPerfManager.INSTANCE.init();
        System.loadLibrary("wxperf-jni");
    }

    public static native void benchmarkInitNative();

    public static native void benchmarkNative();

    public static native void debugNative();

    public static native void statisticNative(byte[] sopath, byte[] soname);
}
