package com.tencent.matrix.batterycanary.monitor;

import android.content.ComponentName;
import android.os.HandlerThread;
import android.os.Process;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.LongSparseArray;

import com.tencent.matrix.batterycanary.monitor.feature.AbsTaskMonitorFeature.TaskJiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.AlarmMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.AlarmMonitorFeature.AlarmSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.AppStatMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.BlueToothMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.BlueToothMonitorFeature.BlueToothSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.CompositeMonitors;
import com.tencent.matrix.batterycanary.monitor.feature.CpuStatFeature;
import com.tencent.matrix.batterycanary.monitor.feature.CpuStatFeature.CpuStateSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature.BatteryTmpSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature.CpuFreqSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.InternalMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot.ThreadJiffiesEntry;
import com.tencent.matrix.batterycanary.monitor.feature.LocationMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.LocationMonitorFeature.LocationSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.LooperTaskMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.BeanEntry;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.DigitEntry;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.ListEntry;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Sampler;
import com.tencent.matrix.batterycanary.monitor.feature.NotificationMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.NotificationMonitorFeature.BadNotification;
import com.tencent.matrix.batterycanary.monitor.feature.TrafficMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.TrafficMonitorFeature.RadioStatSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature.WakeLockSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature.WakeLockTrace.WakeLockRecord;
import com.tencent.matrix.batterycanary.monitor.feature.WifiMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WifiMonitorFeature.WifiSnapshot;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.Consumer;
import com.tencent.matrix.batterycanary.utils.PowerProfile;
import com.tencent.matrix.util.MatrixLog;

import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.Map;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

/**
 * @author Kaede
 * @since 2020/10/27
 */
