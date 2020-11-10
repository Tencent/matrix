package com.tencent.matrix.batterycanary.monitor;

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
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature.BatteryTmpSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature.CpuFreqSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesResult;
import com.tencent.matrix.batterycanary.monitor.feature.LooperTaskMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature.WakeLockSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature.WakeLockTrace.WakeLockRecord;
import com.tencent.matrix.util.MatrixLog;

import java.util.Arrays;
import java.util.List;

/**
 * @author Kaede
 * @since 2020/10/27
 */
public interface BatteryMonitorCallback extends JiffiesMonitorFeature.JiffiesListener, LooperTaskMonitorFeature.LooperTaskListener, WakeLockMonitorFeature.WakeLockListener, AlarmMonitorFeature.AlarmListener {

    @SuppressWarnings({"NotNullFieldNotInitialized", "SpellCheckingInspection"})
    class BatteryPrinter implements BatteryMonitorCallback {
        private static final String TAG = "Matrix.battery.BatteryPrinter";
        private static final int ONE_MIN = 60 * 1000;

        @NonNull
        private BatteryMonitorCore mMonitor;
        private final Printer mPrinter = new Printer();
        private final LongSparseArray<List<LooperTaskMonitorFeature.TaskTraceInfo>> tasks = new LongSparseArray<>();

        @Nullable private JiffiesResult mLastJiffiesResult = null;
        @Nullable private WakeLockSnapshot mLastWakeWakeLockSnapshot = null;
        @Nullable private CpuFreqSnapshot mLastCpuFreqSnapshot = null;
        @Nullable private BatteryTmpSnapshot mLastBatteryTmpSnapshot = null;
        @Nullable private AlarmSnapshot mLastAlarmSnapshot = null;

        @SuppressWarnings("UnusedReturnValue")
        final BatteryPrinter attach(BatteryMonitorCore monitorCore) {
            mMonitor = monitorCore;
            return this;
        }

        @Override
        public void onTraceBegin() {
            WakeLockMonitorFeature jiffies = mMonitor.getMonitorFeature(WakeLockMonitorFeature.class);
            if (null != jiffies) {
                mLastWakeWakeLockSnapshot = jiffies.currentWakeLocks();
            }

            DeviceStatMonitorFeature deviceStat = mMonitor.getMonitorFeature(DeviceStatMonitorFeature.class);
            if (deviceStat != null) {
                mLastCpuFreqSnapshot = deviceStat.currentCpuFreq();
                mLastBatteryTmpSnapshot = deviceStat.currentBatteryTemperature(mMonitor.getContext());
            }

            AlarmMonitorFeature alarm = mMonitor.getMonitorFeature(AlarmMonitorFeature.class);
            if (alarm != null) {
                mLastAlarmSnapshot = alarm.currentAlarms();
            }
        }

        @Override
        public void onTraceEnd() {
        }

