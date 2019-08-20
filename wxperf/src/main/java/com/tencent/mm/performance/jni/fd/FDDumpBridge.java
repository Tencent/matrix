package com.tencent.mm.performance.jni.fd;

import com.tencent.mm.performance.jni.LibWxPerfManager;

/**
 * Created by Yves on 2019-07-22
 */
public class FDDumpBridge {

    static {
        LibWxPerfManager.INSTANCE.init();
    }

    public static String getFdPathName(String path) {
        if (!LibWxPerfManager.INSTANCE.initOk()) {
            return null;
        }
        return getFdPathNameNative(path);
    }

    private static native String getFdPathNameNative(String path);
}
