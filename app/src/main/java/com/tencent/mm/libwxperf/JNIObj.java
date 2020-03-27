package com.tencent.mm.libwxperf;

import android.util.Log;

/**
 * Created by Yves on 2019-08-08
 */
public class JNIObj {
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

    public native void doSomeThing();

    public native void doMmap();

    public native void dump(String libPath);

    public native void nullptr(String[] ss);

    public static void onDumpFinished(String res) {

    }

    public native static void testThread();

    public native static void testThreadSpecific();

    public native static void testJNICall();

    public static void calledByJNI() {
        Log.d("Yves-debug", "called By JNI");
        try {
            Thread.sleep(1);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }
}
