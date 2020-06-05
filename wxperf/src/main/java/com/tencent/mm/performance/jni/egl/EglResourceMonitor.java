package com.tencent.mm.performance.jni.egl;

public class EglResourceMonitor {

    public long resourceId;

    public String javaStack;
    public long nativeStackHash;

    public EglResourceMonitor(long contextId, String javaStack, long nativeStackHash) {
        this.resourceId = contextId;
        this.javaStack = javaStack;
        this.nativeStackHash = nativeStackHash;
    }

    @Override
    public boolean equals(Object obj) {
        if (null == obj) {
            return false;
        }

        EglResourceMonitor compare = (EglResourceMonitor) obj;

        if (compare.nativeStackHash == this.nativeStackHash && compare.javaStack.equals(this.javaStack)) {
            return true;
        }

        return false;
    }
}
