package com.tencent.wxperf.sample;

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

    public native static void testThread();

    public native static void testThreadSpecific();

    public native static void testJNICall();

    public native static void testPthreadFree();

    public native static void mallocTest();

    public native static void tlsTest();

    public native static void poolTest();

    public native static void epollTest();

    public native static void concurrentMapTest();

    public static String calledByJNI() {
        Log.d("Yves-debug", "called By JNI");
        return stackTraceToString(new Throwable().getStackTrace());
    }

    private static String stackTraceToString(final StackTraceElement[] arr) {
        Log.d("Yves-debug", "java: stackTraceToString");
        if (arr == null) {
            return "";
        }

        StringBuilder sb = new StringBuilder();

        for (StackTraceElement stackTraceElement : arr) {
            String className = stackTraceElement.getClassName();
            // remove unused stacks
            if (className.contains("java.lang.Thread")) {
                continue;
            }

            sb.append("\t").append(stackTraceElement).append('\n');
        }
        return sb.toString();
    }
}
