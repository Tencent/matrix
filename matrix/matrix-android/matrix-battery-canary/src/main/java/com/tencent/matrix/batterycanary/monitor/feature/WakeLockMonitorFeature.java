package com.tencent.matrix.batterycanary.monitor.feature;

import android.content.pm.ApplicationInfo;
import android.os.Handler;
import android.os.IBinder;
import android.os.SystemClock;
import android.os.WorkSource;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.BeanEntry;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.PowerManagerServiceHooker;
import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

@SuppressWarnings("NotNullFieldNotInitialized")
public class WakeLockMonitorFeature extends AbsMonitorFeature implements PowerManagerServiceHooker.IListener {
    private static final String TAG = "Matrix.battery.WakeLockMonitorFeature";

    public interface WakeLockListener {
        void onWakeLockTimeout(int warningCount, WakeLockTrace.WakeLockRecord record);
    }

    @VisibleForTesting
    long mOverTimeMillis;
    final Map<IBinder, WakeLockTrace> mWorkingWakeLocks = new ConcurrentHashMap<>(2);
    final List<WakeLockTrace.WakeLockRecord> mFinishedWakeLockRecords = new ArrayList<>();

    private WakeLockListener getListener() {
        return mCore;
    }

    @Override
    public void configure(BatteryMonitorCore monitor) {
        super.configure(monitor);
        mOverTimeMillis = monitor.getConfig().wakelockTimeout;
    }

    @Override
    public void onTurnOn() {
        super.onTurnOn();
        PowerManagerServiceHooker.addListener(this);
    }

    @Override
    public void onTurnOff() {
        super.onTurnOff();
        PowerManagerServiceHooker.removeListener(this);
        mCore.getHandler().removeCallbacksAndMessages(null);
        mWorkingWakeLocks.clear();
    }

    @Override
    public int weight() {
        return Integer.MIN_VALUE;
    }

