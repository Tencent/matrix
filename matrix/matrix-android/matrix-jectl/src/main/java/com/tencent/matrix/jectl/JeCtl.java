package com.tencent.matrix.jectl;


import androidx.annotation.Keep;

import com.tencent.matrix.util.MatrixLog;

/**
 * Created by Yves on 2020/7/15
 */
public class JeCtl {
    private static final String TAG = "Matrix.JeCtl";

    private static boolean initialized = false;

    static {
        try {
            System.loadLibrary("matrix-jectl");
            initNative();
            initialized = true;
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
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
            MatrixLog.e(TAG, "JeCtl init failed! check if so exists");
            return ERR_INIT_FAILED;
        }
        return compactNative();
    }

    private static boolean hasAllocated;
    private static int     sLastPreAllocRet;

    public synchronized static int preAllocRetain(int size0, int size1, int limit0, int limit1) {
        if (!initialized) {
            MatrixLog.e(TAG, "JeCtl init failed! check if so exists");
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
            MatrixLog.e(TAG, "JeCtl init failed! check if so exists");
            return "VER_UNKNOWN";
        }

        return getVersionNative();
    }

    @Keep
    private static native void initNative();

    @Keep
    private static native int compactNative();

    @Keep
    private static native int preAllocRetainNative(int size0, int size1, int limit0, int limit1);

    @Keep
    private static native String getVersionNative();

    @Keep
    public static native boolean setRetain(boolean enable);
}
