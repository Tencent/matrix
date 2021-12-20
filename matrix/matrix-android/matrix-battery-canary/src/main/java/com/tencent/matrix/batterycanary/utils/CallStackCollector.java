package com.tencent.matrix.batterycanary.utils;

import android.os.Looper;
import android.os.Process;

/**
 * @author Kaede
 * @since 2021/12/17
 */
public class CallStackCollector {

    public String collectCurr() {
        return collect(new Throwable());
    }

    public String collectUiThread() {
        return collect(Looper.getMainLooper().getThread());
    }

    public String collect(Throwable throwable) {
        return collect(throwable.getStackTrace());
    }

    public String collect(Thread thread) {
        return collect(thread.getStackTrace());
    }

    public String collect(StackTraceElement[] elements) {
        return BatteryCanaryUtil.stackTraceToString(elements);
    }

    public String collect(int tid) {
        if (tid == Process.myTid()) {
            return collectCurr();
        }
        return "";
    }
}
