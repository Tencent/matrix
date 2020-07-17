package com.tencent.jectl;

/**
 * Created by Yves on 2020/7/15
 */
public class JeCtl {

    static {
        System.loadLibrary("wxperf-jectl");
    }

    public static final int ERR_VERSION = 1;
    public static final int ERR_SYMBOL_NOT_FOUND = 2;

    public static int tryCtlRetain() {
        return 0;
    }

    private static native int tryCtlRetainNative();

}
