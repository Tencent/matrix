package com.tencent.mm.performance.jni;

/**
 * Created by Yves on 2019-08-08
 */
public class LibWxPerfManager {

    public static final LibWxPerfManager INSTANCE = new LibWxPerfManager();

    private volatile boolean isLibLoaded = false;

    public void init() {
        try {
            if (!isLibLoaded) {
                System.loadLibrary("wxperf");
                isLibLoaded = true;
            }
        } catch (Exception e) {
            isLibLoaded = false;
        }
    }
}
