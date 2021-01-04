package com.tencent.wxperf.fd;

import com.tencent.stubs.logger.Log;

/**
 * Created by Yves on 2019-07-22
 */
public class FDDumpBridge {

    private static final String TAG = "FDDumpBridge";

    private static boolean initialized;

    static {
        try {
            System.loadLibrary("wxperf-fd");
            initialized = true;
        } catch (Throwable e) {
            Log.printStack(Log.ERROR, TAG, e);
            initialized = false;
        }
    }

    public static String getFdPathName(String path) {
        if (!initialized) {
            return path;
        }
        return getFdPathNameNative(path);
    }

    public static native String getFdPathNameNative(String path);

    public static native int getFDLimit();
}
