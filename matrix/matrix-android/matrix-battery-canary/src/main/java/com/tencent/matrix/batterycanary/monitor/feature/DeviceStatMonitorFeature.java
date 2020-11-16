package com.tencent.matrix.batterycanary.monitor.feature;

import android.content.Context;
import android.support.annotation.NonNull;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.util.MatrixLog;

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

    @NonNull
    private BatteryMonitorCore mMonitor;

    @Override
    public void configure(BatteryMonitorCore monitor) {
        MatrixLog.i(TAG, "#configure monitor feature");
        mMonitor = monitor;
    }

    @Override
    public void onTurnOn() {
    }

    @Override
    public void onTurnOff() {

    }

    @Override
    public void onForeground(boolean isForeground) {
    }

    @Override
    public int weight() {
        return Integer.MIN_VALUE;
    }

    public CpuFreqSnapshot currentCpuFreq() {
        CpuFreqSnapshot snapshot = new CpuFreqSnapshot();
        try {
            snapshot.cpuFreq = BatteryCanaryUtil.getCpuCurrentFreq();
            snapshot.cpuFreqs = Snapshot.Entry.ListEntry.ofDigits(BatteryCanaryUtil.getCpuCurrentFreq());
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "#currentCpuFreq error");
            snapshot.cpuFreq = new int[0];
        }
        return snapshot;
    }

    public BatteryTmpSnapshot currentBatteryTemperature(Context context) {
        BatteryTmpSnapshot snapshot = new BatteryTmpSnapshot();
        snapshot.temperature = mMonitor.getCurrentBatteryTemperature(context);
        snapshot.temp = Snapshot.Entry.DigitEntry.of(mMonitor.getCurrentBatteryTemperature(context));
        return snapshot;
    }

    public static class CpuFreqSnapshot extends Snapshot<CpuFreqSnapshot> {
        public int[] cpuFreq;
        public Entry.ListEntry<Entry.DigitEntry<Integer>> cpuFreqs;

        @Override
        public Delta<CpuFreqSnapshot> diff(CpuFreqSnapshot bgn) {
            return new Delta<CpuFreqSnapshot>(bgn, this) {
                @Override
                protected CpuFreqSnapshot computeDelta() {
                    CpuFreqSnapshot delta = new CpuFreqSnapshot();
                    delta.cpuFreq = DifferLegacy.sDigitArrayDiffer.diff(bgn.cpuFreq, end.cpuFreq);
                    delta.cpuFreqs = Differ.ListDiffer.globalDiff(bgn.cpuFreqs, end.cpuFreqs);
                    return delta;
                }
            };
        }
    }

    public static class BatteryTmpSnapshot extends Snapshot<BatteryTmpSnapshot> {
        public int temperature;
        public Entry.DigitEntry<Integer> temp;

        @Override
        public Delta<BatteryTmpSnapshot>  diff(BatteryTmpSnapshot bgn) {
            return new Delta<BatteryTmpSnapshot>(bgn, this) {
                @Override
                protected BatteryTmpSnapshot computeDelta() {
                    BatteryTmpSnapshot delta = new BatteryTmpSnapshot();
                    delta.temperature = DifferLegacy.sDigitDiffer.diffInt(bgn.temperature, end.temperature);
                    delta.temp = Differ.DigitDiffer.globalDiff(bgn.temp, end.temp);
                    return delta;
                }
            };
        }
    }
}
