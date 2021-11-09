package com.tencent.matrix.benchmark.test;

public class StacktraceHelper {

    public static String[] getStackTraces() {
        StackTraceElement[] elements = Thread.currentThread().getStackTrace();
        String[] traces = new String[elements.length];

        for (int i = 0; i < elements.length; i++) {
            traces[i] = elements[i].toString();
        }
        return traces;
    }

}
