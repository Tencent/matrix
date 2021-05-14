package com.tencent.matrix.hook.egl;

public class EglResourceMonitor {

    public long resourceId;


    public EglResourceMonitor(long contextId) {
        this.resourceId = contextId;
    }

}
