package com.tencent.mm.performance.jni.fd;

import com.tencent.mm.performance.jni.LibWxPerfManager;

/**
 * Created by Yves on 2019-07-22
 */
public class FDDumpBridge {

    public static String getFdPathName(String path) {
        LibWxPerfManager.INSTANCE.init();
        return getFdPathNameNative(path);
    }

    public static native String getFdPathNameNative(String path);
}
