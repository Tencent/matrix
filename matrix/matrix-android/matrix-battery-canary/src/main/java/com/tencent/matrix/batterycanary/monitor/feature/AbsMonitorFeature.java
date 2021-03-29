package com.tencent.matrix.batterycanary.monitor.feature;

import android.content.pm.ApplicationInfo;
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

    protected String getTag() {
        return TAG;
    }

    @SuppressWarnings("NotNullFieldNotInitialized")
    @NonNull
    protected BatteryMonitorCore mCore;

    @CallSuper
    @Override
    public void configure(BatteryMonitorCore monitor) {
        MatrixLog.i(getTag(), "#configure");
        this.mCore = monitor;
    }

    @CallSuper
    @Override
    public void onTurnOn() {
        MatrixLog.i(getTag(), "#onTurnOn");
    }

    @CallSuper
    @Override
    public void onTurnOff() {
        MatrixLog.i(getTag(), "#onTurnOff");
    }

    @CallSuper
    @Override
    public void onForeground(boolean isForeground) {
        MatrixLog.i(getTag(), "#onForeground, foreground = " + isForeground);
    }

    @CallSuper
    @WorkerThread
    @Override
    public void onBackgroundCheck(long duringMillis) {
        MatrixLog.i(getTag(), "#onBackgroundCheck, since background started millis = " + duringMillis);
    }

    protected boolean shouldTracing() {
        if (mCore.getConfig().isAggressiveMode) return true;
        return  0 != (mCore.getContext().getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE);
    }

    @Override
    public String toString() {
        return getTag();
    }
}

