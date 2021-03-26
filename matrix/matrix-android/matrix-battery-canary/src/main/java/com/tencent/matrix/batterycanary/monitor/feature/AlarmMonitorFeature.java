package com.tencent.matrix.batterycanary.monitor.feature;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.os.Handler;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;
import android.util.Log;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.utils.AlarmManagerServiceHooker;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.util.MatrixLog;

@SuppressWarnings("NotNullFieldNotInitialized")
public final class AlarmMonitorFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.AlarmMonitorFeature";

    public interface AlarmListener {
        void onAlarmDuplicated(int duplicatedCount, AlarmRecord record);
    }

    @NonNull
    @VisibleForTesting
    Handler handler;
    final AlarmTracing mAlarmTracing = new AlarmTracing();
    @Nullable
    AlarmManagerServiceHooker.IListener mListener;

    private AlarmListener getListener() {
        return mCore;
    }

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    public void configure(BatteryMonitorCore monitor) {
        super.configure(monitor);
        handler = monitor.getHandler();
    }

    @Override
    public void onTurnOn() {
        super.onTurnOn();
        if (mCore.getConfig().isAmsHookEnabled) {
            mListener = new AlarmManagerServiceHooker.IListener() {
                @Override
                public void onAlarmSet(int type, long triggerAtMillis, long windowMillis, long intervalMillis, int flags, PendingIntent operation, AlarmManager.OnAlarmListener onAlarmListener) {
                    String stack = "";
                    if (mCore.getConfig().isStatAsSample) {
                        stack = BatteryCanaryUtil.polishStack(Log.getStackTraceString(new Throwable()), "at android.app.AlarmManager");
                    }

                    AlarmRecord alarmRecord = new AlarmRecord(type, triggerAtMillis, windowMillis, intervalMillis, flags, stack);
                    MatrixLog.i(TAG, "#onAlarmSet, target = " + alarmRecord);

                    if (operation != null || onAlarmListener != null) {
                        int traceKey = operation != null ? operation.hashCode() : onAlarmListener.hashCode();
                        mAlarmTracing.onSet(traceKey, alarmRecord);
                    }
                }

                @Override
                public void onAlarmRemove(PendingIntent operation, AlarmManager.OnAlarmListener onAlarmListener) {
                    if (operation != null || onAlarmListener != null) {
                        int traceKey = operation != null ? operation.hashCode() : onAlarmListener.hashCode();
                        mAlarmTracing.onRemove(traceKey);
                    }
                }
            };
            AlarmManagerServiceHooker.addListener(mListener);
        }

    }

    @Override
    public void onTurnOff() {
        super.onTurnOff();
        AlarmManagerServiceHooker.removeListener(mListener);
        handler.removeCallbacksAndMessages(null);
        mAlarmTracing.onClear();
    }

    @Override
    public int weight() {
        return Integer.MIN_VALUE;
    }

    @VisibleForTesting
    void onAlarmSet(int type, long triggerAtMillis, long windowMillis, long intervalMillis, int flags, PendingIntent operation, AlarmManager.OnAlarmListener onAlarmListener) {
        if (mListener != null) {
            mListener.onAlarmSet(type, triggerAtMillis, windowMillis, intervalMillis, flags, operation, onAlarmListener);
        }
    }

    @VisibleForTesting
    void onAlarmRemove(PendingIntent operation, AlarmManager.OnAlarmListener onAlarmListener) {
        if (mListener != null) {
            mListener.onAlarmRemove(operation, onAlarmListener);
        }
    }

    @NonNull
    public AlarmTracing getTracing() {
        return mAlarmTracing;
    }

    public AlarmSnapshot currentAlarms() {
        return mAlarmTracing.getSnapshot();
    }

    public static class AlarmRecord {
        public final int type;
        public final long triggerAtMillis;
        public final long windowMillis;
        public final long intervalMillis;
        public final int flag;
        public final long timeBgn;
        public final String stack;

        public AlarmRecord(int type, long triggerAtMillis, long windowMillis, long intervalMillis, int flag, String stack) {
            this.type = type;
            this.triggerAtMillis = triggerAtMillis;
            this.windowMillis = windowMillis;
            this.intervalMillis = intervalMillis;
            this.flag = flag;
            this.timeBgn = System.currentTimeMillis();
            this.stack = stack;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (o == null || getClass() != o.getClass()) return false;
            AlarmRecord that = (AlarmRecord) o;
            return hashCode() == that.hashCode();
        }

        @NonNull
        @Override
        public String toString() {
            return "AlarmRecord{"
                    + "type=" + type
                    + ", triggerAtMillis=" + triggerAtMillis
                    + ", windowMillis=" + windowMillis
                    + ", intervalMillis=" + intervalMillis
                    + ", flag=" + flag
                    + ", timeBgn=" + timeBgn
                    + ", stack='" + stack + '\''
                    + '}';
        }
    }

    public static final class AlarmTracing {
        private final byte[] mLock = new byte[]{};
        private int mTotalCount;
        private int mTracingCount;
        private int mDuplicatedGroups;
        private int mDuplicatedCounts;

        public void onSet(int key, AlarmRecord record) {
            synchronized (mLock) {
                mTotalCount++;
                mTracingCount++;
            }
        }

        public void onSet() {
            synchronized (mLock) {
                mTotalCount++;
                mTracingCount++;
            }
        }

        public void onRemove(int key) {
            synchronized (mLock) {
                mTracingCount--;
            }
        }

        public void onRemove() {
            synchronized (mLock) {
                mTracingCount--;
            }
        }

        public void onClear() {
            mTotalCount = 0;
            mTracingCount = 0;
            mDuplicatedGroups = 0;
            mDuplicatedCounts = 0;
        }

        public AlarmSnapshot getSnapshot() {
            AlarmSnapshot snapshot = new AlarmSnapshot();
            synchronized (mLock) {
                snapshot.totalCount = Snapshot.Entry.DigitEntry.of(mTotalCount);
                snapshot.tracingCount = Snapshot.Entry.DigitEntry.of(mTracingCount);
                snapshot.duplicatedGroup = Snapshot.Entry.DigitEntry.of(mDuplicatedGroups);
                snapshot.duplicatedCount = Snapshot.Entry.DigitEntry.of(mDuplicatedCounts);
                snapshot.records = Snapshot.Entry.ListEntry.ofEmpty();
            }
            return snapshot;
        }
    }

    public static class AlarmSnapshot extends Snapshot<AlarmSnapshot> {
        public Entry.DigitEntry<Integer> totalCount;
        public Entry.DigitEntry<Integer> tracingCount;
        public Entry.DigitEntry<Integer> duplicatedGroup;
        public Entry.DigitEntry<Integer> duplicatedCount;
        public Entry.ListEntry<Entry.BeanEntry<AlarmRecord>> records;

        @Override
        public Delta<AlarmSnapshot> diff(AlarmSnapshot bgn) {
            return new Delta<AlarmSnapshot>(bgn, this) {
                @Override
                protected AlarmSnapshot computeDelta() {
                    AlarmSnapshot snapshot = new AlarmSnapshot();
                    snapshot.totalCount = Differ.DigitDiffer.globalDiff(bgn.totalCount, end.totalCount);
                    snapshot.tracingCount = Differ.DigitDiffer.globalDiff(bgn.tracingCount, end.tracingCount);
                    snapshot.duplicatedGroup = Differ.DigitDiffer.globalDiff(bgn.duplicatedGroup, end.duplicatedGroup);
                    snapshot.duplicatedCount = Differ.DigitDiffer.globalDiff(bgn.duplicatedCount, end.duplicatedCount);
                    snapshot.records = Differ.ListDiffer.globalDiff(bgn.records, end.records);
                    return snapshot;
                }
            };
        }
    }
}
