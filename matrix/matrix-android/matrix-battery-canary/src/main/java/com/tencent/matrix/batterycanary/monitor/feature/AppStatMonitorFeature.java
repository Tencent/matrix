package com.tencent.matrix.batterycanary.monitor.feature;

import android.arch.core.util.Function;
import android.os.SystemClock;
import android.support.annotation.NonNull;
import android.support.annotation.VisibleForTesting;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * @author Kaede
 * @since 2020/12/8
 */
public final class AppStatMonitorFeature implements MonitorFeature {
    private static final String TAG = "Matrix.battery.AppStatMonitorFeature";

    @SuppressWarnings("NotNullFieldNotInitialized")
    @NonNull
    private BatteryMonitorCore mCore;
    List<Stamp> mStampList = Collections.emptyList();

    @Override
    public void configure(BatteryMonitorCore monitor) {
        mCore = monitor;
    }

    @Override
    public void onTurnOn() {
        MatrixLog.i(TAG, "#onTurnOn");
        Stamp firstStamp = new Stamp(1);
        synchronized (TAG) {
            mStampList = new ArrayList<>();
            mStampList.add(0, firstStamp);
        }
    }

    @Override
    public void onTurnOff() {
        MatrixLog.i(TAG, "#onTurnOff");
        synchronized (TAG) {
            mStampList.clear();
        }
    }

    @Override
    public void onForeground(boolean isForeground) {
        int appStat = BatteryCanaryUtil.getAppStat(mCore.getContext(), isForeground);
        synchronized (TAG) {
            if (mStampList != Collections.EMPTY_LIST) {
                mStampList.add(0, new Stamp(appStat));
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
            Stamp lastStamp = new Stamp(appStat);
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
    static AppStatSnapshot configureSnapshot(List<Stamp> stampList, long windowMillis) {
        final Map<Integer, Long> mapper = new HashMap<>();
        long totalMillis = 0L;
        long lastStampMillis = Long.MIN_VALUE;

        if (windowMillis <= 0L) {
            // configure for long all app uptime
            for (Stamp item : stampList) {
                if (lastStampMillis != Long.MIN_VALUE) {
                    if (lastStampMillis < item.upTime) {
                        // invalid data
                        break;
                    }

                    long interval = lastStampMillis - item.upTime;
                    totalMillis += interval;
                    Long record = mapper.get(item.appStat);
                    mapper.put(item.appStat, interval + (record == null ? 0 : record));
                }
                lastStampMillis = item.upTime;
            }
        } else {
            // just configure for long of the given window
            for (Stamp item : stampList) {
                if (lastStampMillis != Long.MIN_VALUE) {
                    if (lastStampMillis < item.upTime) {
                        // invalid data
                        break;
                    }

                    long interval = lastStampMillis - item.upTime;
                    if (totalMillis + interval >= windowMillis) {
                        // reach widow edge
                        long lastInterval = windowMillis - totalMillis;
                        totalMillis += lastInterval;
                        Long record = mapper.get(item.appStat);
                        mapper.put(item.appStat, lastInterval + (record == null ? 0 : record));
                        break;
                    }

                    totalMillis += interval;
                    Long record = mapper.get(item.appStat);
                    mapper.put(item.appStat, interval + (record == null ? 0 : record));
                }
                lastStampMillis = item.upTime;
            }
        }

        AppStatSnapshot snapshot = new AppStatSnapshot();
        if (totalMillis <= 0L) {
            snapshot.setValid(false);
        } else {
            // window > uptime
            if (windowMillis > totalMillis) {
                snapshot.setValid(false);
            }
            snapshot.uptime = Snapshot.Entry.DigitEntry.of(totalMillis);
            Function<Integer, Long> block = new Function<Integer, Long>() {
                @Override
                public Long apply(Integer input) {
                    Long millis = mapper.get(input);
                    return millis == null ? 0 : millis;
                }
            };
            snapshot.fgRatio = Snapshot.Entry.DigitEntry.of(Math.round((((double) block.apply(1)) / totalMillis) * 100));
            snapshot.bgRatio = Snapshot.Entry.DigitEntry.of(Math.round((((double) block.apply(2)) / totalMillis) * 100));
            snapshot.fgSrvRatio = Snapshot.Entry.DigitEntry.of(Math.round((((double) block.apply(3)) / totalMillis) * 100));
        }
        return snapshot;
    }

    @VisibleForTesting
    static final class Stamp {
        public final int appStat;
        public final long upTime;
        public Stamp(int appStat) {
            this.appStat = appStat;
            this.upTime = SystemClock.uptimeMillis();
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
}
