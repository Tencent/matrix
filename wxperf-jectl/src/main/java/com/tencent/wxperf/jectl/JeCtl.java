package com.tencent.wxperf.jectl;

import com.tencent.stubs.logger.Log;

/**
 * Created by Yves on 2020/7/15
 */
public class JeCtl {
    private static final String TAG = "Wxperf.JeCtl";

    private static boolean initialized = false;

    static {
        try {
            System.loadLibrary("wxperf-jectl");
            initNative();
            initialized = true;
        } catch (Throwable e) {
            Log.printStack(Log.ERROR, TAG, e);
        }
    }

    // 必须和 native 保持一致
    public static final int JECTL_OK         = 0;
    public static final int ERR_INIT_FAILED  = 1;
    public static final int ERR_VERSION      = 2;
    public static final int ERR_64_BIT       = 3;
    public static final int ERR_CTL          = 4;
    public static final int ERR_ALLOC_FAILED = 5;

    public synchronized static int compact() {
        if (!initialized) {
            Log.e(TAG, "JeCtl init failed! check if so exists");
            return ERR_INIT_FAILED;
        }
        return compactNative();
    }

    private static boolean hasAllocated;
    private static int     sLastPreAllocRet;

    public synchronized static int preAllocRetain(int size0, int size1, int limit0, int limit1) {
        if (!initialized) {
            Log.e(TAG, "JeCtl init failed! check if so exists");
            return ERR_INIT_FAILED;
        }
        if (!hasAllocated) {
            hasAllocated = true;
            sLastPreAllocRet = preAllocRetainNative(size0, size1, limit0, limit1);
        }

        return sLastPreAllocRet;
    }

    public synchronized static String version() {
        if (!initialized) {
            Log.e(TAG, "JeCtl init failed! check if so exists");
            return "VER_UNKNOWN";
        }

        return getVersionNative();
    }

    private static native void initNative();

    private static native int compactNative();

    private static native int preAllocRetainNative(int size0, int size1, int limit0, int limit1);

    private static native String getVersionNative();

    public static native boolean setRetain(boolean enable);
}
