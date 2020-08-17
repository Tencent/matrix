package com.tencent.wxperf.jni.test;

//import com.tencent.mm.performance.jni.LibWxPerfManager;

public class UnwindBenckmarkTest {

    static {
//        LibWxPerfManager.INSTANCE.init();
        System.loadLibrary("wxperf-jni");
    }

    public static native void benchmarkInitNative();

    public static native void benchmarkNative();

    public static native void debugNative();
}
