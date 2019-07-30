package com.tencent.mm.performance.jni.fd;

/**
 * Created by Yves on 2019-07-22
 */
public class FDDumpBridge {
    static {
        System.loadLibrary("wxperf");
    }
    public static native String getFdPathName(String path);
}
