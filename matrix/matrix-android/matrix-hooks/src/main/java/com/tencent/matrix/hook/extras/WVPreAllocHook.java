package com.tencent.matrix.hook.extras;

import android.os.Build;

import androidx.annotation.Nullable;

import com.tencent.matrix.hook.AbsHook;

public class WVPreAllocHook extends AbsHook {
    public static final WVPreAllocHook INSTANCE = new WVPreAllocHook();

    @Nullable
    @Override
    protected String getNativeLibraryName() {
        return "matrix-extrashook";
    }

    @Override
    protected void onConfigure() {
        // Ignored.
    }

    @Override
    protected void onHook(boolean enableDebug) {
        installHooksNative(Build.VERSION.SDK_INT, this.getClass().getClassLoader(), enableDebug);
    }

    private native void installHooksNative(int sdkVer, ClassLoader classLoader, boolean enableDebug);
}
