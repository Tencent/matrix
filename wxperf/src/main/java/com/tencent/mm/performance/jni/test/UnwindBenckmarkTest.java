package com.tencent.mm.performance.jni.test;

import com.tencent.mm.performance.jni.LibWxPerfManager;

public class UnwindBenckmarkTest {

    static {
        LibWxPerfManager.INSTANCE.init();
    }

    public static native void benchmarkInitNative();

    public static native void benchmarkNative();

}
