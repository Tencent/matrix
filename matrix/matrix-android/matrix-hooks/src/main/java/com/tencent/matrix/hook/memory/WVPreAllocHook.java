package com.tencent.matrix.hook.memory;

import android.os.Build;

import androidx.annotation.Nullable;

import com.tencent.matrix.hook.AbsHook;

public class WVPreAllocHook extends AbsHook {
    public static final WVPreAllocHook INSTANCE = new WVPreAllocHook();

    @Nullable
    @Override
    protected String getNativeLibraryName() {
        return "matrix-memoryhook";
    }

    @Override
    protected boolean onConfigure() {
        // Ignored.
        return true;
    }

    @Override
    protected boolean onHook(boolean enableDebug) {
        return installHooksNative(Build.VERSION.SDK_INT, this.getClass().getClassLoader(), enableDebug);
    }

    private native boolean installHooksNative(int sdkVer, ClassLoader classLoader, boolean enableDebug);
}
