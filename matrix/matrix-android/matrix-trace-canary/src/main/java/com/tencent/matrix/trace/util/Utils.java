package com.tencent.matrix.trace.util;

public class Utils {

    public static String getStack() {
        StackTraceElement[] trace = new Throwable().getStackTrace();
        if ((trace == null) || (trace.length < 3)) {
            return "";
        }
        StringBuilder t = new StringBuilder(" \nStack:\n");
        t.append(" \n");
        for (int i = 2; i < trace.length; i++) {
            if (trace[i].getClassName().startsWith("android.")) {
                continue;
            }
            if (trace[i].getClassName().startsWith("com.android.")) {
                continue;
            }
            if (trace[i].getClassName().startsWith("java.")) {
                continue;
            }
            t.append("at ");
            t.append(trace[i].getClassName());
            t.append(":");
            t.append(trace[i].getMethodName());
            t.append("(" + trace[i].getLineNumber() + ")");
            t.append("\n");

        }
        return t.toString();
    }

}
