package com.tencent.matrix.batterycanary.stats;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Process;

import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.monitor.feature.AbsMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.BlueToothMonitorFeature.BlueToothSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.CompositeMonitors;
import com.tencent.matrix.batterycanary.monitor.feature.CpuStatFeature;
import com.tencent.matrix.batterycanary.monitor.feature.CpuStatFeature.CpuStateSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature.BatteryTmpSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot.ThreadJiffiesEntry;
import com.tencent.matrix.batterycanary.monitor.feature.LocationMonitorFeature.LocationSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.batterycanary.monitor.feature.WifiMonitorFeature.WifiSnapshot;
import com.tencent.matrix.batterycanary.stats.BatteryRecord.ReportRecord;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.Consumer;
import com.tencent.matrix.batterycanary.utils.PowerProfile;
import com.tencent.matrix.util.MatrixHandlerThread;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;

import androidx.annotation.WorkerThread;

/**
 * @author Kaede
 * @since 2021/12/3
 */
public final class BatteryStatsFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.BatteryStats";

    private HandlerThread mStatsThread;
    private Handler mStatsHandler;
    private BatteryRecorder mBatteryRecorder;

    @Override
    public int weight() {
        return 0;
    }

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    public void onTurnOn() {
        super.onTurnOn();
        mBatteryRecorder = mCore.getConfig().batteryRecorder;
        if (mBatteryRecorder != null) {
            mStatsThread = MatrixHandlerThread.getNewHandlerThread("matrix-stats", Thread.NORM_PRIORITY);
            mStatsHandler = new Handler(mStatsThread.getLooper());
        }

        BatteryRecord.ProcStatRecord procStatRecord = new BatteryRecord.ProcStatRecord();
        procStatRecord.pid = Process.myPid();
        procStatRecord.procStat = BatteryRecord.ProcStatRecord.STAT_PROC_LAUNCH;
        writeRecord(procStatRecord);
    }

    @Override
    public void onForeground(boolean isForeground) {
        super.onForeground(isForeground);
        statsAppStat(isForeground ? AppStats.APP_STAT_FOREGROUND : AppStats.APP_STAT_BACKGROUND);
    }

    @Override
    public void onTurnOff() {
        super.onTurnOff();
        BatteryRecord.ProcStatRecord procStatRecord = new BatteryRecord.ProcStatRecord();
        procStatRecord.pid = Process.myPid();
        procStatRecord.procStat = BatteryRecord.ProcStatRecord.STAT_PROC_OFF;
        writeRecord(procStatRecord);

        if (mStatsHandler != null) {
            mStatsHandler.post(new Runnable() {
                @Override
                public void run() {
                    if (mStatsThread != null) {
                        mStatsThread.quit();
                    }
                }
            });
        }
    }

    public void writeRecord(final BatteryRecord record) {
        if (mBatteryRecorder != null) {
            if (mStatsHandler != null) {
                final String date = getDateString(0);
                mStatsHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        writeRecord(date, record);
                    }
                });
            }
        }
    }

    @WorkerThread
    public List<BatteryRecord> readRecords(int dayOffset) {
        if (mBatteryRecorder != null) {
            final String date = getDateString(dayOffset);
            return mBatteryRecorder.read(date);
        }
        return Collections.emptyList();
    }

    @WorkerThread
    public BatteryRecords readBatteryRecords(int dayOffset) {
        BatteryRecords batteryRecords = new BatteryRecords();
        batteryRecords.date = getDateString(dayOffset);
        batteryRecords.records = readRecords(dayOffset);
        return batteryRecords;
    }

    @WorkerThread
    void writeRecord(String date, BatteryRecord record) {
        if (mBatteryRecorder != null) {
            mBatteryRecorder.write(date, record);
        }
    }

    @WorkerThread
    List<BatteryRecord> readRecords(String date) {
        if (mBatteryRecorder != null) {
            return mBatteryRecorder.read(date);
        }
        return Collections.emptyList();
    }

    @WorkerThread
    void cleanRecords(String date) {
        if (mBatteryRecorder != null) {
            mBatteryRecorder.clean(date);
        }
    }

    public void statsAppStat(int appStat) {
        BatteryRecord.AppStatRecord statRecord = new BatteryRecord.AppStatRecord();
        statRecord.appStat = appStat;
        writeRecord(statRecord);
    }

    public void statsDevStat(int devStat) {
        BatteryRecord.DevStatRecord statRecord = new BatteryRecord.DevStatRecord();
        statRecord.devStat = devStat;
        writeRecord(statRecord);
    }

    public void statsScene(String scene) {
        BatteryRecord.SceneStatRecord statRecord = new BatteryRecord.SceneStatRecord();
        statRecord.scene = scene;
        writeRecord(statRecord);
    }

    public void statsEvent(String event) {
        BatteryRecord.EventStatRecord statRecord = new BatteryRecord.EventStatRecord();
        statRecord.event = event;
        writeRecord(statRecord);
    }

    public void statsMonitors(final CompositeMonitors monitors) {
        if (mBatteryRecorder == null) {
            return;
        }
        final AppStats appStats = monitors.getAppStats();
        if (appStats == null) {
            return;
        }

        final ReportRecord reportRecord = new ReportRecord();
        reportRecord.scope = monitors.getScope();
        reportRecord.windowMillis = appStats.duringMillis;

        // Thread Entry
        final Delta<JiffiesSnapshot> jiffiesDelta = monitors.getDelta(JiffiesSnapshot.class);
        if (jiffiesDelta != null) {
            reportRecord.threadInfoList = new ArrayList<>();
            for (ThreadJiffiesEntry threadJiffies : jiffiesDelta.dlt.threadEntries.getList().subList(0, Math.min(jiffiesDelta.dlt.threadEntries.getList().size(), 5))) {
                ReportRecord.ThreadInfo threadInfo = new ReportRecord.ThreadInfo();
                threadInfo.stat = threadJiffies.stat;
                threadInfo.tid = threadJiffies.tid;
                threadInfo.name = threadJiffies.name;
                threadInfo.jiffies = threadJiffies.get();
                reportRecord.threadInfoList.add(threadInfo);
            }
        }

        reportRecord.entryList = new ArrayList<>();

        // DevStat Entry
        createEntryInfo(new Consumer<ReportRecord.EntryInfo>() {
            @Override
            public void accept(final ReportRecord.EntryInfo entryInfo) {
                entryInfo.name = "设备状态";
                entryInfo.entries = new LinkedHashMap<>();

                // cpu load & cpu sip
                monitors.getFeature(CpuStatFeature.class, new Consumer<CpuStatFeature>() {
                    @Override
                    public void accept(final CpuStatFeature cpuStatFeature) {
                        monitors.getDelta(CpuStateSnapshot.class, new Consumer<Delta<CpuStateSnapshot>>() {
                            @Override
                            public void accept(Delta<CpuStateSnapshot> delta) {
                                if (jiffiesDelta != null) {
                                    long appJiffiesDelta = jiffiesDelta.dlt.totalJiffies.get();
                                    long cpuJiffiesDelta = delta.dlt.totalCpuJiffies();
                                    float cpuLoad = (float) appJiffiesDelta / cpuJiffiesDelta;
                                    float cpuLoadAvg = cpuLoad * BatteryCanaryUtil.getCpuCoreNum();
                                    entryInfo.entries.put("Cpu Load", (int) (cpuLoadAvg * 100) + "%");

                                    final PowerProfile powerProfile = cpuStatFeature.getPowerProfile();
                                    double procSipBgn = delta.bgn.configureProcSip(powerProfile, jiffiesDelta.bgn.totalJiffies.get());
                                    double procSipEnd = delta.end.configureProcSip(powerProfile, jiffiesDelta.end.totalJiffies.get());
                                    entryInfo.entries.put("Cpu Power", String.format(Locale.US, "%.2f mAh", procSipEnd - procSipBgn));
                                }
                            }
                        });
                    }
                });

                // temperature
                monitors.getDelta(BatteryTmpSnapshot.class, new Consumer<Delta<BatteryTmpSnapshot>>() {
                    @Override
                    public void accept(Delta<BatteryTmpSnapshot> delta) {
                        double currTemp = delta.end.temp.get();
                        entryInfo.entries.put("当前电池温度", String.format(Locale.US, "%.1f ℃", currTemp / 10.0f));
                        monitors.getSamplingResult(BatteryTmpSnapshot.class, new Consumer<Snapshot.Sampler.Result>() {
                            @Override
                            public void accept(Snapshot.Sampler.Result result) {
                                double maxTemp = result.sampleMax;
                                double minTemp = result.sampleMin;
                                entryInfo.entries.put("最大电池温度", String.format(Locale.US, "%.1f ℃", maxTemp / 10.0f));
                                entryInfo.entries.put("电池温度变化", String.format(Locale.US, "%.1f ℃", (maxTemp - minTemp) / 10.0f));
                            }
                        });
                    }
                });

                reportRecord.entryList.add(entryInfo);
            }
        });

        // AppStat Entry
        createEntryInfo(new Consumer<ReportRecord.EntryInfo>() {
            @Override
            public void accept(final ReportRecord.EntryInfo entryInfo) {
                entryInfo.name = "App 状态";
                entryInfo.entries = new LinkedHashMap<>();

                entryInfo.entries.put("前台时间占比", appStats.appFgRatio + "%");
                entryInfo.entries.put("后台时间占比", appStats.appBgRatio + "%");
                entryInfo.entries.put("前台服务时间占比", appStats.appFgSrvRatio + "%");
                entryInfo.entries.put("充电时间占比", appStats.devChargingRatio + "%");
                entryInfo.entries.put("息屏时间占比 (排除充电)", appStats.devSceneOffRatio + "%");

                reportRecord.entryList.add(entryInfo);
            }
        });

        // SystemService Entry
        createEntryInfo(new Consumer<ReportRecord.EntryInfo>() {
            @Override
            public void accept(final ReportRecord.EntryInfo entryInfo) {
                entryInfo.name = "系统服务调用";
                entryInfo.entries = new LinkedHashMap<>();

                monitors.getDelta(BlueToothSnapshot.class, new Consumer<Delta<BlueToothSnapshot>>() {
                    @Override
                    public void accept(Delta<BlueToothSnapshot> delta) {
                        entryInfo.entries.put("BlueTooth 扫描", String.format(Locale.US, "register %s, discovery %s, scan %s 次", delta.dlt.regsCount.get(), delta.dlt.discCount.get(), delta.dlt.scanCount.get()));
                    }
                });
                monitors.getDelta(WifiSnapshot.class, new Consumer<Delta<WifiSnapshot>>() {
                    @Override
                    public void accept(Delta<WifiSnapshot> delta) {
                        entryInfo.entries.put("Wifi 扫描", String.format(Locale.US, "query %s, scan %s 次", delta.dlt.queryCount.get(), delta.dlt.scanCount.get()));
                    }
                });
                monitors.getDelta(LocationSnapshot.class, new Consumer<Delta<LocationSnapshot>>() {
                    @Override
                    public void accept(Delta<LocationSnapshot> delta) {
                        entryInfo.entries.put("GPS 扫描", String.format(Locale.US, "scan %s 次", delta.dlt.scanCount.get()));
                    }
                });

                reportRecord.entryList.add(entryInfo);
            }
        });

        writeRecord(reportRecord);
    }

    private void createEntryInfo(Consumer<ReportRecord.EntryInfo> consumer) {
        ReportRecord.EntryInfo entryInfo = new ReportRecord.EntryInfo();
        consumer.accept(entryInfo);
    }


    public static String getDateString(int dayOffset) {
        DateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd", Locale.getDefault());
        Calendar cal = Calendar.getInstance();
        cal.add(Calendar.DATE, dayOffset);
        return dateFormat.format(cal.getTime());
    }


    public static class BatteryRecords {
        public String date;
        public List<BatteryRecord> records;
    }
}
