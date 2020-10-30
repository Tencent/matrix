package com.tencent.matrix.batterycanary.monitor.feature;

import android.os.Handler;
import android.os.IBinder;
import android.os.SystemClock;
import android.os.WorkSource;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.PowerManagerServiceHooker;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

@SuppressWarnings("NotNullFieldNotInitialized")
public class WakeLockMonitorFeature implements MonitorFeature, PowerManagerServiceHooker.IListener {
    private static final String TAG = "Matrix.monitor.WakeLockMonitorFeature";

    public interface WakeLockListener {
        void onWakeLockTimeout(int warningCount, WakeLockTrace.WakeLockRecord record);
    }

    @NonNull
    private BatteryMonitorCore monitor;
    @NonNull
    @VisibleForTesting
    Handler handler;
    @VisibleForTesting
    long mOverTimeMillis;
    final Map<IBinder, WakeLockTrace> mWorkingWakeLocks = new ConcurrentHashMap<>(2);
    final List<WakeLockTrace.WakeLockRecord> mFinishedWakeLockRecords = new ArrayList<>();

    private WakeLockListener getListener() {
        return monitor;
    }

    @Override
    public void configure(BatteryMonitorCore monitor) {
        MatrixLog.i(TAG, "#configure monitor feature");
        this.monitor = monitor;
        mOverTimeMillis = monitor.getConfig().wakelockTimeout;
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
        mWorkingWakeLocks.clear();
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

        WakeLockTrace wakeLockTrace = new WakeLockTrace(token, tag, flags, packageName);
        wakeLockTrace.setListener(new WakeLockTrace.OverTimeListener() {
            @Override
            public void onWakeLockOvertime(int warningCount, WakeLockTrace.WakeLockRecord record) {
                getListener().onWakeLockTimeout(warningCount, record);
            }
        });
        wakeLockTrace.start(handler, mOverTimeMillis);
        mWorkingWakeLocks.put(token, wakeLockTrace);
    }

    @Override
    public void onReleaseWakeLock(IBinder token, int flags) {
        MatrixLog.i(TAG, "[onReleaseWakeLock] token=%s flags=%s", token.hashCode(), flags);
        Iterator<Map.Entry<IBinder, WakeLockTrace>> iterator = mWorkingWakeLocks.entrySet().iterator();
        WakeLockTrace wakeLockTrace = null;
        while (iterator.hasNext()) {
            Map.Entry<IBinder, WakeLockTrace> next = iterator.next();
            if (next.getKey() == token) {
                wakeLockTrace = next.getValue();
                iterator.remove();
                break;
            }
        }
        if (wakeLockTrace != null) {
            wakeLockTrace.finish(handler);
            synchronized (mFinishedWakeLockRecords) {
                mFinishedWakeLockRecords.add(wakeLockTrace.record);
            }
        } else {
            MatrixLog.i(TAG, "[onReleaseWakeLock] missing tracking, token = " + token);
        }
    }

    public WakeLockInfo currentWakeLocksInfo() {
        List<WakeLockTrace.WakeLockRecord> totalWakeLocks;
        long totalWakeLockTime = 0L;
        synchronized (mFinishedWakeLockRecords) {
            for (WakeLockTrace.WakeLockRecord item : mFinishedWakeLockRecords) {
                totalWakeLockTime += item.getLockingTimeMillis();
            }
            totalWakeLocks = new ArrayList<>(mFinishedWakeLockRecords);
        }

        for (WakeLockTrace item : mWorkingWakeLocks.values()) {
            if (!item.isFinished()) {
                totalWakeLocks.add(item.record);
                totalWakeLockTime += item.record.getLockingTimeMillis();
            }
        }

        WakeLockInfo info = new WakeLockInfo();
        info.totalWakeLockTime = totalWakeLockTime;
        info.totalWakeLockRecords = totalWakeLocks;
        return info;
    }

    public static class WakeLockInfo {
        public long totalWakeLockTime;
        public int totalWakeLockCount;
        List<WakeLockTrace.WakeLockRecord> totalWakeLockRecords = Collections.emptyList();
    }

    public static class WakeLockTrace {
        public interface OverTimeListener {
            void onWakeLockOvertime(int warningCount, WakeLockRecord record);
        }

        final IBinder token;
        final WakeLockRecord record;
        int warningCount;
        private Runnable loopTask;
        private OverTimeListener mListener;

        WakeLockTrace(IBinder token, String tag, int flags, String packageName) {
            this.token = token;
            this.record = new WakeLockRecord(tag, flags, packageName);
        }

        void setListener(OverTimeListener listener) {
            mListener = listener;
        }

        void start(final Handler handler, final long intervalMillis) {
            if (loopTask != null || isFinished()) {
                MatrixLog.w(TAG, "cant not start tracing of wakelock, target = " + record);
                return;
            }
            warningCount = 0;
            loopTask = new Runnable() {
                @Override
                public void run() {
                    warningCount++;
                    if (mListener != null) {
                        mListener.onWakeLockOvertime(warningCount, record);
                    }
                    handler.postDelayed(this, intervalMillis);
                }
            };
            handler.postDelayed(loopTask, intervalMillis);
        }

        public void finish(Handler handle) {
            if (loopTask != null) {
                handle.removeCallbacks(loopTask);
                loopTask = null;
            }
            record.finish();
        }

        public boolean isFinished() {
            return record.isFinished();
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

        public static class WakeLockRecord {
            public final int flags;
            public final String tag;
            public final String packageName;
            public final long timeBgn;
            public long timeEnd = 0L;

            public WakeLockRecord(String tag, int flags, String packageName) {
                this.flags = flags;
                this.tag = tag;
                this.packageName = packageName;
                this.timeBgn = SystemClock.uptimeMillis();
            }

            void finish() {
                timeEnd = SystemClock.uptimeMillis();
            }

            public boolean isFinished() {
                return timeEnd >= timeBgn;
            }

            public long getLockingTimeMillis() {
                long during = isFinished() ? timeEnd - timeBgn : SystemClock.uptimeMillis() - timeBgn;
                return during > 0 ? during : 0L;
            }

            @NonNull
            @Override
            public String toString() {
                return "WakeLockRecord{" +
                        "flag=" + flags +
                        ", tag='" + tag + '\'' +
                        ", packageName='" + packageName + '\'' +
                        ", timeBgn=" + timeBgn +
                        ", timeEnd=" + timeEnd +
                        '}';
            }
        }
    }
}
