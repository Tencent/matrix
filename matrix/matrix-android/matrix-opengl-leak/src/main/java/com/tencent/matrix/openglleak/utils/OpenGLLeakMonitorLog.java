package com.tencent.matrix.openglleak.utils;

public class OpenGLLeakMonitorLog {

    private static LogStub stub = null;

    public static void setLogStub(LogStub ls) {
        stub = ls;
    }

    public static void i(String tag, String msg) {
        if (stub != null) {
            stub.i(tag, msg);
        } else {
            android.util.Log.i(tag, msg);
        }
    }

    public static void d(String tag, String msg) {
        if (stub != null) {
            stub.d(tag, msg);
        } else {
            android.util.Log.d(tag, msg);
        }
    }

    public static void e(String tag, String msg) {
        if (stub != null) {
            stub.e(tag, msg);
        } else {
            android.util.Log.e(tag, msg);
        }
    }

    public static void w(String tag, String msg) {
        if (stub != null) {
            stub.w(tag, msg);
        } else {
            android.util.Log.w(tag, msg);
        }
    }

    public interface LogStub {
        void i(String tag, String msg);

        void w(String tag, String msg);

        void d(String tag, String msg);

        void e(String tag, String msg);
    }
}
