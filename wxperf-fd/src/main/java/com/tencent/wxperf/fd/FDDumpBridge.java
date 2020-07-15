package com.tencent.wxperf.fd;

/**
 * Created by Yves on 2019-07-22
 */
public class FDDumpBridge {

    private static final String TAG = "FDDumpBridge";

    static {
        System.loadLibrary("wxperf-fd");
    }

    public static String getFdPathName(String path) {
        return getFdPathNameNative(path);
    }

    public static native String getFdPathNameNative(String path);
}
