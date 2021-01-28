package com.tencent.matrix.batterycanary.monitor.feature;

import android.annotation.SuppressLint;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.support.annotation.NonNull;
import android.support.annotation.VisibleForTesting;
import android.support.v4.util.Consumer;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.AppStatMonitorFeature.AppStatStamp;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Differ.DigitDiffer;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Differ.ListDiffer;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.TimeBreaker;
import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

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
public final class DeviceStatMonitorFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.DeviceStatusMonitorFeature";

    @NonNull private DevStatListener mDevStatListener;
    @NonNull List<TimeBreaker.Stamp> mStampList = Collections.emptyList();
    @NonNull Runnable coolingTask = new Runnable() {
        @Override
        public void run() {
            if (mStampList.size() >= mCore.getConfig().overHeatCount) {
                synchronized (TAG) {
                    TimeBreaker.gcList(mStampList);
                }
            }
        }
    };

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    public void configure(BatteryMonitorCore monitor) {
        super.configure(monitor);
        mDevStatListener = new DevStatListener();
    }

    @Override
    public void onTurnOn() {
        super.onTurnOn();
        int deviceStat = BatteryCanaryUtil.getDeviceStat(mCore.getContext());
        @SuppressLint("VisibleForTests") AppStatStamp firstStamp = new AppStatStamp(deviceStat);
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
                        mStampList.add(0, new TimeBreaker.Stamp(String.valueOf(integer)));
                        checkOverHeat();
                    }
                }
            }
        });

        mDevStatListener.startListen(mCore.getContext());
    }

    @Override
    public void onTurnOff() {
        super.onTurnOff();
        mDevStatListener.stopListen(mCore.getContext());
    }

    @Override
    public void onForeground(boolean isForeground) {
        super.onForeground(isForeground);
        if (!isForeground) {
            if (!mDevStatListener.isListening()) {
                mDevStatListener.startListen(mCore.getContext());
            }
        }
    }

    private void checkOverHeat() {
        mCore.getHandler().removeCallbacks(coolingTask);
        mCore.getHandler().postDelayed(coolingTask, 1000L);
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
        snapshot.temp = Snapshot.Entry.DigitEntry.of(mCore.getCurrentBatteryTemperature(context));
        return snapshot;
    }

    public DevStatSnapshot currentDevStatSnapshot() {
        return currentDevStatSnapshot(0L);
    }

    public DevStatSnapshot currentDevStatSnapshot(long windowMillis) {
        try {
            int devStat = BatteryCanaryUtil.getDeviceStat(mCore.getContext());
            @SuppressLint("VisibleForTests") TimeBreaker.Stamp lastStamp = new TimeBreaker.Stamp(String.valueOf(devStat));
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
    static DevStatSnapshot configureSnapshot(List<TimeBreaker.Stamp> stampList, long windowMillis) {
        TimeBreaker.TimePortions timePortions = TimeBreaker.configurePortions(stampList, windowMillis);
        DevStatSnapshot snapshot = new DevStatSnapshot();
        snapshot.setValid(timePortions.isValid());
        snapshot.uptime = Snapshot.Entry.DigitEntry.of(timePortions.totalUptime);
        snapshot.chargingRatio = Snapshot.Entry.DigitEntry.of((long) timePortions.getRatio("1"));
        snapshot.unChargingRatio = Snapshot.Entry.DigitEntry.of((long) timePortions.getRatio("2"));
        snapshot.screenOff = Snapshot.Entry.DigitEntry.of((long) timePortions.getRatio("3"));
        snapshot.lowEnergyRatio = Snapshot.Entry.DigitEntry.of((long) timePortions.getRatio("4"));
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

    public static final class DevStatSnapshot extends Snapshot<DevStatSnapshot> {
        public Entry.DigitEntry<Long> uptime = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> chargingRatio = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> unChargingRatio = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> screenOff = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> lowEnergyRatio = Entry.DigitEntry.of(0L);

        DevStatSnapshot() {}

        @Override
        public Delta<DevStatSnapshot> diff(DevStatSnapshot bgn) {
            return new Delta<DevStatSnapshot>(bgn, this) {
                @Override
                protected DevStatSnapshot computeDelta() {
                    DevStatSnapshot delta = new DevStatSnapshot();
                    delta.uptime = Differ.DigitDiffer.globalDiff(bgn.uptime, end.uptime);
                    delta.chargingRatio = Differ.DigitDiffer.globalDiff(bgn.chargingRatio, end.chargingRatio);
                    delta.unChargingRatio = Differ.DigitDiffer.globalDiff(bgn.unChargingRatio, end.unChargingRatio);
                    delta.screenOff = Differ.DigitDiffer.globalDiff(bgn.screenOff, end.screenOff);
                    delta.lowEnergyRatio = Differ.DigitDiffer.globalDiff(bgn.lowEnergyRatio, end.lowEnergyRatio);
                    return delta;
                }
            };
        }
    }
}
