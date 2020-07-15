package com.tencent.wxperf.jni.egl;

public class EglResourceMonitor {

    public long resourceId;


    public EglResourceMonitor(long contextId) {
        this.resourceId = contextId;
    }

}
