package com.tencent.matrix.batterycanary.monitor;

import android.content.ComponentName;
import android.os.HandlerThread;
import android.os.Process;
import android.support.annotation.CallSuper;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.util.Consumer;
import android.util.LongSparseArray;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.monitor.feature.AlarmMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.AlarmMonitorFeature.AlarmSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.AppStatMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.BlueToothMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.BlueToothMonitorFeature.BlueToothSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature.BatteryTmpSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature.CpuFreqSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot.ThreadJiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.LooperTaskMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.batterycanary.monitor.feature.TrafficMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.TrafficMonitorFeature.RadioStatSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature.WakeLockSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature.WakeLockTrace.WakeLockRecord;
import com.tencent.matrix.batterycanary.monitor.feature.WifiMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WifiMonitorFeature.WifiSnapshot;
import com.tencent.matrix.util.MatrixLog;

import java.util.Arrays;
import java.util.List;

/**
 * @author Kaede
 * @since 2020/10/27
 */
public interface BatteryMonitorCallback extends
        BatteryMonitorCore.JiffiesListener,
        LooperTaskMonitorFeature.LooperTaskListener,
        WakeLockMonitorFeature.WakeLockListener,
        AlarmMonitorFeature.AlarmListener,
        JiffiesMonitorFeature.JiffiesListener,
        AppStatMonitorFeature.AppStatListener {

    @SuppressWarnings({"NotNullFieldNotInitialized", "SpellCheckingInspection", "unused"})
    class BatteryPrinter implements BatteryMonitorCallback {
        private static final String TAG = "Matrix.battery.BatteryPrinter";
        private static final int ONE_MIN = 60 * 1000;

        @NonNull
        private BatteryMonitorCore mMonitor;
        private final Printer mPrinter = new Printer();
        private final LongSparseArray<List<LooperTaskMonitorFeature.TaskTraceInfo>> tasks = new LongSparseArray<>();

        @Nullable protected AlarmMonitorFeature mAlarmFeat;
        @Nullable protected AppStatMonitorFeature mAppStatFeat;
        @Nullable protected BlueToothMonitorFeature mBlueToothFeat;
        @Nullable protected DeviceStatMonitorFeature mDevStatFeat;
        @Nullable protected JiffiesMonitorFeature mJiffiesFeat;
        @Nullable protected TrafficMonitorFeature mTrafficFeat;
        @Nullable protected WakeLockMonitorFeature mWakeLockFeat;
        @Nullable protected WifiMonitorFeature mWifiMonitorFeat;

        @Nullable protected AlarmSnapshot mLastAlarmSnapshot;
        @Nullable protected BlueToothSnapshot mLastBlueToothSnapshot;
        @Nullable protected BatteryTmpSnapshot mLastBatteryTmpSnapshot;
        @Nullable protected CpuFreqSnapshot mLastCpuFreqSnapshot;
        @Nullable protected JiffiesSnapshot mLastJiffiesSnapshot;
        @Nullable protected RadioStatSnapshot mLastTrafficSnapshot;
        @Nullable protected WakeLockSnapshot mLastWakeWakeLockSnapshot;
        @Nullable protected WifiSnapshot mLastWifiSnapshot;

        @SuppressWarnings("UnusedReturnValue")
        final BatteryPrinter attach(BatteryMonitorCore monitorCore) {
            mMonitor = monitorCore;
            return this;
        }

        @NonNull
        protected BatteryMonitorCore getMonitor() {
            return mMonitor;
        }

        @CallSuper
        @Override
        public void onTraceBegin() {
            // Configure begin snapshots
            mAlarmFeat = mMonitor.getMonitorFeature(AlarmMonitorFeature.class);
            if (mAlarmFeat != null) {
                mLastAlarmSnapshot = mAlarmFeat.currentAlarms();
            }

            mAppStatFeat = mMonitor.getMonitorFeature(AppStatMonitorFeature.class);

            mBlueToothFeat = mMonitor.getMonitorFeature(BlueToothMonitorFeature.class);
            if (mBlueToothFeat != null) {
                mLastBlueToothSnapshot = mBlueToothFeat.currentSnapshot();
            }

            mDevStatFeat = mMonitor.getMonitorFeature(DeviceStatMonitorFeature.class);
            if (mDevStatFeat != null) {
                mLastCpuFreqSnapshot = mDevStatFeat.currentCpuFreq();
                mLastBatteryTmpSnapshot = mDevStatFeat.currentBatteryTemperature(mMonitor.getContext());
            }

            mJiffiesFeat = mMonitor.getMonitorFeature(JiffiesMonitorFeature.class);
            if (mJiffiesFeat != null) {
                mLastJiffiesSnapshot = mJiffiesFeat.currentJiffiesSnapshot();
            }

            mTrafficFeat = mMonitor.getMonitorFeature(TrafficMonitorFeature.class);
            if (mTrafficFeat != null) {
                mLastTrafficSnapshot = mTrafficFeat.currentRadioSnapshot(mMonitor.getContext());
            }

            mWakeLockFeat = mMonitor.getMonitorFeature(WakeLockMonitorFeature.class);
            if (mWakeLockFeat != null) {
                mLastWakeWakeLockSnapshot = mWakeLockFeat.currentWakeLocks();
            }

            mWifiMonitorFeat = mMonitor.getMonitorFeature(WifiMonitorFeature.class);
            if (mWifiMonitorFeat != null) {
                mLastWifiSnapshot = mWifiMonitorFeat.currentSnapshot();
            }
        }

        @CallSuper
        @Override
        public void onTraceEnd(boolean isForeground) {
            onCanaryDump(isForeground);
        }

        @Override
        public void onReportInternalJiffies(Delta<BatteryMonitorCore.TaskJiffiesSnapshot> delta) {
        }

        @Override
        public void onTaskTrace(Thread thread, List<LooperTaskMonitorFeature.TaskTraceInfo> sortList) {
            if (thread instanceof HandlerThread) {
                synchronized (tasks) {
                    tasks.put(((HandlerThread) thread).getThreadId(), sortList);
                }
            }
        }

        @Override
        public void onWakeLockTimeout(int warningCount, WakeLockRecord record) {
        }

        @Override
        public void onWakeLockTimeout(WakeLockRecord record, long backgroundMillis) {
        }

        @Override
        public void onAlarmDuplicated(int duplicatedCount, AlarmMonitorFeature.AlarmRecord record) {
        }

        @Override
        public void onParseError(int pid, int tid) {
        }

        @Override
        public void onForegroundServiceLeak(boolean isMyself, int appImportance, int globalAppImportance, ComponentName componentName, long millis) {
        }

        @Override
        public void onAppSateLeak(boolean isMyself, int appImportance, ComponentName componentName, long millis) {
        }

        @CallSuper
        protected void onCanaryDump(boolean isForeground) {
            mPrinter.clear();

            // title
            mPrinter.writeTitle();
            JiffiesMonitorFeature jiffiesFeautre = mMonitor.getMonitorFeature(JiffiesMonitorFeature.class);
            if (null != jiffiesFeautre && null != mLastJiffiesSnapshot) {
                JiffiesSnapshot curr = jiffiesFeautre.currentJiffiesSnapshot();
                Delta<JiffiesSnapshot> delta = curr.diff(mLastJiffiesSnapshot);
                onReportJiffies(delta);

                // header
                long avgJiffies = delta.dlt.totalJiffies.get() / Math.max(1, delta.during / ONE_MIN);
                mPrinter.append("| ").append("pid=").append(Process.myPid())
                        .tab().tab().append("fg=").append(isForeground)
                        .tab().tab().append("during(min)=").append(delta.during / ONE_MIN)
                        .tab().tab().append("diff(jiffies)=").append(delta.dlt.totalJiffies.get())
                        .tab().tab().append("avg(jiffies/min)=").append(avgJiffies)
                        .enter();

                // jiffies sections
                mPrinter.createSection("jiffies(" + delta.dlt.threadEntries.getList().size() + ")");
                for (ThreadJiffiesSnapshot threadJiffies : delta.dlt.threadEntries.getList().subList(0, Math.min(delta.dlt.threadEntries.getList().size(), 8))) {
                    mPrinter.append("|   -> (").append(threadJiffies.isNewAdded ? "+" : "~").append(")")
                            .append(threadJiffies.name).append("(").append(threadJiffies.tid).append(")\t")
                            .append(threadJiffies.get()).append("\tjiffies")
                            .append("\n");

                    List<LooperTaskMonitorFeature.TaskTraceInfo> threadTasks = tasks.get(threadJiffies.tid);
                    if (null != threadTasks && !threadTasks.isEmpty()) {
                        for (LooperTaskMonitorFeature.TaskTraceInfo task : threadTasks.subList(0, Math.min(3, threadTasks.size()))) {
                            mPrinter.append("|\t\t").append(task).append("\n");
                        }
                    }
                }
                mPrinter.append("|\t\t......\n");
                if (avgJiffies > 1000L || !delta.isValid()) {
                    mPrinter.append("|  ").append(avgJiffies > 1000L ? " #overHeat" : "").append(!delta.isValid() ? " #invalid" : "").append("\n");
                }
            }

            onWritingSections();

            // end
            mPrinter.writeEnding();
            mPrinter.dump();
            synchronized (tasks) {
                tasks.clear();
            }
        }

        @CallSuper
        protected void onWritingSections() {
            final WakeLockMonitorFeature plugin = mMonitor.getMonitorFeature(WakeLockMonitorFeature.class);
            if (null != plugin && null != mLastWakeWakeLockSnapshot) {
                // WakeLock
                createSection("wake_lock", new Consumer<Printer>() {
                    @Override
                    public void accept(Printer printer) {
                        WakeLockSnapshot wakeLockSnapshot = plugin.currentWakeLocks();
                        Delta<WakeLockSnapshot> diff = wakeLockSnapshot.diff(mLastWakeWakeLockSnapshot);
                        onReportWakeLock(diff);
                        printer.createSubSection("during");
                        printer.writeLine(diff.during + "(mls)\t" + (diff.during / ONE_MIN) + "(min)");
                        printer.writeLine("inc_lock_count", String.valueOf(diff.dlt.totalWakeLockCount));
                        printer.writeLine("inc_time_total", String.valueOf(diff.dlt.totalWakeLockTime));
                        printer.createSubSection("locking");
                        for (MonitorFeature.Snapshot.Entry.BeanEntry<WakeLockRecord> item : diff.end.totalWakeLockRecords.getList()) {
                            if (!item.get().isFinished()) {
                                printer.writeLine(item.get().toString());
                            }
                        }
                    }
                });
            }

            final AlarmMonitorFeature alarm = mMonitor.getMonitorFeature(AlarmMonitorFeature.class);
            if (alarm != null && mLastAlarmSnapshot != null) {
                createSection("alarm", new Consumer<Printer>() {
                    @Override
                    public void accept(Printer printer) {
                        AlarmSnapshot alarmSnapshot = alarm.currentAlarms();
                        Delta<AlarmSnapshot> diff = alarmSnapshot.diff(mLastAlarmSnapshot);
                        onReportAlarm(diff);
                        printer.createSubSection("during");
                        printer.writeLine(diff.during + "(mls)\t" + (diff.during / ONE_MIN) + "(min)");
                        printer.writeLine("inc_alarm_count", String.valueOf(diff.dlt.totalCount.get()));
                        printer.writeLine("inc_trace_count", String.valueOf(diff.dlt.tracingCount.get()));
                        printer.writeLine("inc_dupli_group", String.valueOf(diff.dlt.duplicatedGroup.get()));
                        printer.writeLine("inc_dupli_count", String.valueOf(diff.dlt.duplicatedCount.get()));
                    }
                });
            }

            if (/**/(mBlueToothFeat != null && mLastBlueToothSnapshot != null) ||
                    (mWifiMonitorFeat != null && mLastWifiSnapshot != null)) {
                // Scanning
                createSection("scanning", new Consumer<Printer>() {
                    @Override
                    public void accept(Printer printer) {
                        if (mBlueToothFeat != null) {
                            // BlueTooth
                            BlueToothSnapshot currSnapshot = mBlueToothFeat.currentSnapshot();
                            Delta<BlueToothSnapshot> delta = currSnapshot.diff(mLastBlueToothSnapshot);
                            onReportBlueTooth(delta);
                            printer.createSubSection("bluetooh");
                            printer.writeLine(delta.during + "(mls)\t" + (delta.during / ONE_MIN) + "(min)");
                            printer.writeLine("inc_regs_count", String.valueOf(delta.dlt.regsCount.get()));
                            printer.writeLine("inc_dics_count", String.valueOf(delta.dlt.discCount.get()));
                            printer.writeLine("inc_sacn_count", String.valueOf(delta.dlt.scanCount.get()));
                        }
                        if (mWifiMonitorFeat != null) {
                            // Wifi
                            WifiSnapshot currSnapshot = mWifiMonitorFeat.currentSnapshot();
                            Delta<WifiSnapshot> delta = currSnapshot.diff(mLastWifiSnapshot);
                            onReportWifi(delta);
                            printer.createSubSection("wifi");
                            printer.writeLine(delta.during + "(mls)\t" + (delta.during / ONE_MIN) + "(min)");
                            printer.writeLine("inc_sacn_count", String.valueOf(delta.dlt.scanCount.get()));
                            printer.writeLine("inc_qury_count", String.valueOf(delta.dlt.queryCount.get()));
                        }
                    }
                });
            }

            final DeviceStatMonitorFeature deviceStatMonitor = mMonitor.getMonitorFeature(DeviceStatMonitorFeature.class);
            if (deviceStatMonitor != null) {
                // Device Stat
                createSection("dev_stat", new Consumer<Printer>() {
                    @Override
                    public void accept(Printer printer) {
                        if (mLastCpuFreqSnapshot != null) {
                            CpuFreqSnapshot cpuFreqSnapshot = deviceStatMonitor.currentCpuFreq();
                            final Delta<CpuFreqSnapshot> cpuFreqDiff = cpuFreqSnapshot.diff(mLastCpuFreqSnapshot);
                            onReportCpuFreq(cpuFreqDiff);
                            printer.createSubSection("during");
                            printer.writeLine(cpuFreqDiff.during + "(mls)\t" + (cpuFreqDiff.during / ONE_MIN) + "(min)");
                            printer.createSubSection("cpufreq");
                            printer.writeLine("inc", Arrays.toString(cpuFreqDiff.dlt.cpuFreqs.getList().toArray()));
                            printer.writeLine("cur", Arrays.toString(cpuFreqDiff.end.cpuFreqs.getList().toArray()));
                        }

                        if (mLastBatteryTmpSnapshot != null) {
                            BatteryTmpSnapshot batteryTmpSnapshot = deviceStatMonitor.currentBatteryTemperature(Matrix.with().getApplication());
                            Delta<BatteryTmpSnapshot> batteryDiff = batteryTmpSnapshot.diff(mLastBatteryTmpSnapshot);
                            onReportTemperature(batteryDiff);
                            printer.createSubSection("during");
                            printer.writeLine(batteryDiff.during + "(mls)\t" + (batteryDiff.during / ONE_MIN) + "(min)");
                            printer.createSubSection("battery_temperature");
                            printer.writeLine("inc", String.valueOf(batteryDiff.dlt.temp.get()));
                            printer.writeLine("cur", String.valueOf(batteryDiff.end.temp.get()));
                        }
                    }
                });
            }

            final AppStatMonitorFeature appStatFeature = mMonitor.getMonitorFeature(AppStatMonitorFeature.class);
            if (appStatFeature != null) {
                // App Stat
                createSection("app_stat", new Consumer<Printer>() {
                    @Override
                    public void accept(Printer printer) {
                        AppStatMonitorFeature.AppStatSnapshot snapshot = appStatFeature.currentAppStatSnapshot();
                        printer.createSubSection("uptime");
                        printer.writeLine(snapshot.uptime.get() / ONE_MIN + "(min)");
                        printer.createSubSection("ratio");
                        printer.writeLine("fg", String.valueOf(snapshot.fgRatio.get()));
                        printer.writeLine("bg", String.valueOf(snapshot.bgRatio.get()));
                        printer.writeLine("fgSrv", String.valueOf(snapshot.fgSrvRatio.get()));
                    }
                });
            }
        }

        protected void createSection(String sectionName, Consumer<Printer> printerConsumer) {
            mPrinter.createSection(sectionName);
            printerConsumer.accept(mPrinter);
        }

        protected void onReportAlarm(@NonNull Delta<AlarmSnapshot> delta) {}
        protected void onReportBlueTooth(@NonNull Delta<BlueToothSnapshot> delta) {}
        protected void onReportCpuFreq(@NonNull Delta<CpuFreqSnapshot> delta) {}
        protected void onReportJiffies(@NonNull Delta<JiffiesSnapshot> delta) {}
        protected void onReportTemperature(@NonNull Delta<BatteryTmpSnapshot> delta) {}
        protected void onReportWakeLock(@NonNull Delta<WakeLockSnapshot> delta) {}
        protected void onReportWifi(@NonNull Delta<WifiSnapshot> delta) {}

        /**
         * Log Printer
         */
        @SuppressWarnings("UnusedReturnValue")
        public static class Printer {
            private final StringBuilder sb = new StringBuilder("\t\n");

            public Printer() {
            }

            public Printer append(Object obj) {
                sb.append(obj);
                return this;
            }

            public Printer tab() {
                sb.append("\t");
                return this;
            }

            public Printer enter() {
                sb.append("\n");
                return this;
            }

            public Printer writeTitle() {
                sb.append("****************************************** PowerTest *****************************************").append("\n");
                return this;
            }

            public Printer createSection(String sectionName) {
                sb.append("+ --------------------------------------------------------------------------------------------").append("\n");
                sb.append("| ").append(sectionName).append(" :").append("\n");
                return this;
            }

            public Printer writeLine(String line) {
                sb.append("| ").append("  -> ").append(line).append("\n");
                return this;
            }

            public Printer writeLine(String key, String value) {
                sb.append("| ").append("  -> ").append(key).append("\t= ").append(value).append("\n");
                return this;
            }

            public Printer createSubSection(String name) {
                sb.append("| ").append("  <").append(name).append(">\n");
                return this;
            }

            public Printer writeEnding() {
                sb.append("**********************************************************************************************");
                return this;
            }

            public void clear() {
                sb.delete(0, sb.length());
            }

            public void dump() {
                try {
                    MatrixLog.i(TAG, "\t\n" + sb.toString());
                } catch (Throwable e) {
                    MatrixLog.printErrStackTrace(TAG, e, "log format error");
                }
            }

            @Override
            @NonNull
            public String toString() {
                return sb.toString();
            }
        }
    }
}
