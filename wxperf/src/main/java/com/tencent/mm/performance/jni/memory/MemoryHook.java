package com.tencent.mm.performance.jni.memory;

import android.os.Build;

import com.tencent.mm.performance.jni.BuildConfig;
import com.tencent.mm.performance.jni.LibWxPerfManager;

/**
 * Created by Yves on 2019-08-08
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

        if (hookSoList != null && hookSoList.length > 0) {
            xhookRegisterNative(hookSoList);
        }

        if (ignoreSoList != null && ignoreSoList.length > 0) {
            xhookIgnoreNative(ignoreSoList);
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

    public static void refresh() {
        xhookRefreshNative(false);
    }

    public static void refreshAsync() {
        xhookRefreshNative(true);
    }

    public static native void xhookRegisterNative(String[] hookSoList);

    public static native void xhookIgnoreNative(String[] ignoreSoList);

    public static native void dump();

    private static native void enableStacktraceNative(boolean enable);

    private static native void xhookInitNative();

    private static native void xhookClearNative();

    private static native int xhookRefreshNative(boolean async);

    private static native void xhookEnableDebugNative(boolean flag);

    private static native void xhookEnableSigSegvProtectionNative(boolean flag);
}

