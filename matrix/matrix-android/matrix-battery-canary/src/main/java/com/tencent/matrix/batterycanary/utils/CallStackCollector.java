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
        return BatteryCanaryUtil.stackTraceToString(Looper.getMainLooper().getThread().getStackTrace());
    }

    public String collect(Throwable throwable) {
        return BatteryCanaryUtil.stackTraceToString(throwable.getStackTrace());
    }

    public String collect(Thread thread) {
        return BatteryCanaryUtil.stackTraceToString(thread.getStackTrace());
    }

    public String collect(int tid) {
        if (tid == Process.myTid()) {
            return collectCurr();
        }
        return "";
    }
}
