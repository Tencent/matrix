package com.tencent.matrix.trace.util;

public class Utils {

    public static String getStack() {
        StackTraceElement[] trace = new Throwable().getStackTrace();
        return getStack(trace);
    }

    public static String getStack(StackTraceElement[] trace) {
        return getStack(trace, "");
    }

    public static String getStack(StackTraceElement[] trace, String preFixStr) {
        if ((trace == null) || (trace.length < 3)) {
            return "";
        }
        StringBuilder t = new StringBuilder(" \n");
        for (int i = 3; i < trace.length - 3; i++) {
            t.append(preFixStr);
            t.append("at ");
            t.append(trace[i].getClassName());
            t.append(":");
            t.append(trace[i].getMethodName());
            t.append("(" + trace[i].getLineNumber() + ")");
            t.append("\n");

        }
        return t.toString();
    }

    public static String calculateCpuUsage(long threadMs, long ms) {
        if (threadMs <= 0) {
            return ms > 1000 ? "0%" : "100%";
        }

        if (threadMs >= ms) {
            return "100%";
        }

        return String.format("%.2f", 1.f * threadMs / ms * 100) + "%";
    }

    public static boolean isEmpty(String str) {
        return null == str || str.equals("");
    }


}
