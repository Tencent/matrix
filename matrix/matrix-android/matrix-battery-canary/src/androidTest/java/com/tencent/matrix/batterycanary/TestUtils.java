package com.tencent.matrix.batterycanary;

import androidx.test.platform.app.InstrumentationRegistry;

/**
 * @author Kaede
 * @since 2020/11/16
 */
public class TestUtils {
    public static boolean isAssembleTest() {
        StackTraceElement[] ste = Thread.currentThread().getStackTrace();
        String outerMethodName = ste[2 + 1].getMethodName();
        return !String.valueOf(InstrumentationRegistry.getArguments().get("class")).endsWith("#" + outerMethodName);
    }
}
