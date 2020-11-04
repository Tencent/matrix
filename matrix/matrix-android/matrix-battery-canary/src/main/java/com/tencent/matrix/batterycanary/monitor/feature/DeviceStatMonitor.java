package com.tencent.matrix.batterycanary.monitor.feature;

import android.content.Context;
import android.os.Handler;
import android.support.annotation.NonNull;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.util.MatrixHandlerThread;
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
public final class DeviceStatMonitor implements MonitorFeature {
    private static final String TAG = "Matrix.monitor.DeviceStatusMonitorFeature";

    @NonNull
    private BatteryMonitorCore mMonitor;
    @NonNull private Handler mHandler;


    @Override
    public void configure(BatteryMonitorCore monitor) {
        MatrixLog.i(TAG, "#configure monitor feature");
        mMonitor = monitor;
        mHandler = new Handler(MatrixHandlerThread.getDefaultHandlerThread().getLooper());
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
        snapshot.cpuFreq = BatteryCanaryUtil.getCpuCurrentFreq();
        return snapshot;
    }

    public BatteryTmpSnapshot currentBatteryTemperature(Context context) {
        BatteryTmpSnapshot snapshot = new BatteryTmpSnapshot();
        snapshot.temperature = BatteryCanaryUtil.getBatteryTemperature(context);
        return snapshot;
    }

    public static class CpuFreqSnapshot extends Snapshot<CpuFreqSnapshot> {
        public int[] cpuFreq;

        @Override
        public Delta<CpuFreqSnapshot> diff(CpuFreqSnapshot bgn) {
            return new Delta<CpuFreqSnapshot>(bgn, this) {
                @Override
                protected CpuFreqSnapshot computeDelta() {
                    CpuFreqSnapshot delta = new CpuFreqSnapshot();
                    delta.cpuFreq = Differ.sDigitArrayDiffer.diff(bgn.cpuFreq, end.cpuFreq);
                    return delta;
                }
            };
        }
    }

    public static class BatteryTmpSnapshot extends Snapshot<BatteryTmpSnapshot> {
        int temperature;

        @Override
        public Delta<BatteryTmpSnapshot> diff(BatteryTmpSnapshot bgn) {
            return new Delta<BatteryTmpSnapshot>(bgn, this) {
                @Override
                protected BatteryTmpSnapshot computeDelta() {
                    BatteryTmpSnapshot delta = new BatteryTmpSnapshot();
                    delta.temperature = Differ.sDigitDiffer.diffInt(bgn.temperature, end.temperature);
                    return delta;
                }
            };
        }
    }
}
