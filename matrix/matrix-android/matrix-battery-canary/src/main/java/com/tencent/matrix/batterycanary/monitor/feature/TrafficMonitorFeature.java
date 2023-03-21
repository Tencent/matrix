package com.tencent.matrix.batterycanary.monitor.feature;

import android.content.Context;
import androidx.annotation.Nullable;

import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Differ.DigitDiffer;
import com.tencent.matrix.batterycanary.utils.RadioStatUtil;

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
public final class TrafficMonitorFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.TrafficMonitorFeature";

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    public int weight() {
        return Integer.MIN_VALUE;
    }

    @Nullable
    public TrafficMonitorFeature.RadioStatSnapshot currentRadioSnapshot(Context context) {
        RadioStatUtil.RadioStat stat = RadioStatUtil.getCurrentStat(context);
        if (stat == null) {
            return null;
        }

        TrafficMonitorFeature.RadioStatSnapshot snapshot = new TrafficMonitorFeature.RadioStatSnapshot();
        snapshot.wifiRxBytes = Snapshot.Entry.DigitEntry.of(stat.wifiRxBytes);
        snapshot.wifiTxBytes = Snapshot.Entry.DigitEntry.of(stat.wifiTxBytes);
        snapshot.wifiRxPackets = Snapshot.Entry.DigitEntry.of(stat.wifiRxPackets);
        snapshot.wifiTxPackets = Snapshot.Entry.DigitEntry.of(stat.wifiTxPackets);

        snapshot.mobileRxBytes = Snapshot.Entry.DigitEntry.of(stat.mobileRxBytes);
        snapshot.mobileTxBytes = Snapshot.Entry.DigitEntry.of(stat.mobileTxBytes);
        snapshot.mobileRxPackets = Snapshot.Entry.DigitEntry.of(stat.mobileRxPackets);
        snapshot.mobileTxPackets = Snapshot.Entry.DigitEntry.of(stat.mobileTxPackets);
        return snapshot;
    }

    @Nullable
    public TrafficMonitorFeature.RadioBpsSnapshot currentRadioBpsSnapshot(Context context) {
        RadioStatUtil.RadioBps stat = RadioStatUtil.getCurrentBps(context);
        if (stat == null) {
            return null;
        }

        TrafficMonitorFeature.RadioBpsSnapshot snapshot = new TrafficMonitorFeature.RadioBpsSnapshot();
        snapshot.wifiRxBps = Snapshot.Entry.DigitEntry.of(stat.wifiRxBps);
        snapshot.wifiTxBps = Snapshot.Entry.DigitEntry.of(stat.wifiTxBps);
        snapshot.mobileRxBps = Snapshot.Entry.DigitEntry.of(stat.mobileRxBps);
        snapshot.mobileTxBps = Snapshot.Entry.DigitEntry.of(stat.mobileTxBps);
        return snapshot;
    }

    public static class RadioStatSnapshot extends Snapshot<RadioStatSnapshot> {
        public Entry.DigitEntry<Long> wifiRxBytes = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> wifiTxBytes = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> wifiRxPackets = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> wifiTxPackets = Entry.DigitEntry.of(0L);

        public Entry.DigitEntry<Long> mobileRxBytes = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> mobileTxBytes = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> mobileRxPackets = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> mobileTxPackets = Entry.DigitEntry.of(0L);

        @Override
        public Delta<RadioStatSnapshot> diff(RadioStatSnapshot bgn) {
            return new Delta<RadioStatSnapshot>(bgn, this) {
                @Override
                protected RadioStatSnapshot computeDelta() {
                    RadioStatSnapshot delta = new RadioStatSnapshot();
                    delta.wifiRxBytes = DigitDiffer.globalDiff(bgn.wifiRxBytes, end.wifiRxBytes);
                    delta.wifiTxBytes = DigitDiffer.globalDiff(bgn.wifiTxBytes, end.wifiTxBytes);
                    delta.wifiRxPackets = DigitDiffer.globalDiff(bgn.wifiRxPackets, end.wifiRxPackets);
                    delta.wifiTxPackets = DigitDiffer.globalDiff(bgn.wifiTxPackets, end.wifiTxPackets);

                    delta.mobileRxBytes = DigitDiffer.globalDiff(bgn.mobileRxBytes, end.mobileRxBytes);
                    delta.mobileTxBytes = DigitDiffer.globalDiff(bgn.mobileTxBytes, end.mobileTxBytes);
                    delta.mobileRxPackets = DigitDiffer.globalDiff(bgn.mobileRxPackets, end.mobileRxPackets);
                    delta.mobileTxPackets = DigitDiffer.globalDiff(bgn.mobileTxPackets, end.mobileTxPackets);
                    return delta;
                }
            };
        }
    }

    public static class RadioBpsSnapshot extends Snapshot<RadioBpsSnapshot> {
        public Entry.DigitEntry<Long> wifiRxBps = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> wifiTxBps = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> mobileRxBps = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> mobileTxBps = Entry.DigitEntry.of(0L);

        @Override
        public Delta<RadioBpsSnapshot> diff(RadioBpsSnapshot bgn) {
            return new Delta<RadioBpsSnapshot>(bgn, this) {
                @Override
                protected RadioBpsSnapshot computeDelta() {
                    RadioBpsSnapshot delta = new RadioBpsSnapshot();
                    delta.wifiRxBps = DigitDiffer.globalDiff(bgn.wifiRxBps, end.wifiRxBps);
                    delta.wifiTxBps = DigitDiffer.globalDiff(bgn.wifiTxBps, end.wifiTxBps);
                    delta.mobileRxBps = DigitDiffer.globalDiff(bgn.mobileRxBps, end.mobileRxBps);
                    delta.mobileTxBps = DigitDiffer.globalDiff(bgn.mobileTxBps, end.mobileTxBps);
                    return delta;
                }
            };
        }
    }
}
