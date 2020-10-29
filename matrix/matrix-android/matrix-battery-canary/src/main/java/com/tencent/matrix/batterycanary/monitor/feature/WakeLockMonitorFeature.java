package com.tencent.matrix.batterycanary.monitor.feature;

import android.os.Handler;
import android.os.IBinder;
import android.os.SystemClock;
import android.os.WorkSource;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.PowerManagerServiceHooker;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;

@SuppressWarnings("NotNullFieldNotInitialized")
public class WakeLockMonitorFeature implements MonitorFeature, PowerManagerServiceHooker.IListener {
    private static final String TAG = "Matrix.monitor.WakeLockMonitorFeature";

    public interface WakeLockListener {
        void onWakeLockTimeout(String tag, String packageName, int warningCount);
    }

    @NonNull private BatteryMonitorCore monitor;
    @NonNull private Handler handler;
    long wakeLockedTime = 0L;
    int wakeLockRequiredCount = 0;
    static long OVER_TIME;
    final ConcurrentHashMap<IBinder, WakeLockTrace> overTimeWakeLocks = new ConcurrentHashMap<>(2);

    private WakeLockListener getListener() {
        return monitor;
    }

    @Override
    public void configure(BatteryMonitorCore monitor) {
        MatrixLog.i(TAG, "#configure monitor feature");
        this.monitor = monitor;
        OVER_TIME = monitor.getConfig().wakelockTimeout;
        handler = new Handler(MatrixHandlerThread.getDefaultHandlerThread().getLooper());
    }

    @Override
    public void onTurnOn() {
        MatrixLog.i(TAG, "#onTurnOn");
        PowerManagerServiceHooker.addListener(this);
    }

    @Override
    public void onTurnOff() {
        MatrixLog.i(TAG, "#onTurnOff");
        PowerManagerServiceHooker.removeListener(this);
        handler.removeCallbacksAndMessages(null);
    }

    @Override
    public void onForeground(boolean isForeground) {}

    @Override
    public int weight() {
        return Integer.MIN_VALUE;
    }

    @Override
    public void onAcquireWakeLock(IBinder token, int flags, final String tag, final String packageName, WorkSource workSource, String historyTag) {
        MatrixLog.i(TAG, "[onAcquireWakeLock] token=%s flags=%s tag=%s historyTag=%s packageName=%s workSource=%s stack=%s",
                String.valueOf(token), flags, tag, historyTag, packageName, workSource, BatteryCanaryUtil.stackTraceToString(new Throwable().getStackTrace()));

        Runnable timeoutLoopCheckTask = new Runnable() {
            int warningCount = 1;
            @Override
            public void run() {
                getListener().onWakeLockTimeout(tag, packageName, warningCount);
                warningCount++;
                handler.postDelayed(this, OVER_TIME);
            }
        };

        overTimeWakeLocks.put(token, new WakeLockTrace(token, tag, packageName, timeoutLoopCheckTask));
        handler.postDelayed(timeoutLoopCheckTask, OVER_TIME);
        wakeLockRequiredCount++;
    }

    @Override
    public void onReleaseWakeLock(IBinder token, int flags) {
        MatrixLog.i(TAG, "[onReleaseWakeLock] token=%s flags=%s", token.hashCode(), flags);
        WakeLockTrace wakeLockTrace = overTimeWakeLocks.get(token);
        if (wakeLockTrace != null) {
            handler.removeCallbacks(wakeLockTrace.runnable);
            wakeLockTrace.finish();
            wakeLockedTime += (wakeLockTrace.timeEnd - wakeLockTrace.timeBgn);
        } else {
            MatrixLog.i(TAG, "[onReleaseWakeLock] missing tracking, token = " + token);
        }
    }

    public WakeLockInfo currentWakeLocks() {
        long totalWakeLockTime = wakeLockedTime;
        long uptimeMillis = SystemClock.uptimeMillis();

        List<WakeLockTrace> currentOverTimeWakeLocks = new LinkedList<>();
        for (WakeLockTrace item : currentOverTimeWakeLocks) {
            if (!item.isFinished()) {
                currentOverTimeWakeLocks.add(item);
                totalWakeLockTime += (uptimeMillis - item.timeBgn);
            }
        }
        WakeLockInfo info = new WakeLockInfo();
        info.totalWakeLockTime = totalWakeLockTime;
        info.totalWakeLockCount = wakeLockRequiredCount;
        info.currentOverTimeWakeLocks = currentOverTimeWakeLocks;
        return info;
    }

    public static class WakeLockInfo {
        public long totalWakeLockTime;
        public int totalWakeLockCount;
        List<WakeLockTrace> currentOverTimeWakeLocks = Collections.emptyList();
    }

    public static class WakeLockTrace {
        final IBinder token;
        final Runnable runnable;
        final String tag;
        final String packageName;
        final long timeBgn;
        long timeEnd = 0;

        WakeLockTrace(IBinder token, String tag, String packageName, Runnable runnable) {
            this.token = token;
            this.runnable = runnable;
            this.tag = tag;
            this.packageName = packageName;
            this.timeBgn = SystemClock.uptimeMillis();
        }

        @Override
        public int hashCode() {
            return token.hashCode();
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (!(obj instanceof WakeLockTrace)) return false;
            return token.equals(obj);
        }

        public void finish() {
            timeEnd = SystemClock.uptimeMillis();
        }

        public boolean isFinished() {
            return timeEnd >= timeBgn;
        }
    }
}
