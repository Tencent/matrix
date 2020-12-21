package com.tencent.matrix.batterycanary.monitor.feature;

import android.annotation.SuppressLint;
import android.arch.core.util.Function;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;
import android.support.v4.util.Consumer;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.AppStatMonitorFeature.Stamp;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Differ.DigitDiffer;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Differ.ListDiffer;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.RadioStatUtil;
import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Device Status Monitoring:
 * - CpuFreq
 * - Battery Status
 * - Temperatures
 *
 * @author Kaede
 * @since 2020/11/1
 */
@SuppressWarnings("NotNullFieldNotInitialized")
public final class DeviceStatMonitorFeature implements MonitorFeature {
    private static final String TAG = "Matrix.battery.DeviceStatusMonitorFeature";

    @NonNull private BatteryMonitorCore mMonitor;
    @NonNull private DevStatListener mDevStatListener;
    @NonNull List<Stamp> mStampList = Collections.emptyList();

    @Override
    public void configure(BatteryMonitorCore monitor) {
        MatrixLog.i(TAG, "#configure monitor feature");
        mMonitor = monitor;
        mDevStatListener = new DevStatListener();
    }

    @Override
    public void onTurnOn() {
        int deviceStat = BatteryCanaryUtil.getDeviceStat(mMonitor.getContext());
        @SuppressLint("VisibleForTests") Stamp firstStamp = new Stamp(deviceStat);
        synchronized (TAG) {
            mStampList = new ArrayList<>();
            mStampList.add(0, firstStamp);
        }

        mDevStatListener.setListener(new Consumer<Integer>() {
            @SuppressLint("VisibleForTests")
            @Override
            public void accept(Integer integer) {
                synchronized (TAG) {
                    if (mStampList != Collections.EMPTY_LIST) {
                        mStampList.add(0, new Stamp(integer));
                    }
                }
            }
        });

        mDevStatListener.startListen(mMonitor.getContext());
    }

    @Override
    public void onTurnOff() {
        mDevStatListener.stopListen(mMonitor.getContext());
    }

    @Override
    public void onForeground(boolean isForeground) {
        if (!isForeground) {
            if (!mDevStatListener.isListening()) {
                mDevStatListener.startListen(mMonitor.getContext());
            }
        }
    }

    @Override
    public int weight() {
        return Integer.MAX_VALUE;
    }

