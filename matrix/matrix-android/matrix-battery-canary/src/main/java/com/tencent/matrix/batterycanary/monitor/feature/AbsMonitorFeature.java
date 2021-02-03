package com.tencent.matrix.batterycanary.monitor.feature;

import android.support.annotation.CallSuper;
import android.support.annotation.NonNull;
import android.support.annotation.WorkerThread;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.util.MatrixLog;

/**
 * @author Kaede
 * @since 2020/12/24
 */
public abstract class AbsMonitorFeature implements MonitorFeature {
    private static final String TAG = "Matrix.battery.MonitorFeature";

    @SuppressWarnings("NotNullFieldNotInitialized")
    @NonNull
    protected BatteryMonitorCore mCore;

    @CallSuper
    @Override
    public void configure(BatteryMonitorCore monitor) {
        MatrixLog.i(TAG, "#configure");
        this.mCore = monitor;
    }

    @CallSuper
    @Override
    public void onTurnOn() {
        MatrixLog.i(TAG, "#onTurnOn");
    }

    @CallSuper
    @Override
    public void onTurnOff() {
        MatrixLog.i(TAG, "#onTurnOff");
    }

    @CallSuper
    @Override
    public void onForeground(boolean isForeground) {
        MatrixLog.i(TAG, "#onForeground, foreground = " + isForeground);
    }

    @CallSuper
    @WorkerThread
    @Override
    public void onBackgroundCheck(long duringMillis) {
        MatrixLog.i(TAG, "#onBackgroundCheck, since background started millis = " + duringMillis);
    }
}

