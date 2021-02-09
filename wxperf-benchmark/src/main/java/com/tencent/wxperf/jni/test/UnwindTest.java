package com.tencent.wxperf.jni.test;

import com.tencent.stubs.logger.Log;

public class UnwindTest {

    private final static String TAG = "Wxperf.UnwindTest";

    static {
        // FIXME
        try {
            System.loadLibrary("wxperf-jni");
            System.loadLibrary("wxperf-benchmark");
        } catch (Throwable e) {
            Log.printStack(Log.ERROR, TAG, e);
        }
    }

    public static void init() {
        initNative();
    }

    public static void test() {
        testNative();
    }

    public static void test2() {
        testNative2();
    }

    private native static void initNative();

    private native static void testNative();

    private native static void testNative2();

}