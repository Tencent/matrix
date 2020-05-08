package com.tencent.mm.performance.jni.egl;

import android.opengl.EGL14;
import android.support.annotation.Nullable;

public class EglContextMonitor {

    private long contextId;

    private String javaStack;
    private long nativeStackHash;

    public EglContextMonitor(long contextId, String javaStack, long nativeStackHash) {
        this.contextId = contextId;
        this.javaStack = javaStack;
        this.nativeStackHash = nativeStackHash;
    }

    @Override
    public boolean equals(Object obj) {
        if (null == obj) {
            return false;
        }

        EglContextMonitor compare = (EglContextMonitor) obj;

        if (compare.nativeStackHash == this.nativeStackHash && compare.javaStack.equals(this.javaStack)) {
            return true;
        }

        return false;
    }
}
