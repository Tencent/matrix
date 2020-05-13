package com.tencent.mm.libwxperf;

/**
 * Created by Yves on 2020-05-12
 */
public class MemoryBenchmarkTest {
    static {
        try {
            System.loadLibrary("native-lib");
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public static native void benchmarkNative();
}
