package com.tencent.matrix.batterycanary.monitor.feature;

import android.os.Parcel;
import android.os.Parcelable;
import android.os.Process;

import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.DigitEntry;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.ListEntry;
import com.tencent.matrix.batterycanary.shell.TopThreadFeature;
import com.tencent.matrix.batterycanary.shell.ui.TopThreadIndicator;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.KernelCpuSpeedReader;
import com.tencent.matrix.batterycanary.utils.KernelCpuUidFreqTimeReader;
import com.tencent.matrix.batterycanary.utils.PowerProfile;
import com.tencent.matrix.batterycanary.utils.ProcStatUtil;
import com.tencent.matrix.util.MatrixLog;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import androidx.annotation.WorkerThread;
import androidx.core.util.Pair;

import static com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil.JIFFY_MILLIS;
import static com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil.ONE_HOR;

/**
 * @author Kaede
 * @since 2021/9/10
 */
@SuppressWarnings("SpellCheckingInspection")
public class CpuStatFeature extends AbsTaskMonitorFeature {
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
        tryInitPowerProfile();
    }

    @Override
    public void onForeground(boolean isForeground) {
        super.onForeground(isForeground);
        if (!isForeground) {
            if (mPowerProfile == null) {
                mCore.getHandler().post(new Runnable() {
                    @Override
                    public void run() {
                        tryInitPowerProfile();
                    }
                });
            }
        }
    }

    @WorkerThread
    private void tryInitPowerProfile() {
        if (mPowerProfile != null) {
            return;
        }
        synchronized (this) {
            if (mPowerProfile != null) {
                return;
            }
            try {
                // Check PowerProfile compat
                mPowerProfile = PowerProfile.init(mCore.getContext());
                // Check KernelCpuSpeedReader compat
                for (int i = 0; i < mPowerProfile.getCpuCoreNum(); i++) {
                    final int numSpeedSteps = mPowerProfile.getNumSpeedStepsInCpuCluster(mPowerProfile.getClusterByCpuNum(i));
                    new KernelCpuSpeedReader(i, numSpeedSteps).smoke();
                }
                // Check KernelCpuUidFreqTimeReader compat
                int[] clusterSteps = new int[mPowerProfile.getNumCpuClusters()];
                for (int i = 0; i < clusterSteps.length; i++) {
                    clusterSteps[i] = mPowerProfile.getNumSpeedStepsInCpuCluster(i);
                }
                new KernelCpuUidFreqTimeReader(Process.myPid(), clusterSteps).smoke();
            } catch (IOException e) {
                MatrixLog.w(TAG, "Init cpuStat failed: " + e.getMessage());
                mPowerProfile = null;
            }
        }
    }

    public boolean isSupported() {
        return mPowerProfile != null;
    }

    public PowerProfile getPowerProfile() {
        return mPowerProfile;
    }

    public CpuStateSnapshot currentCpuStateSnapshot() {
        return currentCpuStateSnapshot(Process.myPid());
    }

    public CpuStateSnapshot currentCpuStateSnapshot(int pid) {
        CpuStateSnapshot snapshot = new CpuStateSnapshot();
        try {
            if (!isSupported()) {
                throw new IOException("PowerProfile not supported");
            }
            synchronized (this) {
                if (!isSupported()) {
                    throw new IOException("PowerProfile not supported");
                }
                // Cpu core steps jiffies
                if (pid == Process.myPid()) {
                    snapshot.cpuCoreStates = new ArrayList<>();
                    for (int i = 0; i < mPowerProfile.getCpuCoreNum(); i++) {
                        final int numSpeedSteps = mPowerProfile.getNumSpeedStepsInCpuCluster(mPowerProfile.getClusterByCpuNum(i));
                        KernelCpuSpeedReader cpuStepJiffiesReader = new KernelCpuSpeedReader(i, numSpeedSteps);
                        long[] cpuCoreStepJiffies = cpuStepJiffiesReader.readAbsolute();
                        ListEntry<DigitEntry<Long>> cpuCoreState = ListEntry.ofDigits(cpuCoreStepJiffies);
                        snapshot.cpuCoreStates.add(cpuCoreState);
                    }
                }
                // Proc cluster steps jiffies
                int[] clusterSteps = new int[mPowerProfile.getNumCpuClusters()];
                for (int i = 0; i < clusterSteps.length; i++) {
                    clusterSteps[i] = mPowerProfile.getNumSpeedStepsInCpuCluster(i);
                }
                KernelCpuUidFreqTimeReader procStepJiffiesReader = new KernelCpuUidFreqTimeReader(pid, clusterSteps);
                List<long[]> procStepJiffies = procStepJiffiesReader.readAbsolute();
                snapshot.procCpuCoreStates = new ArrayList<>();
                for (long[] item : procStepJiffies) {
                    ListEntry<DigitEntry<Long>> procCpuCoreState = ListEntry.ofDigits(item);
                    snapshot.procCpuCoreStates.add(procCpuCoreState);
                }
            }
        } catch (Exception e) {
            MatrixLog.w(TAG, "Read cpu core state fail: " + e.getMessage());
            snapshot.setValid(false);
        }
        return snapshot;
    }

    public UidCpuStateSnapshot currentUidCpuStateSnapshot() {
        UidCpuStateSnapshot curr = new UidCpuStateSnapshot();
        try {
            List<Pair<Integer, String>> procList = TopThreadFeature.getProcList(mCore.getContext());
            curr.pidCurrCupSateList = new ArrayList<>(procList.size());

            for (Pair<Integer, String> item : procList) {
                //noinspection ConstantConditions
                int pid = item.first;
                String procName = String.valueOf(item.second);
                CpuStateSnapshot snapshot = null;

                if (pid == Process.myPid()) {
                    // from local
                    snapshot = currentCpuStateSnapshot();
                } else {
                    if (ProcStatUtil.exists(pid)) {
                        // from pid
                        snapshot = currentCpuStateSnapshot(pid);
                    }
                    if (snapshot != null && !snapshot.isValid() && mCore.getConfig().ipcCpuStatCollector != null) {
                        // from ipc
                        UidCpuStateSnapshot.IpcCpuStat.RemoteStat remote = mCore.getConfig().ipcCpuStatCollector.apply(item);
                        if (remote != null) {
                            snapshot = UidCpuStateSnapshot.IpcCpuStat.toLocal(remote);
                        }
                    }
                }
                if (snapshot != null) {
                    snapshot.pid = pid;
                    snapshot.name = TopThreadIndicator.getProcSuffix(procName);
                    curr.pidCurrCupSateList.add(snapshot);
                }
            }
        } catch (Exception e) {
            MatrixLog.w(TAG, "get curr UidCpuStatSnapshot failed: " + e.getMessage());
            curr.setValid(false);
        }
        return curr;
    }

    public static final class CpuStateSnapshot extends Snapshot<CpuStateSnapshot> {
        /*
         * cpuCoreStates
         * [
         *     [step1Jiffies, step2Jiffies ...], // CpuCore 1
         *     [step1Jiffies, step2Jiffies ...], // CpuCore 2
         *                                          ...
         * ]
         *
         * procCpuCoreStates
         * [
         *     [step1Jiffies, step2Jiffies ...], // Cluster 1
         *     [step1Jiffies, step2Jiffies ...], // Cluster 2
         *                                          ...
         * ]
         */
        public List<ListEntry<DigitEntry<Long>>> cpuCoreStates = Collections.emptyList();
        public List<ListEntry<DigitEntry<Long>>> procCpuCoreStates = Collections.emptyList();
        public int pid = Process.myPid();
        public String name = BatteryCanaryUtil.getProcessName();

        public long totalCpuJiffies() {
            long sum = 0;
            for (ListEntry<DigitEntry<Long>> cpuCoreState : cpuCoreStates) {
                for (DigitEntry<Long> item : cpuCoreState.getList()) {
                    sum += item.value;
                }
            }
            return sum;
        }

        public long totalProcCpuJiffies() {
            long sum = 0;
            for (ListEntry<DigitEntry<Long>> cpuCoreState : procCpuCoreStates) {
                for (DigitEntry<Long> item : cpuCoreState.getList()) {
                    sum += item.value;
                }
            }
            return sum;
        }

        public double configureCpuSip(PowerProfile powerProfile) {
            if (!powerProfile.isSupported()) {
                return 0;
            }
            double sipSum = 0;
            for (int i = 0; i < cpuCoreStates.size(); i++) {
                List<DigitEntry<Long>> stepJiffies = cpuCoreStates.get(i).getList();
                for (int j = 0; j < stepJiffies.size(); j++) {
                    double jiffy = stepJiffies.get(j).get();
                    int cluster = powerProfile.getClusterByCpuNum(i);
                    double power = powerProfile.getAveragePowerForCpuCore(cluster, j);
                    double sip = power * (jiffy * JIFFY_MILLIS / ONE_HOR);
                    sipSum += sip;
                }
            }
            return sipSum;
        }

        public double configureProcSip(PowerProfile powerProfile, long procJiffies) {
            if (!powerProfile.isSupported()) {
                return 0;
            }
            long jiffySum = 0;
            for (ListEntry<DigitEntry<Long>> stepJiffies : procCpuCoreStates) {
                for (DigitEntry<Long> item : stepJiffies.getList()) {
                    jiffySum += item.get();
                }
            }
            double sipSum = 0;
            for (int i = 0; i < procCpuCoreStates.size(); i++) {
                List<DigitEntry<Long>> stepJiffies = procCpuCoreStates.get(i).getList();
                for (int j = 0; j < stepJiffies.size(); j++) {
                    long jiffy = stepJiffies.get(j).get();
                    double figuredJiffies = ((double) jiffy / jiffySum) * procJiffies;
                    double power = powerProfile.getAveragePowerForCpuCore(i, j);
                    double sip = power * (figuredJiffies * JIFFY_MILLIS / ONE_HOR);
                    sipSum += sip;
                }
            }
            return sipSum;
        }

        CpuStateSnapshot() {
        }

        @Override
        public Delta<CpuStateSnapshot> diff(CpuStateSnapshot bgn) {
            return new Delta<CpuStateSnapshot>(bgn, this) {
                @Override
                protected CpuStateSnapshot computeDelta() {
                    CpuStateSnapshot delta = new CpuStateSnapshot();
                    delta.pid = end.pid;
                    delta.name = end.name;
                    if (bgn.cpuCoreStates.size() != end.cpuCoreStates.size()) {
                        delta.setValid(false);
                    } else {
                        delta.cpuCoreStates = new ArrayList<>();
                        for (int i = 0; i < end.cpuCoreStates.size(); i++) {
                            delta.cpuCoreStates.add(Differ.ListDiffer.globalDiff(bgn.cpuCoreStates.get(i), end.cpuCoreStates.get(i)));
                        }
                        delta.procCpuCoreStates = new ArrayList<>();
                        for (int i = 0; i < end.procCpuCoreStates.size(); i++) {
                            delta.procCpuCoreStates.add(Differ.ListDiffer.globalDiff(bgn.procCpuCoreStates.get(i), end.procCpuCoreStates.get(i)));
                        }
                    }
                    return delta;
                }
            };
        }
    }

    public static final class UidCpuStateSnapshot extends MonitorFeature.Snapshot<UidCpuStateSnapshot> {
        public List<CpuStateSnapshot> pidCurrCupSateList = Collections.emptyList();
        public List<MonitorFeature.Snapshot.Delta<CpuStateSnapshot>> pidDeltaCpuSateList = Collections.emptyList();

        @Override
        public MonitorFeature.Snapshot.Delta<UidCpuStateSnapshot> diff(UidCpuStateSnapshot bgn) {
            return new MonitorFeature.Snapshot.Delta<UidCpuStateSnapshot>(bgn, this) {
                @Override
                protected UidCpuStateSnapshot computeDelta() {
                    UidCpuStateSnapshot delta = new UidCpuStateSnapshot();
                    if (end.pidCurrCupSateList.size() > 0) {
                        delta.pidDeltaCpuSateList = new ArrayList<>();
                        for (CpuStateSnapshot end : end.pidCurrCupSateList) {
                            CpuStateSnapshot last = null;
                            for (CpuStateSnapshot bgn : bgn.pidCurrCupSateList) {
                                if (bgn.pid == end.pid) {
                                    last = bgn;
                                    break;
                                }
                            }
                            if (last == null) {
                                // newAdded Pid
                                CpuStateSnapshot empty = new CpuStateSnapshot();
                                empty.pid = end.pid;
                                empty.name = end.name;
                                empty.procCpuCoreStates = new ArrayList<>(end.procCpuCoreStates.size());
                                for (ListEntry<DigitEntry<Long>> item : end.procCpuCoreStates) {
                                    long[] emptyStats = new long[item.getList().size()];
                                    empty.procCpuCoreStates.add(ListEntry.ofDigits(emptyStats));
                                }
                                last = empty;
                            }
                            MonitorFeature.Snapshot.Delta<CpuStateSnapshot> deltaPidCpuState = end.diff(last);
                            delta.pidDeltaCpuSateList.add(deltaPidCpuState);
                        }
                    }
                    return delta;
                }
            };
        }


        public static class IpcCpuStat {
            public static RemoteStat toIpc(CpuStateSnapshot local) {
                RemoteStat remote = new RemoteStat();
                remote.procCpuCoreStates = new ArrayList<>(local.procCpuCoreStates.size());
                for (ListEntry<DigitEntry<Long>> item : local.procCpuCoreStates) {
                    long[] stats = new long[item.getList().size()];
                    for (int i = 0; i < stats.length; i++) {
                        stats[i] = item.getList().get(i).get();
                    }
                    remote.procCpuCoreStates.add(stats);
                }
                return remote;
            }

            public static CpuStateSnapshot toLocal(RemoteStat remote) {
                CpuStateSnapshot local = new CpuStateSnapshot();
                local.procCpuCoreStates = new ArrayList<>(remote.procCpuCoreStates.size());
                for (long[] item : remote.procCpuCoreStates) {
                    local.procCpuCoreStates.add(ListEntry.ofDigits(item));
                }
                return local;
            }

            public static class RemoteStat implements Parcelable {
                public List<long[]> procCpuCoreStates = Collections.emptyList();

                public RemoteStat() {
                }

                protected RemoteStat(Parcel in) {
                    int size = in.readInt();
                    procCpuCoreStates = new ArrayList<>(size);
                    for (int i = 0; i < size; i++) {
                        procCpuCoreStates.add(in.createLongArray());
                    }
                }

                @Override
                public void writeToParcel(Parcel dest, int flags) {
                    int numOfArrays = procCpuCoreStates.size();
                    dest.writeInt(numOfArrays);
                    for (int i = 0; i < numOfArrays; i++) {
                        dest.writeLongArray(procCpuCoreStates.get(i));
                    }
                }

                @Override
                public int describeContents() {
                    return 0;
                }

                public static final Creator<RemoteStat> CREATOR = new Creator<RemoteStat>() {
                    @Override
                    public RemoteStat createFromParcel(Parcel in) {
                        return new RemoteStat(in);
                    }
                    @Override
                    public RemoteStat[] newArray(int size) {
                        return new RemoteStat[size];
                    }
                };
            }
        }
    }
}