        @Override
        public void onJiffies(JiffiesResult result) {
            mLastJiffiesResult = result;
            onCanaryDump();
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
        public void onAlarmDuplicated(int duplicatedCount, AlarmMonitorFeature.AlarmRecord record) {
        }

        @CallSuper
        protected void onCanaryDump() {
            mPrinter.clear();

            // title
            mPrinter.writeTitle();
            JiffiesResult result = mLastJiffiesResult;
            if (result != null) {
                onReportJiffies(result);

                // header
                mPrinter.append("| ").append("pid=").append(Process.myPid())
                        .tab().tab().append("fg=").append(mMonitor.isForeground())
                        .tab().tab().append("during(min)=").append(result.upTimeDiff / ONE_MIN).append("<").append(result.timeDiff / ONE_MIN)
                        .tab().tab().append("diff(jiffies)=").append(result.totalJiffiesDiff)
                        .tab().tab().append("avg(jiffies/min)=").append(result.totalJiffiesDiff / Math.max(1, result.upTimeDiff / ONE_MIN))
                        .enter();

                // jiffies sections
                mPrinter.createSection("jiffies(" + result.status + ")");
                for (JiffiesResult.ThreadJiffies threadJiffies : result.threadJiffies.subList(0, Math.min(result.threadJiffies.size(), 8))) {
                    if (threadJiffies.jiffiesDiff <= 0) {
                        mPrinter.append("|\t\t......\n");
                        break;
                    }
                    mPrinter.append("| -> ").append(threadJiffies).append("\n");
                    List<LooperTaskMonitorFeature.TaskTraceInfo> threadTasks = tasks.get(threadJiffies.threadInfo.tid);
                    if (null != threadTasks && !threadTasks.isEmpty()) {
                        for (LooperTaskMonitorFeature.TaskTraceInfo task : threadTasks.subList(0, Math.min(3, threadTasks.size()))) {
                            mPrinter.append("|\t\t").append(task).append("\n");
                        }
                    }
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
                        printer.writeLine(diff.during + "(mls)\t" + (diff.during/ONE_MIN) +"(min)");
                        printer.writeLine("inc_lock_count", String.valueOf(diff.dlt.totalWakeLockCount));
                        printer.writeLine("inc_time_total", String.valueOf(diff.dlt.totalWakeLockTime));
                        printer.createSubSection("locking");
                        for (WakeLockRecord item : diff.end.totalWakeLockRecords) {
                            if (!item.isFinished()) {
                                printer.writeLine(item.toString());
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
                        printer.writeLine(diff.during + "(mls)\t" + (diff.during/ONE_MIN) +"(min)");
                        printer.writeLine("inc_alarm_count", String.valueOf(diff.dlt.totalCount));
                        printer.writeLine("inc_trace_count", String.valueOf(diff.dlt.tracingCount));
                        printer.writeLine("inc_dupli_group", String.valueOf(diff.dlt.duplicatedGroup));
                        printer.writeLine("inc_dupli_count", String.valueOf(diff.dlt.duplicatedCount));
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
                            printer.writeLine(cpuFreqDiff.during + "(mls)\t" + (cpuFreqDiff.during/ONE_MIN) +"(min)");
                            printer.createSubSection("cpufreq");
                            printer.writeLine("inc", Arrays.toString(cpuFreqDiff.dlt.cpuFreq));
                            printer.writeLine("cur", Arrays.toString(cpuFreqDiff.end.cpuFreq));
                        }

                        if (mLastBatteryTmpSnapshot != null) {
                            BatteryTmpSnapshot batteryTmpSnapshot = deviceStatMonitor.currentBatteryTemperature(Matrix.with().getApplication());
                            Delta<BatteryTmpSnapshot> batteryDiff = batteryTmpSnapshot.diff(mLastBatteryTmpSnapshot);
                            onReportTemperature(batteryDiff);
                            printer.createSubSection("during");
                            printer.writeLine(batteryDiff.during + "(mls)\t" + (batteryDiff.during/ONE_MIN) +"(min)");
                            printer.createSubSection("battery_temperature");
                            printer.writeLine("inc", String.valueOf(batteryDiff.dlt.temperature));
                            printer.writeLine("cur", String.valueOf(batteryDiff.end.temperature));
                        }
                    }
                });
            }
        }

        protected void createSection(String sectionName, Consumer<Printer> printerConsumer) {
            mPrinter.createSection(sectionName);
            printerConsumer.accept(mPrinter);
        }

        protected void onReportJiffies(@NonNull JiffiesResult result) {}
        protected void onReportWakeLock(@NonNull Delta<WakeLockSnapshot> delta) {}
        protected void onReportAlarm(@NonNull Delta<AlarmSnapshot> delta) {}
        protected void onReportCpuFreq(@NonNull Delta<CpuFreqSnapshot> delta) {}
        protected void onReportTemperature(@NonNull Delta<BatteryTmpSnapshot> delta) {}

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
                MatrixLog.i(TAG, "\t\n" + sb.toString());
            }

            @Override
            @NonNull
            public String toString() {
                return sb.toString();
            }
        }
    }
}
