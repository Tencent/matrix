package com.tencent.mm.performance.jni;

import com.tencent.mm.performance.jni.LibWxPerfManager;
import com.tencent.stubs.logger.Log;

/**
 * Created by Yves on 2019-07-22
 */
public class FDDumpBridge {

    private static final String TAG = "FDDumpBridge";

    static {
        LibWxPerfManager.INSTANCE.init();
    }

    public static String getFdPathName(String path) {
        if (!LibWxPerfManager.INSTANCE.initOk()) {
            Log.e(TAG, "wxperf init NOT ok");
            return null;
        }
        return getFdPathNameNative(path);
    }

    private static native String getFdPathNameNative(String path);
}
