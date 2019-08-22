package com.tencent.mm.libwxperf;

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

    public native void doSomeThing();
    public native void dump(String libPath);

    public static void onDumpFinished(String res) {

    }
}
