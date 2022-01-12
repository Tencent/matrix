package com.tencent.matrix.openglleak.utils;

import android.os.Handler;

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
