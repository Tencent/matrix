package com.tencent.matrix.openglleak.utils;

import com.tencent.matrix.util.MatrixLog;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public class JavaStacktrace {

    private static final Map<Integer, Throwable> sThrowableMap = new ConcurrentHashMap<>();

    private static int sCollision = 0;

    private JavaStacktrace() {
    }

    public static int getBacktraceKey() {
        Throwable throwable = new Throwable();
        int key = throwable.hashCode();
        if (sThrowableMap.get(key) != null) {
            sCollision++;
        }
        sThrowableMap.put(key, throwable);
        return key;
    }

    public static String getBacktraceValue(int key) {
        Throwable throwable = sThrowableMap.get(key);
        if (throwable == null) {
            MatrixLog.e("Backtrace", "getBacktraceValue key = " + key);
            return "";
        }
        return stackTraceToString(throwable.getStackTrace());
    }

    public static int getCollision() {
        return sCollision;
    }

    public static void removeBacktraceKey(int key) {
        sThrowableMap.remove(key);
    }

    private static String stackTraceToString(StackTraceElement[] arr) {
        StringBuilder sb = new StringBuilder();

        for (int i = 0; i < arr.length; i++) {
            if (i == 0) {
                continue;
            }
            StackTraceElement element = arr[i];
            String className = element.getClassName();
            if (className.contains("java.lang.Thread")) {
                continue;
            }
            sb.append("\t").append(element).append('\n');
        }
        return sb.toString();
    }


}
