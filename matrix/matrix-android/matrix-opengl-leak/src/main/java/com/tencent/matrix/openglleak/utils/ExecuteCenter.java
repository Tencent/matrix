package com.tencent.matrix.openglleak.utils;

import android.opengl.GLES20;
import android.opengl.GLES30;
import android.os.Handler;
import android.os.HandlerThread;

public class ExecuteCenter {

    private static final ExecuteCenter mInstance = new ExecuteCenter();

    private final Handler mHandler;

    public static ExecuteCenter getInstance() {
        return mInstance;
    }

    private ExecuteCenter() {
        mHandler = new Handler(GlLeakHandlerThread.getInstance().getLooper());
    }

    public void post(Runnable runnable) {
        mHandler.post(runnable);
    }

    public void postDelay(Runnable runnable, long million) {
        mHandler.postDelayed(runnable, million);
    }

}
