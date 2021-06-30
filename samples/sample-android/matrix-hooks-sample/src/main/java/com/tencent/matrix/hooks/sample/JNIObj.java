package com.tencent.matrix.hooks.sample;

/**
 * Created by Yves on 2019-08-08
 */
public class JNIObj {

    private static final String TAG = "Matrix.test.JNIObj";

    static {
        init();
    }

    public static void init() {
        try {
            System.loadLibrary("native-lib");
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public native void reallocTest();

    public native void doMmap();

    public native static void mallocTest();

    public native static void threadTest();
}
