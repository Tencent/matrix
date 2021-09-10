package com.tencent.matrix.batterycanary.monitor.feature;

import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.DigitEntry;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.ListEntry;
import com.tencent.matrix.batterycanary.utils.KernelCpuSpeedReader;
import com.tencent.matrix.batterycanary.utils.PowerProfile;
import com.tencent.matrix.util.MatrixLog;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * @author Kaede
 * @since 2021/9/10
 */
public class CpuStatFeature extends  AbsTaskMonitorFeature {
    private static final String TAG = "Matrix.battery.CpuStatFeature";
    private PowerProfile mPowerProfile;

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    public int weight() {
        return 0;
    }

    @Override
    public void onTurnOn() {
        super.onTurnOn();
        try {
            mPowerProfile = PowerProfile.init(mCore.getContext());
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public boolean isSupported() {
        return mPowerProfile != null;
    }

    public PowerProfile getPowerProfile() {
        return mPowerProfile;
    }

    public CpuStateSnapshot currentCpuStateSnapshot() {
        CpuStateSnapshot snapshot = new CpuStateSnapshot();
        try {
            if (!isSupported()) {
                throw new IOException("PowerProfile not supported");
            }
            snapshot.cpuCoreStates = new ArrayList<>();
            for (int i = 0; i < mPowerProfile.getCpuCoreNum(); i++) {
                final int numSpeedSteps = mPowerProfile.getNumSpeedStepsInCpuCluster(mPowerProfile.getClusterByCpuNum(i));
                KernelCpuSpeedReader reader = new KernelCpuSpeedReader(i, numSpeedSteps);
                long[] state = reader.readAbsolute();
                ListEntry<DigitEntry<Long>> cpuCoreState = ListEntry.ofDigits(state);
                snapshot.cpuCoreStates.add(cpuCoreState);
            }
        } catch (Exception e) {
            MatrixLog.w(TAG, "Read cpu core state fail: " + e.getMessage());
            snapshot.setValid(false);
        }
        return snapshot;
    }

    public static final class CpuStateSnapshot extends Snapshot<CpuStateSnapshot> {
        /*
         * cpuCoreStates
         * [
         *     [step1Jiffies, step2Jiffies ...], // CpuCore 1
         *     [step1Jiffies, step2Jiffies ...], // CpuCore 2
         *                                          ...
         * ]
         */
        public List<ListEntry<DigitEntry<Long>>> cpuCoreStates = Collections.emptyList();

        public long totalJiffies() {
            long sum = 0;
            for (ListEntry<DigitEntry<Long>> cpuCoreState : cpuCoreStates) {
                for (DigitEntry<Long> item : cpuCoreState.getList()) {
                    sum += item.value;
                }
            }
            return sum;
        }

        CpuStateSnapshot() {
        }

        @Override
        public Delta<CpuStateSnapshot> diff(CpuStateSnapshot bgn) {
            return new Delta<CpuStateSnapshot>(bgn, this) {
                @Override
                protected CpuStateSnapshot computeDelta() {
                    CpuStateSnapshot delta = new CpuStateSnapshot();
                    if (bgn.cpuCoreStates.size() != end.cpuCoreStates.size()) {
                        delta.setValid(false);
                    } else {
                        delta.cpuCoreStates = new ArrayList<>();
                        for (int i = 0; i < end.cpuCoreStates.size(); i++) {
                            delta.cpuCoreStates.add(Differ.ListDiffer.globalDiff(bgn.cpuCoreStates.get(i), end.cpuCoreStates.get(i)));
                        }
                    }
                    return delta;
                }
            };
        }
    }
}
