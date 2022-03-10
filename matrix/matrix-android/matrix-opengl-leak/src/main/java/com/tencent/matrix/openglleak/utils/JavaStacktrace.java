package com.tencent.matrix.openglleak.utils;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public class JavaStacktrace {

    private static final Map<Integer, Throwable> sThrowableMap = new ConcurrentHashMap<>();

    private static final Map<String, Trace> sString2Trace = new ConcurrentHashMap<>();

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

    public static Trace getBacktraceValue(int key) {
        Throwable throwable = sThrowableMap.get(key);
        if (throwable == null) {
            return new Trace();
        }
        String traceKey = stackTraceToString(throwable.getStackTrace());
        Trace mapTrace = sString2Trace.get(traceKey);
        if (mapTrace == null) {
            Trace resultTrace = new Trace(traceKey);
            resultTrace.addReference();
            sString2Trace.put(traceKey, resultTrace);
            sThrowableMap.remove(key);
            return resultTrace;
        } else {
            sThrowableMap.remove(key);
            return mapTrace;
        }
    }

    public static int getCollision() {
        return sCollision;
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

    public static class Trace {

        private final String content;

        private int refCount = 0;

        public Trace() {
            content = "";
        }

        public Trace(String content) {
            this.content = content;
        }

        public String getContent() {
            return content;
        }

        public void addReference() {
            refCount++;
        }

        public void reduceReference() {
            refCount--;
            if (refCount == 0) {
                sString2Trace.remove(this.content);
            }
        }

    }


}
