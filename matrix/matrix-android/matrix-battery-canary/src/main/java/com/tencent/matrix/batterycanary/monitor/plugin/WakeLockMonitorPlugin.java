package com.tencent.matrix.batterycanary.monitor.plugin;

import android.os.Handler;
import android.os.IBinder;
import android.os.SystemClock;
import android.os.WorkSource;
import android.support.annotation.Nullable;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.utils.PowerManagerServiceHooker;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import java.util.List;
import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;

public class WakeLockMonitorPlugin implements IBatteryMonitorPlugin, PowerManagerServiceHooker.IListener {
    private static final String TAG = "Matrix.WakeLockMonitorPlugin";

    public interface WakeLockListener {
        void onWakeLockTimeout(String tag, String packageName, int warningCount);
    }

    private Handler handler = null;
    private BatteryMonitorCore monitor;
    private ConcurrentHashMap<Object, Cache> timeoutMap = new ConcurrentHashMap<>(2);
    private long wakeLockTime = 0L;
    private int wakeLockingCount = 0;
    private int wakeLockCount = 0;

    private WakeLockListener getListener() {
        return monitor;
    }

    @Override
    public void onInstall(BatteryMonitorCore monitor) {
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
    public int weight() {
        return Integer.MIN_VALUE;
    }

    @Override
    public void onAcquireWakeLock(IBinder token, int flags, final String tag, final String packageName, WorkSource workSource, String historyTag) {
        MatrixLog.i(TAG, "[onAcquireWakeLock] token=%s flags=%s tag=%s historyTag=%s packageName=%s workSource=%s stack=%s"
                , token.hashCode(), flags, tag, historyTag, packageName, workSource, BatteryCanaryUtil.stackTraceToString(new Throwable().getStackTrace()));

        Runnable timeoutRunnable = new Runnable() {
            int warningCount = 1;

            @Override
            public void run() {
                if (getListener() != null) {
                    getListener().onWakeLockTimeout(tag, packageName, warningCount);
                }
                warningCount++;
                handler.postDelayed(this, monitor.getConfig().wakelockTimeout);
            }
        };
        timeoutMap.put(token, new Cache(token, SystemClock.uptimeMillis(), timeoutRunnable));
        handler.postDelayed(timeoutRunnable, monitor.getConfig().wakelockTimeout);
        wakeLockingCount += 1;
        wakeLockCount++;
    }

    @Override
    public void onReleaseWakeLock(IBinder token, int flags) {
        MatrixLog.i(TAG, "[onReleaseWakeLock] token=%s flags=%s", token.hashCode(), flags);
        Cache cache = timeoutMap.get(token);
        handler.removeCallbacks(cache.runnable);
        wakeLockTime += (SystemClock.uptimeMillis() - cache.time);
        wakeLockingCount -= 1;
    }

    public Info getInfo() {
        long time = wakeLockTime;
        if (wakeLockingCount > 0) {
            for (Cache cache : timeoutMap.values()) {
                time += (SystemClock.uptimeMillis() - cache.time);
            }
        }
        return new Info(time, wakeLockingCount, wakeLockCount);
    }

    public static class Info {

        public Info(long wakeLockTime, int wakeLockingCount, int wakeLockCount) {
            this.wakeLockTime = wakeLockTime;
            this.wakeLockingCount = wakeLockingCount;
            this.wakeLockCount = wakeLockCount;
        }

        public long wakeLockTime;
        public int wakeLockingCount;
        public int wakeLockCount;
    }


    public static class Cache {
        IBinder token;
        long time;
        Runnable runnable;

        Cache(IBinder token, long time, Runnable runnable) {
            this.token = token;
            this.time = time;
            this.runnable = runnable;
        }

        @Override
        public int hashCode() {
            return token.hashCode();
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            return token.equals(obj);
        }
    }
}