public interface BatteryMonitorCallback extends
        BatteryMonitorCore.JiffiesListener,
        InternalMonitorFeature.InternalListener,
        LooperTaskMonitorFeature.LooperTaskListener,
        WakeLockMonitorFeature.WakeLockListener,
        AlarmMonitorFeature.AlarmListener,
        JiffiesMonitorFeature.JiffiesListener,
        NotificationMonitorFeature.NotificationListener,
        AppStatMonitorFeature.AppStatListener {

    @SuppressWarnings({"NotNullFieldNotInitialized", "SpellCheckingInspection", "unused"})
    class BatteryPrinter implements BatteryMonitorCallback {
        private static final String TAG = "Matrix.battery.BatteryPrinter";
        private static final int ONE_MIN = 60 * 1000;

        @NonNull
        protected BatteryMonitorCore mMonitor;
        @NonNull
        protected CompositeMonitors mCompositeMonitors;

        protected long mTraceBgnMillis;
        protected boolean mIsForeground;

        // TODO: Remove deprecated fields
        @Deprecated
        protected Printer mPrinter = new Printer();
        @Deprecated
        protected final LongSparseArray<List<LooperTaskMonitorFeature.TaskTraceInfo>> tasks = new LongSparseArray<>();
        @Deprecated
        protected AlarmMonitorFeature mAlarmFeat;
        @Deprecated
        protected AppStatMonitorFeature mAppStatFeat;
        @Deprecated
        protected BlueToothMonitorFeature mBlueToothFeat;
        @Deprecated
        protected DeviceStatMonitorFeature mDevStatFeat;
        @Deprecated
        protected JiffiesMonitorFeature mJiffiesFeat;
        @Deprecated
        protected LocationMonitorFeature mLocationFeat;
        @Deprecated
        protected TrafficMonitorFeature mTrafficFeat;
        @Deprecated
        protected WakeLockMonitorFeature mWakeLockFeat;
        @Deprecated
        protected WifiMonitorFeature mWifiMonitorFeat;
        @Deprecated
        protected CpuStatFeature mCpuStatFeat;

        @Deprecated
        protected AlarmSnapshot mLastAlarmSnapshot;
        @Deprecated
        protected BlueToothSnapshot mLastBlueToothSnapshot;
        @Deprecated
        protected BatteryTmpSnapshot mLastBatteryTmpSnapshot;
        @Deprecated
        protected CpuFreqSnapshot mLastCpuFreqSnapshot;
        @Deprecated
        protected JiffiesSnapshot mLastJiffiesSnapshot;
        @Deprecated
        protected LocationSnapshot mLastLocationSnapshot;
        @Deprecated
        protected RadioStatSnapshot mLastTrafficSnapshot;
        @Deprecated
        protected WakeLockSnapshot mLastWakeWakeLockSnapshot;
        @Deprecated
        protected WifiSnapshot mLastWifiSnapshot;
        @Deprecated
        protected CpuStateSnapshot mLastCpuStateSnapshot;

        @SuppressWarnings("UnusedReturnValue")
        @VisibleForTesting
        public BatteryPrinter attach(BatteryMonitorCore monitorCore) {
            mMonitor = monitorCore;
            mCompositeMonitors = new CompositeMonitors(monitorCore);
            mCompositeMonitors.metricAll();
            return this;
        }

        @NonNull
        protected BatteryMonitorCore getMonitor() {
            return mMonitor;
        }

        protected boolean isForegroundReport() {
            return mIsForeground;
        }

        @Deprecated
        protected AppStats getAppStats() {
            if (mCompositeMonitors.getAppStats() != null) {
                return mCompositeMonitors.getAppStats();
            }
            return AppStats.current();
        }

        @CallSuper
        @Override
        public void onTraceBegin() {
            mTraceBgnMillis = SystemClock.uptimeMillis();
            mCompositeMonitors.clear();
            mCompositeMonitors.start();

            // TODO: Remove deprecated statements
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

            mCpuStatFeat = mMonitor.getMonitorFeature(CpuStatFeature.class);
            if (mCpuStatFeat != null && mCpuStatFeat.isSupported()) {
                mLastCpuStateSnapshot = mCpuStatFeat.currentCpuStateSnapshot();
            }
        }

        @Override
        public void onTraceEnd(boolean isForeground) {
            mIsForeground = isForeground;
            long duringMillis = SystemClock.uptimeMillis() - mTraceBgnMillis;
            if (mTraceBgnMillis <= 0L || duringMillis <= 0L) {
                MatrixLog.w(TAG, "skip invalid battery tracing, bgn = " + mTraceBgnMillis + ", during = " + duringMillis);
                return;
            }
            mCompositeMonitors.finish();
            onCanaryDump(mCompositeMonitors);
        }

        @Override
        public void onReportInternalJiffies(Delta<TaskJiffiesSnapshot> delta) {
            CompositeMonitors monitors = new CompositeMonitors(mMonitor);
            monitors.setAppStats(AppStats.current(delta.during));
            monitors.putDelta(InternalMonitorFeature.InternalSnapshot.class, delta);
            onCanaryReport(monitors);
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
        public void onLooperTaskOverHeat(@NonNull List<Delta<TaskJiffiesSnapshot>> deltas) {
        }

        @Override
        public void onLooperConcurrentOverHeat(String key, int concurrentCount, long duringMillis) {
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
        public void onNotify(@NonNull BadNotification notification) {
        }

        @Override
        public void onWatchingThreads(ListEntry<? extends ThreadJiffiesEntry> threadJiffiesList) {
            Printer printer = new Printer();
            printer.writeTitle();
            printer.append("| Thread WatchDog").append("\n");

            printer.createSection("jiffies(" + threadJiffiesList.getList().size() + ")");
            printer.writeLine("desc", "(status)name(tid)\ttotal");
            for (ThreadJiffiesEntry threadJiffies : threadJiffiesList.getList()) {
                long entryJffies = threadJiffies.get();
                printer.append("|   -> (").append(threadJiffies.isNewAdded ? "+" : "~").append("/").append(threadJiffies.stat).append(")")
                        .append(threadJiffies.name).append("(").append(threadJiffies.tid).append(")\t")
                        .append(entryJffies).append("\tjiffies")
                        .append("\n");
            }

            // Dump thread stacks if need
            printer.createSection("stacks");
            boolean dumpStacks = getMonitor().getConfig().isAggressiveMode;
            if (!dumpStacks || !getMonitor().getConfig().threadWatchList.isEmpty()) {
                for (ThreadJiffiesEntry threadJiffies : threadJiffiesList.getList()) {
                    for (String settingItem : getMonitor().getConfig().threadWatchList) {
                        if (settingItem.equalsIgnoreCase(threadJiffies.name) || threadJiffies.name.contains(settingItem)) {
                            dumpStacks = true;
                            break;
                        }
                    }
                    if (dumpStacks) {
                        break;
                    }
                }
            }
            if (dumpStacks) {
                Map<Thread, StackTraceElement[]> stackTraces = Thread.getAllStackTraces();
                MatrixLog.i(TAG, "onWatchingThreads dump stacks, get all threads size = " + stackTraces);

                for (Map.Entry<Thread, StackTraceElement[]> entry : stackTraces.entrySet()) {
                    Thread thread = entry.getKey();
                    StackTraceElement[] elements = entry.getValue();
                    String threadName = thread.getName();

                    for (ThreadJiffiesEntry threadJiffies : threadJiffiesList.getList()) {
                        String targetThreadName = threadJiffies.name;
                        if (targetThreadName.equalsIgnoreCase(threadName)
                                || threadName.contains(targetThreadName)) {

                            // Dump stacks of traced thread
                            // thread name
                            printer.append("|   -> ")
                                    .append("(").append(thread.getState()).append(")")
                                    .append(threadName).append("(").append(thread.getId()).append(")")
                                    .append("\n");
                            String stack = BatteryCanaryUtil.stackTraceToString(elements);
                            // thread stacks
                            for (StackTraceElement item : elements) {
                                printer.append("|      ").append(item).append("\n");
                            }
                        }
                    }
                }
            } else {
                printer.append("|   disabled").append("\n");
            }

            printer.writeEnding();
            printer.dump();
        }

        @Override
        public void onForegroundServiceLeak(boolean isMyself, int appImportance, int globalAppImportance, ComponentName componentName, long millis) {
        }

        @Override
        public void onAppSateLeak(boolean isMyself, int appImportance, ComponentName componentName, long millis) {
        }

        protected void checkBadThreads(final CompositeMonitors monitors) {
            monitors.getDelta(JiffiesSnapshot.class, new Consumer<Delta<JiffiesSnapshot>>() {
                @Override
                public void accept(final Delta<JiffiesSnapshot> delta) {
                    monitors.getAppStats(new Consumer<AppStats>() {
                        @Override
                        public void accept(final AppStats appStats) {

                            final long minute = appStats.getMinute();
                            for (final ThreadJiffiesEntry threadJiffies : delta.dlt.threadEntries.getList()) {
                                if (!threadJiffies.stat.toUpperCase().contains("R")) {
                                    continue;
                                }
                                monitors.getFeature(JiffiesMonitorFeature.class, new Consumer<JiffiesMonitorFeature>() {
                                    @Override
                                    public void accept(JiffiesMonitorFeature feature) {
                                        // Watching thread state when thread is:
                                        // 1. still running (status 'R')
                                        // 2. runing time > 10min
                                        // 3. avgJiffies > THRESHOLD
                                        long avgJiffies = threadJiffies.get() / minute;
                                        if (appStats.isForeground()) {
                                            if (minute > 10 && avgJiffies > getMonitor().getConfig().fgThreadWatchingLimit) {
                                                MatrixLog.i(TAG, "threadWatchDog fg set, name = " + delta.dlt.name
                                                        + ", pid = " + delta.dlt.pid
                                                        + ", tid = " + threadJiffies.tid);
                                                feature.watchBackThreadSate(true, delta.dlt.pid, threadJiffies.tid);
                                            }
                                        } else {
                                            if (minute > 10 && avgJiffies > getMonitor().getConfig().bgThreadWatchingLimit) {
                                                MatrixLog.i(TAG, "threadWatchDog bg set, name = " + delta.dlt.name
                                                        + ", pid = " + delta.dlt.pid
                                                        + ", tid = " + threadJiffies.tid);
                                                feature.watchBackThreadSate(false, delta.dlt.pid, threadJiffies.tid);
                                            }
                                        }
                                    }
                                });
                            }
                        }
                    });
                }
            });
        }

        protected Dumper createDumper() {
            return new Dumper() {
                @Override
                protected void onWritingSections(CompositeMonitors monitors, Printer printer) {
                    super.onWritingSections(monitors, printer);
                    monitors.getAppStats(new Consumer<AppStats>() {
                        @Override
                        public void accept(AppStats appStats) {
                            BatteryPrinter.this.onWritingSections(appStats);
                        }
                    });
                }

                @Override
                protected boolean onWritingSectionContent(@NonNull Delta<?> sessionDelta, CompositeMonitors monitors, Printer printer) {
                    if (!super.onWritingSectionContent(sessionDelta, monitors, printer)) {
                        AppStats appStats = monitors.getAppStats();
                        if (appStats != null) {
                            return BatteryPrinter.this.onWritingSectionContent(sessionDelta, appStats, printer);
                        }
                    }
                    return false;
                }
            };
        }

        protected Printer createPrinter() {
            // TODO: Remove deprecated statements
            mPrinter = new Printer();
            return mPrinter;
        }

        @CallSuper
        protected void onCanaryDump(final CompositeMonitors monitors) {
            monitors.getAppStats(new Consumer<AppStats>() {
                @Override
                public void accept(AppStats appStats) {
                    onCanaryDump(appStats);
                }
            });

            Dumper dumper = createDumper();
            Printer printer = createPrinter();
            printer.writeTitle();
            dumper.dump(monitors, printer);
            printer.writeEnding();
            printer.dump();

            checkBadThreads(monitors);
            onCanaryReport(monitors);

            synchronized (tasks) {
                tasks.clear();
            }
        }

        protected void onPreDumping(CompositeMonitors monitors) {
            checkBadThreads(monitors);
        }

        @Deprecated
        @CallSuper
        protected void onCanaryDump(AppStats appStats) {
            // mPrinter.clear();
            //
            // // title
            // mPrinter.writeTitle();
            //
            // // sections
            // onWritingJiffiesSection(appStats);
            // onWritingSections(appStats);
            // onWritingAppStatSection(appStats);
            //
            // // end
            // mPrinter.writeEnding();
            // mPrinter.dump();
            // synchronized (tasks) {
            //     tasks.clear();
            // }
        }

        // @Deprecated
        // @CallSuper
        // protected void onWritingJiffiesSection(final AppStats appStats) {
        //     mCompositeMonitors.getDelta(JiffiesSnapshot.class, new Consumer<Delta<JiffiesSnapshot>>() {
        //         @Override
        //         public void accept(final Delta<JiffiesSnapshot> delta) {
        //             final long minute = appStats.getMinute();
        //             for (final ThreadJiffiesEntry threadJiffies : delta.dlt.threadEntries.getList()) {
        //                 if (!threadJiffies.stat.toUpperCase().contains("R")) {
        //                     continue;
        //                 }
        //                 mCompositeMonitors.getFeature(JiffiesMonitorFeature.class, new Consumer<JiffiesMonitorFeature>() {
        //                     @Override
        //                     public void accept(JiffiesMonitorFeature feature) {
        //                         // Watching thread state when thread is:
        //                         // 1. still running (status 'R')
        //                         // 2. runing time > 10min
        //                         // 3. avgJiffies > THRESHOLD
        //                         long avgJiffies = threadJiffies.get() / minute;
        //                         if (appStats.isForeground()) {
        //                             if (minute > 10 && avgJiffies > getMonitor().getConfig().fgThreadWatchingLimit) {
        //                                 MatrixLog.i(TAG, "threadWatchDog fg set, name = " + delta.dlt.name
        //                                         + ", pid = " + delta.dlt.pid
        //                                         + ", tid = " + threadJiffies.tid);
        //                                 feature.watchBackThreadSate(true, delta.dlt.pid, threadJiffies.tid);
        //                             }
        //                         } else {
        //                             if (minute > 10 && avgJiffies > getMonitor().getConfig().bgThreadWatchingLimit) {
        //                                 MatrixLog.i(TAG, "threadWatchDog bg set, name = " + delta.dlt.name
        //                                         + ", pid = " + delta.dlt.pid
        //                                         + ", tid = " + threadJiffies.tid);
        //                                 feature.watchBackThreadSate(false, delta.dlt.pid, threadJiffies.tid);
        //                             }
        //                         }
        //                     }
        //                 });
        //             }
        //             onReportJiffies(delta);
        //             onWritingSectionContent(delta, appStats, mPrinter);
        //         }
        //     });
        // }

        // @Deprecated
        // @CallSuper
        // protected void onWritingAppStatSection(final AppStats appStats) {
        //     createSection("app_stats", new Consumer<Printer>() {
        //         @Override
        //         public void accept(final Printer printer) {
        //             printer.createSubSection("stat_time");
        //             printer.writeLine("time", appStats.getMinute() + "(min)");
        //             printer.writeLine("fg", String.valueOf(appStats.appFgRatio));
        //             printer.writeLine("bg", String.valueOf(appStats.appBgRatio));
        //             printer.writeLine("fgSrv", String.valueOf(appStats.appFgSrvRatio));
        //             printer.writeLine("devCharging", String.valueOf(appStats.devChargingRatio));
        //             printer.writeLine("devScreenOff", String.valueOf(appStats.devSceneOffRatio));
        //             if (!TextUtils.isEmpty(appStats.sceneTop1)) {
        //                 printer.writeLine("sceneTop1", appStats.sceneTop1 + "/" + appStats.sceneTop1Ratio);
        //             }
        //             if (!TextUtils.isEmpty(appStats.sceneTop2)) {
        //                 printer.writeLine("sceneTop2", appStats.sceneTop2 + "/" + appStats.sceneTop2Ratio);
        //             }
        //             mCompositeMonitors.getFeature(AppStatMonitorFeature.class, new Consumer<AppStatMonitorFeature>() {
        //                 @Override
        //                 public void accept(AppStatMonitorFeature feature) {
        //                     AppStatMonitorFeature.AppStatSnapshot currSnapshot = feature.currentAppStatSnapshot();
        //                     printer.createSubSection("run_time");
        //                     printer.writeLine("time", currSnapshot.uptime.get() / ONE_MIN + "(min)");
        //                     printer.writeLine("fg", String.valueOf(currSnapshot.fgRatio.get()));
        //                     printer.writeLine("bg", String.valueOf(currSnapshot.bgRatio.get()));
        //                     printer.writeLine("fgSrv", String.valueOf(currSnapshot.fgSrvRatio.get()));
        //                 }
        //             });
        //         }
        //     });
        // }

        @Deprecated
        @CallSuper
        protected void onWritingSections(final AppStats appStats) {
        }

        // @Deprecated
        // @CallSuper
        // protected void onWritingSections() {
        // }

        @Deprecated
        @CallSuper
        protected boolean onWritingSectionContent(@NonNull Delta<?> sessionDelta, AppStats appStats, final Printer printer) {
            return false;
        }

        @Deprecated
        protected void createSection(String sectionName, Consumer<Printer> printerConsumer) {
            mPrinter.createSection(sectionName);
            printerConsumer.accept(mPrinter);
        }

        protected void onCanaryReport(CompositeMonitors monitors) {
            monitors.getDelta(AlarmSnapshot.class, new Consumer<Delta<AlarmSnapshot>>() {
                @Override
                public void accept(Delta<AlarmSnapshot> delta) {
                    onReportAlarm(delta);
                }
            });
            monitors.getDelta(BlueToothSnapshot.class, new Consumer<Delta<BlueToothSnapshot>>() {
                @Override
                public void accept(Delta<BlueToothSnapshot> delta) {
                    onReportBlueTooth(delta);
                }
            });
            monitors.getDelta(CpuFreqSnapshot.class, new Consumer<Delta<CpuFreqSnapshot>>() {
                @Override
                public void accept(Delta<CpuFreqSnapshot> delta) {
                    onReportCpuFreq(delta);
                }
            });
            monitors.getDelta(CpuStateSnapshot.class, new Consumer<Delta<CpuStateSnapshot>>() {
                @Override
                public void accept(Delta<CpuStateSnapshot> delta) {
                    onReportCpuStats(delta);
                }
            });
            monitors.getDelta(JiffiesSnapshot.class, new Consumer<Delta<JiffiesSnapshot>>() {
                @Override
                public void accept(Delta<JiffiesSnapshot> delta) {
                    onReportJiffies(delta);
                }
            });
            monitors.getDelta(BatteryTmpSnapshot.class, new Consumer<Delta<BatteryTmpSnapshot>>() {
                @Override
                public void accept(Delta<BatteryTmpSnapshot> delta) {
                    onReportTemperature(delta);
                }
            });
            monitors.getDelta(WakeLockSnapshot.class, new Consumer<Delta<WakeLockSnapshot>>() {
                @Override
                public void accept(Delta<WakeLockSnapshot> delta) {
                    onReportWakeLock(delta);
                }
            });
            monitors.getDelta(WifiSnapshot.class, new Consumer<Delta<WifiSnapshot>>() {
                @Override
                public void accept(Delta<WifiSnapshot> delta) {
                    onReportWifi(delta);
                }
            });
            monitors.getDelta(LocationSnapshot.class, new Consumer<Delta<LocationSnapshot>>() {
                @Override
                public void accept(Delta<LocationSnapshot> delta) {
                    onReportLocation(delta);
                }
            });
        }

        @Deprecated
        protected void onReportAlarm(@NonNull Delta<AlarmSnapshot> delta) {
        }

        @Deprecated
        protected void onReportBlueTooth(@NonNull Delta<BlueToothSnapshot> delta) {
        }

        @Deprecated
        protected void onReportCpuFreq(@NonNull Delta<CpuFreqSnapshot> delta) {
        }

        @Deprecated
        protected void onReportCpuStats(@NonNull Delta<CpuStateSnapshot> delta) {
        }

        @Deprecated
        protected void onReportJiffies(@NonNull Delta<JiffiesSnapshot> delta) {
        }

        @Deprecated
        protected void onReportTemperature(@NonNull Delta<BatteryTmpSnapshot> delta) {
        }

        @Deprecated
        protected void onReportWakeLock(@NonNull Delta<WakeLockSnapshot> delta) {
        }

        @Deprecated
        protected void onReportWifi(@NonNull Delta<WifiSnapshot> delta) {
        }

        @Deprecated
        protected void onReportLocation(@NonNull Delta<LocationSnapshot> delta) {
        }


        public static class Dumper {
            public void dump(CompositeMonitors monitors, Printer printer) {
                onWritingSections(monitors, printer);
                onWritingAppStatSection(monitors, printer);
            }

            protected void onWritingSections(final CompositeMonitors monitors, final Printer printer) {
                if (monitors.getMonitor() == null || monitors.getAppStats() == null) {
                    return;
                }

                // Thread Jiffies
                monitors.getDelta(JiffiesSnapshot.class, new Consumer<Delta<JiffiesSnapshot>>() {
                    @Override
                    public void accept(final Delta<JiffiesSnapshot> delta) {
                        onWritingSectionContent(delta, monitors, printer);
                    }
                });


                final AppStats appStats = monitors.getAppStats();
                if (/**/(monitors.getDelta(AlarmSnapshot.class) != null)
                        || (monitors.getDelta(WakeLockSnapshot.class) != null)
                ) {
                    // Alarm, WakeLock
                    printer.createSection("awake");
                    monitors.getDelta(AlarmSnapshot.class, new Consumer<Delta<AlarmSnapshot>>() {
                        @Override
                        public void accept(Delta<AlarmSnapshot> delta) {
                            onWritingSectionContent(delta, monitors, printer);
                        }
                    });
                    monitors.getDelta(WakeLockSnapshot.class, new Consumer<Delta<WakeLockSnapshot>>() {
                        @Override
                        public void accept(Delta<WakeLockSnapshot> delta) {
                            onWritingSectionContent(delta, monitors, printer);
                        }
                    });
                }

                if (/**/(monitors.getDelta(BlueToothSnapshot.class) != null)
                        || (monitors.getDelta(WifiSnapshot.class) != null)
                        || (monitors.getDelta(LocationSnapshot.class) != null)
                ) {
                    // Scanning: BL, WIFI, GPS
                    printer.createSection("scanning");
                    // BlueTooth
                    monitors.getDelta(BlueToothSnapshot.class, new Consumer<Delta<BlueToothSnapshot>>() {
                        @Override
                        public void accept(Delta<BlueToothSnapshot> delta) {
                            onWritingSectionContent(delta, monitors, printer);
                        }
                    });
                    // Wifi
                    monitors.getDelta(WifiSnapshot.class, new Consumer<Delta<WifiSnapshot>>() {
                        @Override
                        public void accept(Delta<WifiSnapshot> delta) {
                            onWritingSectionContent(delta, monitors, printer);
                        }
                    });
                    // Location
                    monitors.getDelta(LocationSnapshot.class, new Consumer<Delta<LocationSnapshot>>() {
                        @Override
                        public void accept(Delta<LocationSnapshot> delta) {
                            onWritingSectionContent(delta, monitors, printer);
                        }
                    });
                }

                if (/**/(monitors.getFeature(AppStatMonitorFeature.class) != null)
                        || (monitors.getDelta(CpuStateSnapshot.class) != null)
                        || (monitors.getDelta(CpuFreqSnapshot.class) != null)
                        || (monitors.getDelta(BatteryTmpSnapshot.class) != null)
                ) {
                    // Stats: Cpu Usage, Device Status
                    printer.createSection("dev_stats");
                    monitors.getDelta(CpuStateSnapshot.class, new Consumer<Delta<CpuStateSnapshot>>() {
                        @Override
                        public void accept(Delta<CpuStateSnapshot> delta) {
                            onWritingSectionContent(delta, monitors, printer);
                        }
                    });
                    monitors.getDelta(CpuFreqSnapshot.class, new Consumer<Delta<CpuFreqSnapshot>>() {
                        @Override
                        public void accept(Delta<CpuFreqSnapshot> delta) {
                            onWritingSectionContent(delta, monitors, printer);
                        }
                    });
                    monitors.getDelta(BatteryTmpSnapshot.class, new Consumer<Delta<BatteryTmpSnapshot>>() {
                        @Override
                        public void accept(Delta<BatteryTmpSnapshot> delta) {
                            onWritingSectionContent(delta, monitors, printer);
                        }
                    });
                }
            }

            protected boolean onWritingSectionContent(@NonNull Delta<?> sessionDelta, CompositeMonitors monitors, final Printer printer) {
                if (monitors.getMonitor() == null || monitors.getAppStats() == null) {
                    return false;
                }
                final AppStats appStats = monitors.getAppStats();
                // - Dump Jiffies
                if (sessionDelta.dlt instanceof JiffiesSnapshot) {
                    //noinspection unchecked
                    Delta<JiffiesSnapshot> delta = (Delta<JiffiesSnapshot>) sessionDelta;
                    // header
                    long minute = Math.max(1, delta.during / ONE_MIN);
                    long avgJiffies = delta.dlt.totalJiffies.get() / minute;
                    printer.append("| ").append("pid=").append(Process.myPid())
                            .tab().tab().append("fg=").append(BatteryCanaryUtil.convertAppStat(appStats.getAppStat()))
                            .tab().tab().append("during(min)=").append(minute)
                            .tab().tab().append("diff(jiffies)=").append(delta.dlt.totalJiffies.get())
                            .tab().tab().append("avg(jiffies/min)=").append(avgJiffies)
                            .enter();

                    // jiffies sections
                    printer.createSection("jiffies(" + delta.dlt.threadEntries.getList().size() + ")");
                    printer.writeLine("desc", "(status)name(tid)\tavg/total");
                    printer.writeLine("inc_thread_num", String.valueOf(delta.dlt.threadNum.get()));
                    printer.writeLine("cur_thread_num", String.valueOf(delta.end.threadNum.get()));
                    for (ThreadJiffiesEntry threadJiffies : delta.dlt.threadEntries.getList().subList(0, Math.min(delta.dlt.threadEntries.getList().size(), 8))) {
                        long entryJffies = threadJiffies.get();
                        printer.append("|   -> (").append(threadJiffies.isNewAdded ? "+" : "~").append("/").append(threadJiffies.stat).append(")")
                                .append(threadJiffies.name).append("(").append(threadJiffies.tid).append(")\t")
                                .append(entryJffies / minute).append("/").append(entryJffies).append("\tjiffies")
                                .append("\n");

                        // List<LooperTaskMonitorFeature.TaskTraceInfo> threadTasks = tasks.get(threadJiffies.tid);
                        // if (null != threadTasks && !threadTasks.isEmpty()) {
                        //     for (LooperTaskMonitorFeature.TaskTraceInfo task : threadTasks.subList(0, Math.min(3, threadTasks.size()))) {
                        //         printer.append("|\t\t").append(task).append("\n");
                        //     }
                        // }
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
                    monitors.getSamplingResult(CpuFreqSnapshot.class, new Consumer<Sampler.Result>() {
                        @Override
                        public void accept(Sampler.Result result) {
                            printer.createSubSection("cpufreq_sampling");
                            printer.writeLine(result.duringMillis + "(mls)\t" + (result.interval) + "(itv)");
                            printer.writeLine("max", String.valueOf(result.sampleMax));
                            printer.writeLine("min", String.valueOf(result.sampleMin));
                            printer.writeLine("avg", String.valueOf(result.sampleAvg));
                            printer.writeLine("cnt", String.valueOf(result.count));
                        }
                    });
                    return true;
                }

                // - Dump CpuStats
                if (sessionDelta.dlt instanceof CpuStateSnapshot) {
                    //noinspection unchecked
                    final Delta<CpuStateSnapshot> delta = (Delta<CpuStateSnapshot>) sessionDelta;
                    // Cpu Usage
                    printer.createSubSection("cpu_load");
                    printer.writeLine(delta.during + "(mls)\t" + (delta.during / ONE_MIN) + "(min)");
                    final CpuStatFeature cpuStatFeature = monitors.getFeature(CpuStatFeature.class);
                    if (cpuStatFeature != null) {
                        monitors.getDelta(JiffiesSnapshot.class, new Consumer<Delta<JiffiesSnapshot>>() {
                            @Override
                            public void accept(Delta<JiffiesSnapshot> jiffiesDelta) {
                                long appJiffiesDelta = jiffiesDelta.dlt.totalJiffies.get();
                                long cpuJiffiesDelta = delta.dlt.totalCpuJiffies();
                                float cpuLoad = (float) appJiffiesDelta / cpuJiffiesDelta;
                                float cpuLoadAvg = cpuLoad * cpuStatFeature.getPowerProfile().getCpuCoreNum();
                                printer.writeLine("usage", (int) (cpuLoadAvg * 100) + "%");
                            }
                        });
                    }
                    for (int i = 0; i < delta.dlt.cpuCoreStates.size(); i++) {
                        ListEntry<DigitEntry<Long>> listEntry = delta.dlt.cpuCoreStates.get(i);
                        printer.writeLine("cpu" + i, Arrays.toString(listEntry.getList().toArray()));
                    }
                    // BatterySip
                    if (cpuStatFeature != null) {
                        printer.createSubSection("cpu_sip");
                        // Cpu battery sip - CPU State
                        final PowerProfile powerProfile = cpuStatFeature.getPowerProfile();
                        printer.writeLine("inc_cpu_sip", String.format(Locale.US, "%.2f(mAh)", delta.dlt.configureCpuSip(powerProfile)));
                        printer.writeLine("cur_cpu_sip", String.format(Locale.US, "%.2f(mAh)", delta.end.configureCpuSip(powerProfile)));
                        // Cpu battery sip - Proc State
                        monitors.getDelta(JiffiesSnapshot.class, new Consumer<Delta<JiffiesSnapshot>>() {
                            @Override
                            public void accept(Delta<JiffiesSnapshot> jiffiesDelta) {
                                double procSipDelta = delta.dlt.configureProcSip(powerProfile, jiffiesDelta.dlt.totalJiffies.get());
                                double procSipEnd = delta.end.configureProcSip(powerProfile, jiffiesDelta.end.totalJiffies.get());
                                printer.writeLine("inc_prc_sip", String.format(Locale.US, "%.2f(mAh)", procSipDelta));
                                printer.writeLine("cur_prc_sip", String.format(Locale.US, "%.2f(mAh)", procSipEnd));
                                if (Double.isNaN(procSipDelta)) {
                                    double procSipBgn = delta.bgn.configureProcSip(powerProfile, jiffiesDelta.bgn.totalJiffies.get());
                                    printer.writeLine("inc_prc_sipr", String.format(Locale.US, "%.2f(mAh)", procSipEnd - procSipBgn));
                                }
                            }
                        });
                    }
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
                    monitors.getSamplingResult(BatteryTmpSnapshot.class, new Consumer<Sampler.Result>() {
                        @Override
                        public void accept(Sampler.Result result) {
                            printer.createSubSection("batt_temp_sampling");
                            printer.writeLine(result.duringMillis + "(mls)\t" + (result.interval) + "(itv)");
                            printer.writeLine("max", String.valueOf(result.sampleMax));
                            printer.writeLine("min", String.valueOf(result.sampleMin));
                            printer.writeLine("avg", String.valueOf(result.sampleAvg));
                            printer.writeLine("cnt", String.valueOf(result.count));
                        }
                    });
                    return true;
                }

                return false;
            }

            protected void onWritingAppStatSection(CompositeMonitors monitors, final Printer printer) {
                if (monitors.getMonitor() == null || monitors.getAppStats() == null) {
                    return;
                }

                AppStats appStats = monitors.getAppStats();
                printer.createSection("app_stats");
                printer.createSubSection("stat_time");
                printer.writeLine("time", appStats.getMinute() + "(min)");
                printer.writeLine("fg", String.valueOf(appStats.appFgRatio));
                printer.writeLine("bg", String.valueOf(appStats.appBgRatio));
                printer.writeLine("fgSrv", String.valueOf(appStats.appFgSrvRatio));
                printer.writeLine("devCharging", String.valueOf(appStats.devChargingRatio));
                printer.writeLine("devScreenOff", String.valueOf(appStats.devSceneOffRatio));
                if (!TextUtils.isEmpty(appStats.sceneTop1)) {
                    printer.writeLine("sceneTop1", appStats.sceneTop1 + "/" + appStats.sceneTop1Ratio);
                }
                if (!TextUtils.isEmpty(appStats.sceneTop2)) {
                    printer.writeLine("sceneTop2", appStats.sceneTop2 + "/" + appStats.sceneTop2Ratio);
                }
                monitors.getFeature(AppStatMonitorFeature.class, new Consumer<AppStatMonitorFeature>() {
                    @Override
                    public void accept(AppStatMonitorFeature feature) {
                        AppStatMonitorFeature.AppStatSnapshot currSnapshot = feature.currentAppStatSnapshot();
                        printer.createSubSection("run_time");
                        printer.writeLine("time", currSnapshot.uptime.get() / ONE_MIN + "(min)");
                        printer.writeLine("fg", String.valueOf(currSnapshot.fgRatio.get()));
                        printer.writeLine("bg", String.valueOf(currSnapshot.bgRatio.get()));
                        printer.writeLine("fgSrv", String.valueOf(currSnapshot.fgSrvRatio.get()));
                    }
                });
            }

        }

        /**
         * Log Printer
         */
        @SuppressWarnings("UnusedReturnValue")
        public static class Printer {
            private final StringBuilder sb = new StringBuilder();

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
                    MatrixLog.i(TAG, "%s", "\t\n" + sb.toString());
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
