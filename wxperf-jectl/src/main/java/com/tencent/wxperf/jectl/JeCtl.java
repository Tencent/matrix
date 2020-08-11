package com.tencent.wxperf.jectl;

/**
 * Created by Yves on 2020/7/15
 */
public class JeCtl {

    static {
        System.loadLibrary("wxperf-jectl");
    }

    // 必须和 native 保持一致
    public static final int JECTL_OK        = 0;
    public static final int ERR_INIT_FAILED = 1;
    public static final int ERR_VERSION     = 2;
    public static final int ERR_64_BIT      = 3;

    public synchronized static int compact() {
        return compactNative();
    }

    private static native int compactNative();
}
