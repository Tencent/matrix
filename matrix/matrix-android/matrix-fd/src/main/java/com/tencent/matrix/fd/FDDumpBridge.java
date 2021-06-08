package com.tencent.matrix.fd;


import androidx.annotation.Keep;

import com.tencent.matrix.util.MatrixLog;

/**
 * Created by Yves on 2019-07-22
 */
public class FDDumpBridge {

    private static final String TAG = "FDDumpBridge";

    private static boolean initialized;

    static {
        try {
            System.loadLibrary("matrix-fd");
            initialized = true;
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
            initialized = false;
        }
    }

    public static String getFdPathName(String path) {
        if (!initialized) {
            return path;
        }
        return getFdPathNameNative(path);
    }

    @Keep
    public static native String getFdPathNameNative(String path);

    @Keep
    public static native int getFDLimit();
}