    public CpuFreqSnapshot currentCpuFreq() {
        CpuFreqSnapshot snapshot = new CpuFreqSnapshot();
        try {
            snapshot.cpuFreqs = Snapshot.Entry.ListEntry.ofDigits(BatteryCanaryUtil.getCpuCurrentFreq());
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "#currentCpuFreq error");
            snapshot.cpuFreqs = Snapshot.Entry.ListEntry.ofDigits(new int[]{});
        }
        return snapshot;
    }

    public BatteryTmpSnapshot currentBatteryTemperature(Context context) {
        BatteryTmpSnapshot snapshot = new BatteryTmpSnapshot();
        snapshot.temp = Snapshot.Entry.DigitEntry.of(mMonitor.getCurrentBatteryTemperature(context));
        return snapshot;
    }

    @Nullable
    public RadioStatSnapshot currentRadioSnapshot(Context context) {
        RadioStatUtil.RadioStat stat = RadioStatUtil.getCurrentStat(context);
        if (stat == null) {
            return null;
        }
        RadioStatSnapshot snapshot = new RadioStatSnapshot();
        snapshot.wifiRxBytes = Snapshot.Entry.DigitEntry.of(stat.wifiRxBytes);
        snapshot.wifiTxBytes = Snapshot.Entry.DigitEntry.of(stat.wifiTxBytes);
        snapshot.mobileRxBytes = Snapshot.Entry.DigitEntry.of(stat.mobileRxBytes);
        snapshot.mobileTxBytes = Snapshot.Entry.DigitEntry.of(stat.mobileTxBytes);
        return snapshot;
    }

    public DevStatSnapshot currentDevStatSnapshot() {
        return currentDevStatSnapshot(0L);
    }

    public DevStatSnapshot currentDevStatSnapshot(long windowMillis) {
        try {
            int appStat = BatteryCanaryUtil.getDeviceStat(mMonitor.getContext());
            @SuppressLint("VisibleForTests") Stamp lastStamp = new Stamp(appStat);
            synchronized (TAG) {
                if (mStampList != Collections.EMPTY_LIST) {
                    mStampList.add(0, lastStamp);
                }
            }
            return configureSnapshot(mStampList, windowMillis);
        } catch (Throwable e) {
            MatrixLog.w(TAG, "configureSnapshot fail: " + e.getMessage());
            DevStatSnapshot snapshot = new DevStatSnapshot();
            snapshot.setValid(false);
            return snapshot;
        }
    }

    @VisibleForTesting
    static DevStatSnapshot configureSnapshot(List<Stamp> stampList, long windowMillis) {
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

        DevStatSnapshot snapshot = new DevStatSnapshot();
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
            snapshot.chargingRatio = Snapshot.Entry.DigitEntry.of(Math.round((((double) block.apply(1)) / totalMillis) * 100));
            snapshot.screenOff = Snapshot.Entry.DigitEntry.of(Math.round((((double) block.apply(3)) / totalMillis) * 100));
        }
        return snapshot;
    }

    static final class DevStatListener extends BroadcastReceiver {
        Consumer<Integer> mListener = new Consumer<Integer>() {
            @Override
            public void accept(Integer integer) {

            }
        };

        boolean mIsCharging = false;
        boolean mIsScreenOff = false;
        boolean mIsListening = false;

        public void setListener(Consumer<Integer> listener) {
            mListener = listener;
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action != null) {
                if (action.equals(Intent.ACTION_BATTERY_CHANGED)) {
                    int plugged = intent.getIntExtra(BatteryManager.EXTRA_PLUGGED, -1);
                    mIsCharging = plugged == BatteryManager.BATTERY_PLUGGED_AC || plugged == BatteryManager.BATTERY_PLUGGED_USB || plugged == BatteryManager.BATTERY_PLUGGED_WIRELESS;
                    updateStatus();
                    return;
                }
                if (action.equals(Intent.ACTION_SCREEN_OFF)) {
                    mIsScreenOff = true;
                    updateStatus();
                    return;
                }
                if (action.equals(Intent.ACTION_SCREEN_ON)) {
                    mIsScreenOff = false;
                    updateStatus();
                }
            }
        }

        protected void updateStatus() {
            int devStat = mIsCharging ? 1 : mIsScreenOff ? 3 : 2;
            mListener.accept(devStat);
        }

        public boolean isListening() {
            return mIsListening;
        }

        public boolean startListen(Context context) {
            if (!mIsListening) {
                try {
                    IntentFilter filter = new IntentFilter(Intent.ACTION_SCREEN_ON);
                    filter.addAction(Intent.ACTION_SCREEN_OFF);
                    filter.addAction(Intent.ACTION_BATTERY_CHANGED);
                    context.registerReceiver(this, filter);
                    mIsListening = true;
                    return true;
                } catch (Throwable e) {
                    MatrixLog.printErrStackTrace(TAG, e, "#startListen failed");
                    try {
                        context.unregisterReceiver(this);
                    } catch (Throwable ignored) {
                    }
                    mIsListening = false;
                    return false;
                }
            } else {
                return true;
            }
        }

        public void stopListen(Context context) {
            if (mIsListening) {
                try {
                    context.unregisterReceiver(this);
                } catch (Throwable ignored) {
                }
                mIsListening = false;
            }
        }
    }

    public static class CpuFreqSnapshot extends Snapshot<CpuFreqSnapshot> {
        public Entry.ListEntry<Entry.DigitEntry<Integer>> cpuFreqs;

        @Override
        public Delta<CpuFreqSnapshot> diff(CpuFreqSnapshot bgn) {
            return new Delta<CpuFreqSnapshot>(bgn, this) {
                @Override
                protected CpuFreqSnapshot computeDelta() {
                    CpuFreqSnapshot delta = new CpuFreqSnapshot();
                    delta.cpuFreqs = ListDiffer.globalDiff(bgn.cpuFreqs, end.cpuFreqs);
                    return delta;
                }
            };
        }
    }

    public static class BatteryTmpSnapshot extends Snapshot<BatteryTmpSnapshot> {
        public Entry.DigitEntry<Integer> temp;

        @Override
        public Delta<BatteryTmpSnapshot>  diff(BatteryTmpSnapshot bgn) {
            return new Delta<BatteryTmpSnapshot>(bgn, this) {
                @Override
                protected BatteryTmpSnapshot computeDelta() {
                    BatteryTmpSnapshot delta = new BatteryTmpSnapshot();
                    delta.temp = DigitDiffer.globalDiff(bgn.temp, end.temp);
                    return delta;
                }
            };
        }
    }

    public static class RadioStatSnapshot extends Snapshot<RadioStatSnapshot> {
        public Entry.DigitEntry<Long> wifiRxBytes = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> wifiTxBytes = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> mobileRxBytes = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> mobileTxBytes = Entry.DigitEntry.of(0L);

        @Override
        public Delta<RadioStatSnapshot> diff(RadioStatSnapshot bgn) {
            return new Delta<RadioStatSnapshot>(bgn, this) {
                @Override
                protected RadioStatSnapshot computeDelta() {
                    RadioStatSnapshot delta = new RadioStatSnapshot();
                    delta.wifiRxBytes = DigitDiffer.globalDiff(bgn.wifiRxBytes, end.wifiRxBytes);
                    delta.wifiTxBytes = DigitDiffer.globalDiff(bgn.wifiTxBytes, end.wifiTxBytes);
                    delta.mobileRxBytes = DigitDiffer.globalDiff(bgn.mobileRxBytes, end.mobileRxBytes);
                    delta.mobileTxBytes = DigitDiffer.globalDiff(bgn.mobileTxBytes, end.mobileTxBytes);
                    return delta;
                }
            };
        }
    }

    public static final class DevStatSnapshot extends Snapshot<DevStatSnapshot> {
        public Entry.DigitEntry<Long> uptime = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> chargingRatio = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> screenOff = Entry.DigitEntry.of(0L);

        DevStatSnapshot() {}

        @Override
        public Delta<DevStatSnapshot> diff(DevStatSnapshot bgn) {
            return new Delta<DevStatSnapshot>(bgn, this) {
                @Override
                protected DevStatSnapshot computeDelta() {
                    DevStatSnapshot delta = new DevStatSnapshot();
                    delta.uptime = Differ.DigitDiffer.globalDiff(bgn.uptime, end.uptime);
                    delta.chargingRatio = Differ.DigitDiffer.globalDiff(bgn.chargingRatio, end.chargingRatio);
                    delta.screenOff = Differ.DigitDiffer.globalDiff(bgn.screenOff, end.screenOff);
                    return delta;
                }
            };
        }
    }
}
