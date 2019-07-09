package com.tencent.matrix.batterycanary.monitor.plugin;

import android.os.Handler;
import android.os.IBinder;
import android.os.WorkSource;

import com.tencent.matrix.batterycanary.core.PowerManagerServiceHooker;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitor;
import com.tencent.matrix.batterycanary.util.BatteryCanaryUtil;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import java.util.HashMap;
import java.util.Objects;

public class WakeLockMonitorPlugin implements IBatteryMonitorPlugin, PowerManagerServiceHooker.IListener {

    private static final String TAG = "Matrix.WakeLockMonitorPlugin";
    private Handler handler = null;
    private BatteryMonitor monitor;
    private HashMap<Object, Runnable> timeoutMap = new HashMap<>(2);

    @Override
    public void onInstall(BatteryMonitor monitor) {
        this.monitor = monitor;
        this.handler = new Handler(MatrixHandlerThread.getDefaultHandlerThread().getLooper());
    }

    @Override
    public void onTurnOn() {
        Objects.requireNonNull(monitor);
        PowerManagerServiceHooker.addListener(this);
    }

    @Override
    public void onTurnOff() {
        Objects.requireNonNull(monitor);
        PowerManagerServiceHooker.removeListener(this);
        handler.removeCallbacksAndMessages(null);
    }

    @Override
    public void onAppForeground(boolean isForeground) {

    }

    @Override
    public void onAcquireWakeLock(IBinder token, int flags, final String tag, final String packageName, WorkSource workSource, String historyTag) {
        MatrixLog.i(TAG, "[onAcquireWakeLock] token=%s flags=%s tag=%s historyTag=%s packageName=%s workSource=%s stack=%s"
                , token.hashCode(), flags, tag, historyTag, packageName, workSource, BatteryCanaryUtil.stackTraceToString(new Throwable().getStackTrace()));

        Runnable timeoutRunnable = new Runnable() {
            int warningCount = 1;

            @Override
            public void run() {
                if (monitor.getConfig().printer != null) {
                    monitor.getConfig().printer.onWakeLockTimeout(tag, packageName, warningCount);
                    warningCount++;
                    handler.postDelayed(this, monitor.getConfig().wakelockTimeout);
                }
            }
        };
        timeoutMap.put(token, timeoutRunnable);
        handler.postDelayed(timeoutRunnable, monitor.getConfig().wakelockTimeout);
    }

    @Override
    public void onReleaseWakeLock(IBinder token, int flags) {
        MatrixLog.i(TAG, "[onReleaseWakeLock] token=%s flags=%s", token.hashCode(), flags);
        handler.removeCallbacks(timeoutMap.get(token));
    }
}
