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
}
