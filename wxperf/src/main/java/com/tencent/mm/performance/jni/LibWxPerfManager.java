package com.tencent.mm.performance.jni;

import com.tencent.stubs.logger.Log;

/**
 * Created by Yves on 2019-08-20
 */
/* package */ class LibWxPerfManager {

    private static final String TAG = "LibWxPerfManager";

    public static final LibWxPerfManager INSTANCE = new LibWxPerfManager();

    private volatile boolean isLibLoaded = false;

    public void init() {
        try {
            if (!isLibLoaded) {
                System.loadLibrary("wxperf");
                isLibLoaded = true;
            }
        } catch (Throwable e) {
            isLibLoaded = false;
            Log.printStack(Log.ERROR, TAG, e);
        }
    }

    public boolean initOk() {
        return isLibLoaded;
    }
}
