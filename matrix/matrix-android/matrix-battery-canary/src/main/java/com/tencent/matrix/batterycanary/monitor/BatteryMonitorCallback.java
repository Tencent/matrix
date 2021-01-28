package com.tencent.matrix.batterycanary.monitor;

import android.content.ComponentName;
import android.os.HandlerThread;
import android.os.Process;
import android.os.SystemClock;
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
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot.ThreadJiffiesEntry;
import com.tencent.matrix.batterycanary.monitor.feature.LocationMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.LocationMonitorFeature.LocationSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.LooperTaskMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.BeanEntry;
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
        private long mTraceBgnMillis;

        @Nullable protected AlarmMonitorFeature mAlarmFeat;
        @Nullable protected AppStatMonitorFeature mAppStatFeat;
        @Nullable protected BlueToothMonitorFeature mBlueToothFeat;
        @Nullable protected DeviceStatMonitorFeature mDevStatFeat;
        @Nullable protected JiffiesMonitorFeature mJiffiesFeat;
        @Nullable protected LocationMonitorFeature mLocationFeat;
        @Nullable protected TrafficMonitorFeature mTrafficFeat;
        @Nullable protected WakeLockMonitorFeature mWakeLockFeat;
        @Nullable protected WifiMonitorFeature mWifiMonitorFeat;

        @Nullable protected AlarmSnapshot mLastAlarmSnapshot;
        @Nullable protected BlueToothSnapshot mLastBlueToothSnapshot;
        @Nullable protected BatteryTmpSnapshot mLastBatteryTmpSnapshot;
        @Nullable protected CpuFreqSnapshot mLastCpuFreqSnapshot;
        @Nullable protected JiffiesSnapshot mLastJiffiesSnapshot;
        @Nullable protected LocationSnapshot mLastLocationSnapshot;
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
            mTraceBgnMillis = SystemClock.uptimeMillis();

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

            mLocationFeat = mMonitor.getMonitorFeature(LocationMonitorFeature.class);
            if (mLocationFeat != null) {
                mLastLocationSnapshot = mLocationFeat.currentSnapshot();
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

        @Override
        public void onTraceEnd(boolean isForeground) {
            long duringMillis = SystemClock.uptimeMillis() - mTraceBgnMillis;
            if (mTraceBgnMillis <= 0L || duringMillis <= 0L) {
                MatrixLog.w(TAG, "skip invalid battery tracing, bgn = " + mTraceBgnMillis + ", during = " + duringMillis);
                return;
            }

            onCanaryDump(AppStats.current(duringMillis));
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
        protected void onCanaryDump(AppStats appStats) {
            mPrinter.clear();

            // title
            mPrinter.writeTitle();

            // sections
            onWritingJiffiesSection(appStats);
            onWritingSections(appStats);

            // end
            mPrinter.writeEnding();
            mPrinter.dump();
            synchronized (tasks) {
                tasks.clear();
            }
        }

        @CallSuper
        protected void onWritingJiffiesSection(AppStats appStats) {
            if (null != mJiffiesFeat && null != mLastJiffiesSnapshot) {
                JiffiesSnapshot curr = mJiffiesFeat.currentJiffiesSnapshot();
                Delta<JiffiesSnapshot> delta = curr.diff(mLastJiffiesSnapshot);
                onReportJiffies(delta);
                onWritingSectionContent(delta, appStats, mPrinter);
            }
        }

        @CallSuper
        protected void onWritingSections(final AppStats appStats) {
            if (/**/(mAlarmFeat != null && mLastAlarmSnapshot != null)
                    || (mWakeLockFeat != null && mLastWakeWakeLockSnapshot != null)
            ) {
                // Alarm, WakeLock
                createSection("awake", new Consumer<Printer>() {
                    @Override
                    public void accept(Printer printer) {
                        if (mAlarmFeat != null && mLastAlarmSnapshot != null) {
                            AlarmSnapshot alarmSnapshot = mAlarmFeat.currentAlarms();
                            Delta<AlarmSnapshot> delta = alarmSnapshot.diff(mLastAlarmSnapshot);
                            onReportAlarm(delta);
                            onWritingSectionContent(delta, appStats, mPrinter);
                        }
                        if (mWakeLockFeat != null && mLastWakeWakeLockSnapshot != null) {
                            WakeLockSnapshot wakeLockSnapshot = mWakeLockFeat.currentWakeLocks();
                            Delta<WakeLockSnapshot> delta = wakeLockSnapshot.diff(mLastWakeWakeLockSnapshot);
                            onReportWakeLock(delta);
                            onWritingSectionContent(delta, appStats, mPrinter);
                        }
                    }
                });
            }

            if (/**/(mBlueToothFeat != null && mLastBlueToothSnapshot != null)
                    || (mWifiMonitorFeat != null && mLastWifiSnapshot != null)
                    || (mLocationFeat != null && mLastLocationSnapshot != null)
            ) {
                // Scanning
                createSection("scanning", new Consumer<Printer>() {
                    @Override
                    public void accept(Printer printer) {
                        if (mBlueToothFeat != null && mLastBlueToothSnapshot != null) {
                            // BlueTooth
                            BlueToothSnapshot currSnapshot = mBlueToothFeat.currentSnapshot();
                            Delta<BlueToothSnapshot> delta = currSnapshot.diff(mLastBlueToothSnapshot);
                            onReportBlueTooth(delta);
                            onWritingSectionContent(delta, appStats, mPrinter);
                        }
                        if (mWifiMonitorFeat != null && mLastWifiSnapshot != null) {
                            // Wifi
                            WifiSnapshot currSnapshot = mWifiMonitorFeat.currentSnapshot();
                            Delta<WifiSnapshot> delta = currSnapshot.diff(mLastWifiSnapshot);
                            onReportWifi(delta);
                            onWritingSectionContent(delta, appStats, mPrinter);
                        }
                        if (mLocationFeat != null && mLastLocationSnapshot != null) {
                            // Location
                            LocationSnapshot currSnapshot = mLocationFeat.currentSnapshot();
                            Delta<LocationSnapshot> delta = currSnapshot.diff(mLastLocationSnapshot);
                            onReportLocation(delta);
                            onWritingSectionContent(delta, appStats, mPrinter);
                        }
                    }
                });
            }

            if (/**/(mAppStatFeat != null)
                    || (mDevStatFeat != null && mLastCpuFreqSnapshot != null)
                    || (mDevStatFeat != null && mLastBatteryTmpSnapshot != null)
            ) {
                // Status
                createSection("app_stats", new Consumer<Printer>() {
                    @Override
                    public void accept(Printer printer) {
                        if (mDevStatFeat != null && mLastCpuFreqSnapshot != null) {
                            CpuFreqSnapshot cpuFreqSnapshot = mDevStatFeat.currentCpuFreq();
                            final Delta<CpuFreqSnapshot> delta = cpuFreqSnapshot.diff(mLastCpuFreqSnapshot);
                            onReportCpuFreq(delta);
                            onWritingSectionContent(delta, appStats, mPrinter);
                        }

                        if (mDevStatFeat != null && mLastBatteryTmpSnapshot != null) {
                            BatteryTmpSnapshot batteryTmpSnapshot = mDevStatFeat.currentBatteryTemperature(Matrix.with().getApplication());
                            Delta<BatteryTmpSnapshot> delta = batteryTmpSnapshot.diff(mLastBatteryTmpSnapshot);
                            onReportTemperature(delta);
                            onWritingSectionContent(delta, appStats, mPrinter);
                        }

                        if (mAppStatFeat != null) {
                            AppStatMonitorFeature.AppStatSnapshot currSnapshot = mAppStatFeat.currentAppStatSnapshot();
                            printer.createSubSection("app_uptime");
                            printer.writeLine(currSnapshot.uptime.get() / ONE_MIN + "(min)");
                            printer.createSubSection("app_stat_ratio");
                            printer.writeLine("fg", String.valueOf(currSnapshot.fgRatio.get()));
                            printer.writeLine("bg", String.valueOf(currSnapshot.bgRatio.get()));
                            printer.writeLine("fgSrv", String.valueOf(currSnapshot.fgSrvRatio.get()));
                        }
                    }
                });
            }

            onWritingSections();
        }

        @Deprecated
        @CallSuper
        protected void onWritingSections() {}

        @CallSuper
        protected boolean onWritingSectionContent(@NonNull Delta<?> sessionDelta, AppStats appStats, Printer printer) {
            // - Dump Jiffies
            if (sessionDelta.dlt instanceof JiffiesSnapshot) {
                //noinspection unchecked
                Delta<JiffiesSnapshot> delta = (Delta<JiffiesSnapshot>) sessionDelta;
                // header
                long minute = Math.max(1, delta.during / ONE_MIN);
                long avgJiffies = delta.dlt.totalJiffies.get() / minute;
                printer.append("| ").append("pid=").append(Process.myPid())
                        .tab().tab().append("fg=").append(appStats.getAppStat())
                        .tab().tab().append("during(min)=").append(minute)
                        .tab().tab().append("diff(jiffies)=").append(delta.dlt.totalJiffies.get())
                        .tab().tab().append("avg(jiffies/min)=").append(avgJiffies)
                        .enter();

                // jiffies sections
                printer.createSection("jiffies(" + delta.dlt.threadEntries.getList().size() + ")");
                printer.writeLine("inc_thread_num", String.valueOf(delta.dlt.threadNum.get()));
                printer.writeLine("cur_thread_num", String.valueOf(delta.end.threadNum.get()));
                for (ThreadJiffiesEntry threadJiffies : delta.dlt.threadEntries.getList().subList(0, Math.min(delta.dlt.threadEntries.getList().size(), 8))) {
                    long entryJffies = threadJiffies.get();
                    printer.append("|   -> (").append(threadJiffies.isNewAdded ? "+" : "~").append("/").append(threadJiffies.stat).append(")")
                            .append(threadJiffies.name).append("(").append(threadJiffies.tid).append(")\t")
                            .append(entryJffies/minute).append("/").append(entryJffies).append("\tjiffies")
                            .append("\n");

                    List<LooperTaskMonitorFeature.TaskTraceInfo> threadTasks = tasks.get(threadJiffies.tid);
                    if (null != threadTasks && !threadTasks.isEmpty()) {
                        for (LooperTaskMonitorFeature.TaskTraceInfo task : threadTasks.subList(0, Math.min(3, threadTasks.size()))) {
                            printer.append("|\t\t").append(task).append("\n");
                        }
                    }
                }
                printer.append("|\t\t......\n");
                if (avgJiffies > 1000L || !delta.isValid()) {
                    printer.append("|  ").append(avgJiffies > 1000L ? " #overHeat" : "").append(!delta.isValid() ? " #invalid" : "").append("\n");
                }
                return true;
            }

            // - Dump Alarm
            if (sessionDelta.dlt instanceof AlarmSnapshot) {
                //noinspection unchecked
                Delta<AlarmSnapshot> delta = (Delta<AlarmSnapshot>) sessionDelta;
                printer.createSubSection("alarm");
                printer.writeLine(delta.during + "(mls)\t" + (delta.during / ONE_MIN) + "(min)");
                printer.writeLine("inc_alarm_count", String.valueOf(delta.dlt.totalCount.get()));
                printer.writeLine("inc_trace_count", String.valueOf(delta.dlt.tracingCount.get()));
                printer.writeLine("inc_dupli_group", String.valueOf(delta.dlt.duplicatedGroup.get()));
                printer.writeLine("inc_dupli_count", String.valueOf(delta.dlt.duplicatedCount.get()));
                return true;
            }

            // - Dump WakeLock
            if (sessionDelta.dlt instanceof WakeLockSnapshot) {
                //noinspection unchecked
                Delta<WakeLockSnapshot> delta = (Delta<WakeLockSnapshot>) sessionDelta;
                printer.createSubSection("wake_lock");
                printer.writeLine(delta.during + "(mls)\t" + (delta.during / ONE_MIN) + "(min)");
                printer.writeLine("inc_lock_count", String.valueOf(delta.dlt.totalWakeLockCount));
                printer.writeLine("inc_time_total", String.valueOf(delta.dlt.totalWakeLockTime));

                List<BeanEntry<WakeLockRecord>> wakeLockRecordsList = delta.end.totalWakeLockRecords.getList();
                if (!wakeLockRecordsList.isEmpty()) {
                    printer.createSubSection("locking");
                    for (BeanEntry<WakeLockRecord> item : wakeLockRecordsList) {
                        if (!item.get().isFinished()) {
                            printer.writeLine(item.get().toString());
                        }
                    }
                }
                return true;
            }

            // - Dump BlueTooth
            if (sessionDelta.dlt instanceof BlueToothSnapshot) {
                //noinspection unchecked
                Delta<BlueToothSnapshot> delta = (Delta<BlueToothSnapshot>) sessionDelta;
                printer.createSubSection("bluetooh");
                printer.writeLine(delta.during + "(mls)\t" + (delta.during / ONE_MIN) + "(min)");
                printer.writeLine("inc_regs_count", String.valueOf(delta.dlt.regsCount.get()));
                printer.writeLine("inc_dics_count", String.valueOf(delta.dlt.discCount.get()));
                printer.writeLine("inc_scan_count", String.valueOf(delta.dlt.scanCount.get()));
                return true;
            }

            // - Dump BlueTooth
            if (sessionDelta.dlt instanceof WifiSnapshot) {
                //noinspection unchecked
                Delta<WifiSnapshot> delta = (Delta<WifiSnapshot>) sessionDelta;
                printer.createSubSection("wifi");
                printer.writeLine(delta.during + "(mls)\t" + (delta.during / ONE_MIN) + "(min)");
                printer.writeLine("inc_scan_count", String.valueOf(delta.dlt.scanCount.get()));
                printer.writeLine("inc_qury_count", String.valueOf(delta.dlt.queryCount.get()));
                return true;
            }

            // - Dump BlueTooth
            if (sessionDelta.dlt instanceof LocationSnapshot) {
                //noinspection unchecked
                Delta<LocationSnapshot> delta = (Delta<LocationSnapshot>) sessionDelta;
                printer.createSubSection("location");
                printer.writeLine(delta.during + "(mls)\t" + (delta.during / ONE_MIN) + "(min)");
                printer.writeLine("inc_scan_count", String.valueOf(delta.dlt.scanCount.get()));
                return true;
            }

            // - Dump CpuFreq
            if (sessionDelta.dlt instanceof CpuFreqSnapshot) {
                //noinspection unchecked
                Delta<CpuFreqSnapshot> delta = (Delta<CpuFreqSnapshot>) sessionDelta;
                printer.createSubSection("cpufreq");
                printer.writeLine(delta.during + "(mls)\t" + (delta.during / ONE_MIN) + "(min)");
                printer.writeLine("inc", Arrays.toString(delta.dlt.cpuFreqs.getList().toArray()));
                printer.writeLine("cur", Arrays.toString(delta.end.cpuFreqs.getList().toArray()));
                return true;
            }

            // - Dump Battery Temperature
            if (sessionDelta.dlt instanceof BatteryTmpSnapshot) {
                //noinspection unchecked
                Delta<BatteryTmpSnapshot> delta = (Delta<BatteryTmpSnapshot>) sessionDelta;
                printer.createSubSection("batt_temp");
                printer.writeLine(delta.during + "(mls)\t" + (delta.during / ONE_MIN) + "(min)");
                printer.writeLine("inc", String.valueOf(delta.dlt.temp.get()));
                printer.writeLine("cur", String.valueOf(delta.end.temp.get()));
                return true;
            }

            return false;
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
        protected void onReportLocation(@NonNull Delta<LocationSnapshot> delta) {}

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
