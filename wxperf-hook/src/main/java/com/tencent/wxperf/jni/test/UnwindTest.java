package com.tencent.wxperf.jni.test;

//import com.tencent.mm.performance.jni.LibWxPerfManager;

public class UnwindTest {

    static {
//        LibWxPerfManager.INSTANCE.init();
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