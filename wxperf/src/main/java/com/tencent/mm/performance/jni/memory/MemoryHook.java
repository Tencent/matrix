package com.tencent.mm.performance.jni.memory;

import com.tencent.mm.performance.jni.BuildConfig;
import com.tencent.mm.performance.jni.LibWxPerfManager;

/**
 * Created by Yves on 2019-08-08
 * fixme 接口重构
 */
public class MemoryHook {

//    public static void register(String[] regex) {
//        xhookRegisterNative(regex);
//    }
//
//    public static void ignore(String[] regex) {
//        xhookIgnoreNative(regex);
//    }

    static {
        LibWxPerfManager.INSTANCE.init();
    }

    public static void hook(String[] hookSoList, String[] ignoreSoList) {
        hook(hookSoList, ignoreSoList, 0);
    }

    public static void hook(String[] hookSoList, String[] ignoreSoList, int traceThreshold) {
        hook(hookSoList, ignoreSoList, traceThreshold, 0, 1);
    }

    /**
     * @param hookSoList
     * @param ignoreSoList
     * @param minSize      >= 0, 0 表示不限制
     * @param maxSize      0 或 > minSize, 0 表示不限制
     * @param sampling     [0,1]
     */
    public static void hook(String[] hookSoList, String[] ignoreSoList, int minSize, int maxSize, double sampling) {
        if (hookSoList != null && hookSoList.length > 0) {
            xhookRegisterNative(hookSoList);
        }

        if (ignoreSoList != null && ignoreSoList.length > 0) {
            xhookIgnoreNative(ignoreSoList);
        }

        if (minSize < 0 || (maxSize != 0 && maxSize < minSize)) {
            throw new IllegalArgumentException("sizes should not be negative and maxSize should be 0 or greater than minSize");
        }

        if (minSize > 0 && (maxSize == 0 || maxSize >= minSize)) {
            setSampleSizeRangeNative(minSize, maxSize);
        }

        if (0 <= sampling && sampling <= 1) {
            setSamplingNative(sampling);
        }

        enableStacktraceNative(true);
        xhookEnableDebugNative(true);
        xhookEnableSigSegvProtectionNative(!BuildConfig.DEBUG);
        xhookRefreshNative(false);
    }

    public static void hook() {

        enableStacktraceNative(false);
        xhookInitNative();
        xhookEnableDebugNative(true);
        xhookEnableSigSegvProtectionNative(!BuildConfig.DEBUG);
        xhookRefreshNative(false);
    }

    @Deprecated
    public static void hook(String[] hookSoList) {
        if (hookSoList != null && hookSoList.length > 0) {
            xhookRegisterNative(hookSoList);
        }

        xhookIgnoreNative(null);
        enableStacktraceNative(false);
        xhookEnableDebugNative(true);
        xhookEnableSigSegvProtectionNative(!BuildConfig.DEBUG);
        xhookRefreshNative(false);
    }

    public static void refresh() {
        xhookRefreshNative(false);
    }

    public static void refreshAsync() {
        xhookRefreshNative(true);
    }

    public static native void dump();

    public static native void groupByMemorySize(boolean enable);

    private static native void xhookIgnoreNative(String[] ignoreSoList);

//    private static native void setTraceSizeThresholdNative(int threshold);

    private static native void setSamplingNative(double sampling);

    private static native void setSampleSizeRangeNative(int minSize, int maxSize);

    private static native void xhookRegisterNative(String[] hookSoList);

    private static native void enableStacktraceNative(boolean enable);

    private static native void xhookInitNative();

    private static native void xhookClearNative();

    private static native int xhookRefreshNative(boolean async);

    private static native void xhookEnableDebugNative(boolean flag);

    private static native void xhookEnableSigSegvProtectionNative(boolean flag);
}

