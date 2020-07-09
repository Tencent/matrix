package com.tencent.mm.performance.jni.egl;

public class EglResourceMonitor {

    public long resourceId;


    public EglResourceMonitor(long contextId) {
        this.resourceId = contextId;
    }

}
