package com.tencent.mm.performance.jni.memory;

import com.tencent.mm.performance.jni.LibWxPerfManager;

/**
 * Created by Yves on 2019-08-08
 */
public class MemoryHook {

    public static void hook(boolean enableStackTrace) {
        LibWxPerfManager.INSTANCE.init();

        enableStacktraceNative(enableStackTrace);
        xhookInitNative();
        xhookEnableDebugNative(true);
        xhookEnableSigSegvProtectionNative(true);
        xhookRefreshNative(false);
    }

    public static void refresh(){
        xhookRefreshNative(false);
    }

    public static void refreshAsync() {
        xhookRefreshNative(true);
    }

    public static native void dump();

    private static native void enableStacktraceNative(boolean enable);
    private static native void xhookInitNative();
    private static native void xhookClearNative();
    private static native int xhookRefreshNative(boolean async);
    private static native void xhookEnableDebugNative(boolean flag);
    private static native void xhookEnableSigSegvProtectionNative(boolean flag);
}

