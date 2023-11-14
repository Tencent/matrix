package com.tencent.matrix.batterycanary.monitor.feature;

import android.os.Handler;
import android.os.IBinder;
import android.os.SystemClock;
import android.os.WorkSource;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.BeanEntry;
import com.tencent.matrix.batterycanary.utils.PowerManagerServiceHooker;
import com.tencent.matrix.util.MatrixLog;

import java.util.Iterator;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

@SuppressWarnings("NotNullFieldNotInitialized")
public final class WakeLockMonitorFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.WakeLockMonitorFeature";

    public interface WakeLockListener {
        @Deprecated
        void onWakeLockTimeout(int warningCount, WakeLockTrace.WakeLockRecord record);

        void onWakeLockTimeout(WakeLockTrace.WakeLockRecord record, long backgroundMillis);
    }

    @VisibleForTesting
    long mOverTimeMillis;
    final Map<IBinder, WakeLockTrace> mWorkingWakeLocks = new ConcurrentHashMap<>(2);
    final WakeLockTracing mWakeLockTracing = new WakeLockTracing();
    @Nullable
    PowerManagerServiceHooker.IListener mListener;

    private WakeLockListener getListener() {
        return mCore;
    }

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    public void configure(BatteryMonitorCore monitor) {
        super.configure(monitor);
        mOverTimeMillis = monitor.getConfig().wakelockTimeout;
    }

    @Override
    public void onTurnOn() {
        super.onTurnOn();
        if (mCore.getConfig().isAmsHookEnabled) {
            mListener = new PowerManagerServiceHooker.IListener() {
                @Override
                public void onAcquireWakeLock(IBinder token, int flags, String tag, String packageName, @Nullable WorkSource workSource, @Nullable String historyTag) {
                    String stack = shouldTracing(tag) ? mCore.getConfig().callStackCollector.collectCurr() : "";
                    MatrixLog.i(TAG, "[onAcquireWakeLock] token=%s flags=%s tag=%s historyTag=%s packageName=%s workSource=%s stack=%s",
                            String.valueOf(token), flags, tag, historyTag, packageName, workSource, stack);

                    // remove duplicated old trace
                    WakeLockTrace existingTrace = mWorkingWakeLocks.get(token);
                    if (existingTrace != null) {
                        existingTrace.finish(mCore.getHandler());
                    }

                    final WakeLockTrace wakeLockTrace = new WakeLockTrace(token, tag, flags, packageName, stack);
                    wakeLockTrace.setListener(new WakeLockTrace.OverTimeListener() {
                        @Override
                        public void onWakeLockOvertime(int warningCount, WakeLockTrace.WakeLockRecord record) {
                            getListener().onWakeLockTimeout(warningCount, record);
                            if (wakeLockTrace.isExpired()) {
                                wakeLockTrace.finish(mCore.getHandler());
                                Iterator<Map.Entry<IBinder, WakeLockTrace>> iterator = mWorkingWakeLocks.entrySet().iterator();
                                while (iterator.hasNext()) {
                                    Map.Entry<IBinder, WakeLockTrace> next = iterator.next();
                                    if (next.getValue() == wakeLockTrace) {
                                        iterator.remove();
                                        break;
                                    }
                                }
                            }
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
                        mWakeLockTracing.add(wakeLockTrace.record);
                        String tag = wakeLockTrace.record.tag;
                        String stack = shouldTracing(tag) ? mCore.getConfig().callStackCollector.collectCurr() : "";
                        MatrixLog.i(TAG, "[onReleaseWakeLock] tag = " + tag + ", stack = " + stack);

                        // dump tracing info
                        dumpTracingForTag(tag);

                    } else {
                        MatrixLog.w(TAG, "missing tracking, token = " + token);
                    }
                }
            };
            PowerManagerServiceHooker.addListener(mListener);
        }
    }

    @Override
    public void onTurnOff() {
        super.onTurnOff();
        PowerManagerServiceHooker.removeListener(mListener);
        mCore.getHandler().removeCallbacksAndMessages(null);
        mWorkingWakeLocks.clear();
        mWakeLockTracing.onClear();
    }

    @Override
    public void onBackgroundCheck(long duringMillis) {
        super.onBackgroundCheck(duringMillis);
        if (!mWorkingWakeLocks.isEmpty()) {
            for (WakeLockTrace item : mWorkingWakeLocks.values()) {
                if (!item.isFinished()) {
                    if (shouldTracing(item.record.tag)) {
                        // wakelock not released in background
                        getListener().onWakeLockTimeout(item.record, duringMillis);
                    }
                }
            }
        }
    }

    @Override
    public int weight() {
        return Integer.MIN_VALUE;
    }

    @VisibleForTesting
    void onAcquireWakeLock(IBinder token, int flags, final String tag, final String packageName, WorkSource workSource, String historyTag) {
        if (mListener != null) {
            mListener.onAcquireWakeLock(token, flags, tag, packageName, workSource, historyTag);
        }
    }

    @VisibleForTesting
    void onReleaseWakeLock(IBinder token, int flags) {
        if (mListener != null) {
            mListener.onReleaseWakeLock(token, flags);
        }
    }

    private boolean shouldTracing(String tag) {
        return shouldTracing() || !mCore.getConfig().tagWhiteList.contains(tag) || mCore.getConfig().tagBlackList.contains(tag);
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

    @NonNull
    public WakeLockTracing getTracing() {
        return mWakeLockTracing;
    }

    public WakeLockSnapshot currentWakeLocks() {
        return mWakeLockTracing.getSnapshot();
    }

    public static class WakeLockTrace {
        public interface OverTimeListener {
            void onWakeLockOvertime(int warningCount, WakeLockRecord record);
        }

        final IBinder token;
        final WakeLockRecord record;
        int warningCount;
        int warningCountLimit = 30;
        private Runnable loopTask;
        private OverTimeListener mListener;

        WakeLockTrace(IBinder token, String tag, int flags, String packageName, String stack) {
            this.token = token;
            this.record = new WakeLockRecord(tag, flags, packageName, stack);
        }

        public WakeLockTrace attach(int warningCountLimit) {
            this.warningCountLimit = warningCountLimit;
            return this;
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
                    if (warningCount < warningCountLimit) {
                        handler.postDelayed(this, intervalMillis);
                    }
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

        public boolean isExpired() {
            return warningCount >= warningCountLimit;
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
                return "WakeLockRecord{"
                        + "flags=" + flags
                        + ", tag='" + tag
                        + '\'' + ", packageName='" + packageName + '\''
                        + ", stack='" + stack + '\''
                        + ", timeBgn=" + timeBgn
                        + ", timeEnd=" + timeEnd
                        + '}';
            }
        }
    }

    public static final class WakeLockTracing {
        private final byte[] mLock = new byte[]{};
        private int mCount;
        private long mMillis;
        private int mTotalCount;
        private int mTracingCount;

        public void add(WakeLockTrace.WakeLockRecord record) {
            synchronized (mLock) {
                mCount++;
                mMillis += record.getLockingTimeMillis();
            }
        }

        public void onAcquire() {
            synchronized (mLock) {
                mTotalCount++;
                mTracingCount++;
            }
        }

        public void onRelease() {
            synchronized (mLock) {
                mTracingCount--;
            }
        }

        public int getTotalCount() {
            return mCount;
        }

        public long getTimeMillis() {
            return mMillis;
        }

        public void onClear() {
            mCount = 0;
            mMillis = 0;
            mTotalCount = 0;
            mTracingCount = 0;
        }

        public WakeLockSnapshot getSnapshot() {
            WakeLockSnapshot snapshot = new WakeLockSnapshot();
            snapshot.totalWakeLockTime = Snapshot.Entry.DigitEntry.of(getTimeMillis());
            snapshot.totalWakeLockCount = Snapshot.Entry.DigitEntry.of(getTotalCount());
            snapshot.totalWakeLockRecords = Snapshot.Entry.ListEntry.ofEmpty();
            snapshot.totalAcquireCount = Snapshot.Entry.DigitEntry.of(mTotalCount);
            snapshot.totalReleaseCount = Snapshot.Entry.DigitEntry.of(mTracingCount);
            return snapshot;
        }
    }

    public static class WakeLockSnapshot extends Snapshot<WakeLockSnapshot> {
        public Entry.DigitEntry<Long> totalWakeLockTime;
        public Entry.DigitEntry<Integer> totalWakeLockCount;
        public Entry.ListEntry<BeanEntry<WakeLockTrace.WakeLockRecord>> totalWakeLockRecords;
        public Entry.DigitEntry<Integer> totalAcquireCount;
        public Entry.DigitEntry<Integer> totalReleaseCount;

        WakeLockSnapshot() {
        }

        @Override
        public Delta<WakeLockSnapshot> diff(WakeLockSnapshot bgn) {
            return new Delta<WakeLockSnapshot>(bgn, this) {
                @Override
                protected WakeLockSnapshot computeDelta() {
                    WakeLockSnapshot delta = new WakeLockSnapshot();
                    delta.totalWakeLockTime = Differ.DigitDiffer.globalDiff(bgn.totalWakeLockTime, end.totalWakeLockTime);
                    delta.totalWakeLockCount = Differ.DigitDiffer.globalDiff(bgn.totalWakeLockCount, end.totalWakeLockCount);
                    delta.totalWakeLockRecords = Differ.ListDiffer.globalDiff(bgn.totalWakeLockRecords, end.totalWakeLockRecords);
                    delta.totalAcquireCount = Differ.DigitDiffer.globalDiff(bgn.totalAcquireCount, end.totalAcquireCount);
                    delta.totalReleaseCount = Differ.DigitDiffer.globalDiff(bgn.totalReleaseCount, end.totalReleaseCount);
                    return delta;
                }
            };
        }
    }
}
