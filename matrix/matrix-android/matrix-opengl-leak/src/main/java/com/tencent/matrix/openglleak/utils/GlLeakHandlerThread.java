package com.tencent.matrix.openglleak.utils;

import android.os.HandlerThread;

public class GlLeakHandlerThread extends HandlerThread {

    private static GlLeakHandlerThread mInstance = new GlLeakHandlerThread("GpuResLeakMonitor");

    private GlLeakHandlerThread(String name) {
        super(name);
        start();
    }

    public static GlLeakHandlerThread getInstance() {
        return mInstance;
    }
}
