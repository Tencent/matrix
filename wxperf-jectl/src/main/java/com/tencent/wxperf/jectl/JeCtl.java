package com.tencent.wxperf.jectl;

/**
 * Created by Yves on 2020/7/15
 */
public class JeCtl {

    static {
        System.loadLibrary("wxperf-jectl");
    }

    public static final int ERR_VERSION = 1;
    public static final int ERR_SYMBOL_NOT_FOUND = 2;
    // TODO

    public static int tryDisableRetain() {
        return tryDisableRetainNative();
    }

    private static native int tryDisableRetainNative();

}