    @Override
    public void onAcquireWakeLock(IBinder token, int flags, final String tag, final String packageName, WorkSource workSource, String historyTag) {
        String stack = shouldTracing(tag) ? BatteryCanaryUtil.stackTraceToString(new Throwable().getStackTrace()) : "";
        MatrixLog.i(TAG, "[onAcquireWakeLock] token=%s flags=%s tag=%s historyTag=%s packageName=%s workSource=%s stack=%s",
                String.valueOf(token), flags, tag, historyTag, packageName, workSource, stack);

        // remove duplicated old trace
        WakeLockTrace existingTrace = mWorkingWakeLocks.get(token);
        if (existingTrace != null) {
            existingTrace.finish(mCore.getHandler());
        }

        WakeLockTrace wakeLockTrace = new WakeLockTrace(token, tag, flags, packageName, stack);
        wakeLockTrace.setListener(new WakeLockTrace.OverTimeListener() {
            @Override
            public void onWakeLockOvertime(int warningCount, WakeLockTrace.WakeLockRecord record) {
                getListener().onWakeLockTimeout(warningCount, record);
            }
        });
        wakeLockTrace.start(mCore.getHandler(), mOverTimeMillis);
        mWorkingWakeLocks.put(token, wakeLockTrace);

        // dump tracing info
        dumpTracingForTag(wakeLockTrace.record.tag);
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
            wakeLockTrace.finish(mCore.getHandler());
            synchronized (mFinishedWakeLockRecords) {
                mFinishedWakeLockRecords.add(wakeLockTrace.record);
            }
            String tag = wakeLockTrace.record.tag;
            String stack = shouldTracing(tag) ? BatteryCanaryUtil.stackTraceToString(new Throwable().getStackTrace()) : "";
            MatrixLog.i(TAG, "[onReleaseWakeLock] tag = " + tag + ", stack = " + stack);

            // dump tracing info
            dumpTracingForTag(tag);

        } else {
            MatrixLog.w(TAG, "missing tracking, token = " + token);
        }
    }

    private boolean shouldTracing(String tag) {
        boolean debuggable = 0 != (mCore.getContext().getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE);
        return debuggable || !mCore.getConfig().tagWhiteList.contains(tag) || mCore.getConfig().tagBlackList.contains(tag);
    }

    private void dumpTracingForTag(String tag) {
        if (mCore.getConfig().tagBlackList.contains(tag)) {
            // dump trace of wakelocks within blacklist
            MatrixLog.w(TAG, "dump wakelocks tracing for tag '" + tag + "':");
            for (WakeLockTrace item : mWorkingWakeLocks.values()) {
                if (item.record.tag.equalsIgnoreCase(tag)) {
                    MatrixLog.w(TAG, " - " + item.record);
                }
            }
        }
    }

    public WakeLockSnapshot currentWakeLocks() {
        List<BeanEntry<WakeLockTrace.WakeLockRecord>> totalWakeLocks = new ArrayList<>();
        long totalWakeLockTime = 0L;
        int totalWakeLockCount = 0;
        synchronized (mFinishedWakeLockRecords) {
            for (WakeLockTrace.WakeLockRecord item : mFinishedWakeLockRecords) {
                totalWakeLockTime += item.getLockingTimeMillis();
                totalWakeLocks.add(BeanEntry.of(item));
            }
            totalWakeLockCount += mFinishedWakeLockRecords.size();
        }

        for (WakeLockTrace item : mWorkingWakeLocks.values()) {
            if (!item.isFinished()) {
                totalWakeLocks.add(BeanEntry.of(item.record));
                totalWakeLockTime += item.record.getLockingTimeMillis();
            }
        }
        totalWakeLockCount += mWorkingWakeLocks.size();

        WakeLockSnapshot snapshot = new WakeLockSnapshot();
        snapshot.totalWakeLockTime = Snapshot.Entry.DigitEntry.of(totalWakeLockTime);
        snapshot.totalWakeLockCount = Snapshot.Entry.DigitEntry.of(totalWakeLockCount);
        snapshot.totalWakeLockRecords = Snapshot.Entry.ListEntry.of(totalWakeLocks);
        return snapshot;
    }

    public static class WakeLockSnapshot extends Snapshot<WakeLockSnapshot> {
        public Entry.DigitEntry<Long> totalWakeLockTime;
        public Entry.DigitEntry<Integer> totalWakeLockCount;
        public Entry.ListEntry<BeanEntry<WakeLockTrace.WakeLockRecord>> totalWakeLockRecords;

        @Override
        public Delta<WakeLockSnapshot> diff(WakeLockSnapshot bgn) {
            return new Delta<WakeLockSnapshot>(bgn, this) {
                @Override
                protected WakeLockSnapshot computeDelta() {
                    WakeLockSnapshot delta = new WakeLockSnapshot();
                    delta.totalWakeLockTime = Differ.DigitDiffer.globalDiff(bgn.totalWakeLockTime, end.totalWakeLockTime);
                    delta.totalWakeLockCount = Differ.DigitDiffer.globalDiff(bgn.totalWakeLockCount, end.totalWakeLockCount);
                    delta.totalWakeLockRecords = Differ.ListDiffer.globalDiff(bgn.totalWakeLockRecords, end.totalWakeLockRecords);
                    return delta;
                }
            };
        }
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

        WakeLockTrace(IBinder token, String tag, int flags, String packageName, String stack) {
            this.token = token;
            this.record = new WakeLockRecord(tag, flags, packageName, stack);
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
            public final String stack;
            public final long timeBgn;
            public long timeEnd = 0L;

            public WakeLockRecord(String tag, int flags, String packageName, String stack) {
                this.flags = flags;
                this.tag = tag;
                this.packageName = packageName;
                this.stack = stack;
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
                        "flags=" + flags +
                        ", tag='" + tag + '\'' +
                        ", packageName='" + packageName + '\'' +
                        ", stack='" + stack + '\'' +
                        ", timeBgn=" + timeBgn +
                        ", timeEnd=" + timeEnd +
                        '}';
            }
        }
    }
}
