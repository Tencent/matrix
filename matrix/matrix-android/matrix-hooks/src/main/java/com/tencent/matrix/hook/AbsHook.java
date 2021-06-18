package com.tencent.matrix.hook;

import androidx.annotation.Nullable;

/**
 * Created by Yves on 2020-03-18
 */
public abstract class AbsHook {
    @Nullable
    protected abstract String getNativeLibraryName();

    protected abstract void onConfigure();

    protected abstract void onHook(boolean enableDebug);
}
