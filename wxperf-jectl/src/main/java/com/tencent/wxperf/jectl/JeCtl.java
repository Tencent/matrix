package com.tencent.wxperf.jectl;

/**
 * Created by Yves on 2020/7/15
 */
public class JeCtl {

    static {
        System.loadLibrary("wxperf-jectl");
    }

    // 必须和 native 保持一致
    public static final int JECTL_OK           = 0;
    public static final int ERR_SO_NOT_FOUND   = 1;
    public static final int ERR_SYM_MALLCTL    = 2;
    public static final int ERR_SYM_OPT_RETAIN = 3;
    public static final int ERR_VERSION        = 4;
    public static final int ERR_64_BIT         = 5;

    public static int tryDisableRetain() {
        return tryDisableRetainNative();
    }

    private static native int tryDisableRetainNative();

}
