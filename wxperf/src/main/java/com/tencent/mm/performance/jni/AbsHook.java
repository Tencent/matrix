package com.tencent.mm.performance.jni;

/**
 * Created by Yves on 2020-03-18
 */
public abstract class AbsHook {
    protected abstract void onConfigure();
    protected abstract void onHook();
}
