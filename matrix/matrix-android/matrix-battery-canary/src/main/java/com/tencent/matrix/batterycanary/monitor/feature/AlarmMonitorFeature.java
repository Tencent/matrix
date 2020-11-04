package com.tencent.matrix.batterycanary.monitor.feature;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.os.Handler;
import android.support.annotation.NonNull;
import android.support.annotation.VisibleForTesting;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.utils.AlarmManagerServiceHooker;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

@SuppressWarnings("NotNullFieldNotInitialized")
public class AlarmMonitorFeature implements MonitorFeature, AlarmManagerServiceHooker.IListener {
    private static final String TAG = "Matrix.monitor.AlarmMonitorFeature";

    public interface AlarmListener {
        void onAlarmDuplicated(int duplicatedCount, AlarmRecord record);
    }

    @NonNull
    private BatteryMonitorCore monitor;
    @NonNull
    @VisibleForTesting
    Handler handler;
    final Map<Integer, List<AlarmRecord>> mTracingAlarms = new ConcurrentHashMap<>(2);
    final List<AlarmRecord> mTotalAlarms = new ArrayList<>();

    private AlarmListener getListener() {
        return monitor;
    }

    @Override
    public void configure(BatteryMonitorCore monitor) {
        MatrixLog.i(TAG, "#configure monitor feature");
        this.monitor = monitor;
        handler = new Handler(MatrixHandlerThread.getDefaultHandlerThread().getLooper());
    }

    @Override
    public void onTurnOn() {
        MatrixLog.i(TAG, "#onTurnOn");
        AlarmManagerServiceHooker.addListener(this);
    }

    @Override
    public void onTurnOff() {
        MatrixLog.i(TAG, "#onTurnOff");
        AlarmManagerServiceHooker.removeListener(this);
        handler.removeCallbacksAndMessages(null);
        mTracingAlarms.clear();
        mTotalAlarms.clear();
    }

    @Override
    public void onForeground(boolean isForeground) {}

    @Override
    public int weight() {
        return Integer.MIN_VALUE;
    }


    @Override
    public void onAlarmSet(int type, long triggerAtMillis, long windowMillis, long intervalMillis, int flags, PendingIntent operation, AlarmManager.OnAlarmListener onAlarmListener) {
        AlarmRecord alarmRecord = new AlarmRecord(type, triggerAtMillis, windowMillis, intervalMillis, flags);
        MatrixLog.i(TAG, "#onAlarmSet, target = " + alarmRecord);

        synchronized (mTotalAlarms) {
            mTotalAlarms.add(alarmRecord);
        }

        if (operation != null || onAlarmListener != null) {
            int traceKey = operation != null ? operation.hashCode() : onAlarmListener.hashCode();

            List<AlarmRecord> records = mTracingAlarms.get(traceKey);
            if (records == null) {
                records = new LinkedList<>();
                records.add(alarmRecord);
                mTracingAlarms.put(traceKey, records);
            } else {
                // duplicated
                records.add(alarmRecord);
                getListener().onAlarmDuplicated(records.size(), alarmRecord);
            }
        }
    }

    @Override
    public void onAlarmRemove(PendingIntent operation, AlarmManager.OnAlarmListener onAlarmListener) {
        if (operation != null || onAlarmListener != null) {
            int traceKey = operation != null ? operation.hashCode() : onAlarmListener.hashCode();
            mTracingAlarms.remove(traceKey);
        }
    }

    public AlarmSnapshot currentAlarms() {
        AlarmSnapshot snapshot = new AlarmSnapshot();
        synchronized (mTotalAlarms) {
            snapshot.totalCount = mTotalAlarms.size();
            snapshot.tracingCount = mTracingAlarms.size();
            for (List<AlarmRecord> item : mTracingAlarms.values()) {
                if (item.size() > 1) {
                    snapshot.duplicatedGroup ++;
                    snapshot.duplicatedCount += item.size();
                }
            }
        }
        return snapshot;
    }

    public static class AlarmRecord {
        public final int type;
        public final long triggerAtMillis;
        public final long windowMillis;
        public final long intervalMillis;
        public final int flag;
        public final long timeBgn;

        public AlarmRecord(int type, long triggerAtMillis, long windowMillis, long intervalMillis, int flag) {
            this.type = type;
            this.triggerAtMillis = triggerAtMillis;
            this.windowMillis = windowMillis;
            this.intervalMillis = intervalMillis;
            this.flag = flag;
            this.timeBgn = System.currentTimeMillis();
        }

        @Override
        @NonNull
        public String toString() {
            return "AlarmRecord{" +
                    "type=" + type +
                    ", triggerAtMillis=" + triggerAtMillis +
                    ", windowMillis=" + windowMillis +
                    ", intervalMillis=" + intervalMillis +
                    ", flag=" + flag +
                    ", timeBgn=" + timeBgn +
                    '}';
        }
    }

    public static class AlarmSnapshot extends Snapshot<AlarmSnapshot> {
        public int totalCount;
        public int tracingCount;
        public int duplicatedGroup;
        public int duplicatedCount;

        @Override
        public Delta<AlarmSnapshot> diff(AlarmSnapshot bgn) {
            return new Delta<AlarmSnapshot>(bgn, this) {
                @Override
                protected AlarmSnapshot computeDelta() {
                    AlarmSnapshot snapshot = new AlarmSnapshot();
                    snapshot.totalCount = Differ.sDigitDiffer.diffInt(bgn.totalCount, end.totalCount);
                    snapshot.tracingCount = Differ.sDigitDiffer.diffInt(bgn.tracingCount, end.tracingCount);
                    snapshot.duplicatedCount = Differ.sDigitDiffer.diffInt(bgn.duplicatedGroup, end.duplicatedGroup);
                    snapshot.duplicatedCount = Differ.sDigitDiffer.diffInt(bgn.duplicatedCount, end.duplicatedCount);
                    return snapshot;
                }
            };
        }
    }
}
