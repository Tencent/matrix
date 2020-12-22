package com.tencent.matrix.batterycanary.monitor.feature;

import android.support.annotation.NonNull;
import android.support.annotation.VisibleForTesting;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.TimeBreaker;
import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * @author Kaede
 * @since 2020/12/8
 */
public final class AppStatMonitorFeature implements MonitorFeature {
    private static final String TAG = "Matrix.battery.AppStatMonitorFeature";

    @SuppressWarnings("NotNullFieldNotInitialized")
    @NonNull private BatteryMonitorCore mCore;
    @NonNull List<AppStatStamp> mStampList = Collections.emptyList();
    @NonNull List<TimeBreaker.Stamp> mSceneStampList = Collections.emptyList();

    @Override
    public void configure(BatteryMonitorCore monitor) {
        mCore = monitor;
    }

    @Override
    public void onTurnOn() {
        MatrixLog.i(TAG, "#onTurnOn");
        AppStatStamp firstStamp = new AppStatStamp(1);
        TimeBreaker.Stamp firstSceneStamp = new TimeBreaker.Stamp(mCore.getScene());
        synchronized (TAG) {
            mStampList = new ArrayList<>();
            mStampList.add(0, firstStamp);
            mSceneStampList = new ArrayList<>();
            mSceneStampList.add(0, firstSceneStamp);
        }
    }

    @Override
    public void onTurnOff() {
        MatrixLog.i(TAG, "#onTurnOff");
        synchronized (TAG) {
            mStampList.clear();
            mSceneStampList.clear();
        }
    }

    @Override
    public void onForeground(boolean isForeground) {
        int appStat = BatteryCanaryUtil.getAppStat(mCore.getContext(), isForeground);
        synchronized (TAG) {
            if (mStampList != Collections.EMPTY_LIST) {
                mStampList.add(0, new AppStatStamp(appStat));
            }
        }
    }

    public void onStatScene(@NonNull String scene) {
        synchronized (TAG) {
            if (mSceneStampList != Collections.EMPTY_LIST) {
                mSceneStampList.add(0, new TimeBreaker.Stamp(scene));
            }
        }
    }

    @Override
    public int weight() {
        return Integer.MAX_VALUE;
    }

    public AppStatSnapshot currentAppStatSnapshot() {
        return currentAppStatSnapshot(0L);
    }

    public AppStatSnapshot currentAppStatSnapshot(long windowMillis) {
        try {
            int appStat = BatteryCanaryUtil.getAppStat(mCore.getContext(), mCore.isForeground());
            AppStatStamp lastStamp = new AppStatStamp(appStat);
            synchronized (TAG) {
                if (mStampList != Collections.EMPTY_LIST) {
                    mStampList.add(0, lastStamp);
                }
            }
            return configureSnapshot(mStampList, windowMillis);
        } catch (Throwable e) {
            MatrixLog.w(TAG, "configureSnapshot fail: " + e.getMessage());
            AppStatSnapshot snapshot = new AppStatSnapshot();
            snapshot.setValid(false);
            return snapshot;
        }
    }

    @VisibleForTesting
    static AppStatSnapshot configureSnapshot(List<AppStatStamp> stampList, long windowMillis) {
        TimeBreaker.TimePortions timePortions = TimeBreaker.configurePortions(stampList, windowMillis);
        AppStatSnapshot snapshot = new AppStatSnapshot();
        snapshot.setValid(timePortions.isValid());
        snapshot.uptime = Snapshot.Entry.DigitEntry.of(timePortions.totalUptime);
        snapshot.fgRatio = Snapshot.Entry.DigitEntry.of((long) timePortions.getRatio("1"));
        snapshot.bgRatio = Snapshot.Entry.DigitEntry.of((long) timePortions.getRatio("2"));
        snapshot.fgSrvRatio = Snapshot.Entry.DigitEntry.of((long) timePortions.getRatio("3"));
        return snapshot;
    }

    @VisibleForTesting
    static final class AppStatStamp extends TimeBreaker.Stamp {
        public AppStatStamp(int appStat) {
            super(String.valueOf(appStat));
        }
    }

    public static final class AppStatSnapshot extends Snapshot<AppStatSnapshot> {
        public Entry.DigitEntry<Long> uptime = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> fgRatio = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> bgRatio = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> fgSrvRatio = Entry.DigitEntry.of(0L);

        AppStatSnapshot() {}

        @Override
        public Delta<AppStatSnapshot> diff(AppStatSnapshot bgn) {
            return new Delta<AppStatSnapshot>(bgn, this) {
                @Override
                protected AppStatSnapshot computeDelta() {
                    AppStatSnapshot delta = new AppStatSnapshot();
                    delta.uptime = Differ.DigitDiffer.globalDiff(bgn.uptime, end.uptime);
                    delta.fgRatio = Differ.DigitDiffer.globalDiff(bgn.fgRatio, end.fgRatio);
                    delta.bgRatio = Differ.DigitDiffer.globalDiff(bgn.bgRatio, end.bgRatio);
                    delta.fgSrvRatio = Differ.DigitDiffer.globalDiff(bgn.fgSrvRatio, end.fgSrvRatio);
                    return delta;
                }
            };
        }
    }

    public TimeBreaker.TimePortions currentSceneSnapshot() {
        return currentSceneSnapshot(0L);
    }

    public TimeBreaker.TimePortions currentSceneSnapshot(long windowMillis) {
        try {
            TimeBreaker.Stamp lastSceneStamp = new TimeBreaker.Stamp(mCore.getScene());
            synchronized (TAG) {
                if (mSceneStampList != Collections.EMPTY_LIST) {
                    mSceneStampList.add(0, lastSceneStamp);
                }
            }
            return configureSceneSnapshot(mSceneStampList, windowMillis);
        } catch (Throwable e) {
            MatrixLog.w(TAG, "currentSceneSnapshot fail: " + e.getMessage());
            return TimeBreaker.TimePortions.ofInvalid();
        }
    }

    @VisibleForTesting
    static TimeBreaker.TimePortions configureSceneSnapshot(List<TimeBreaker.Stamp> stampList, long windowMillis) {
        return TimeBreaker.configurePortions(stampList, windowMillis);
    }
}
