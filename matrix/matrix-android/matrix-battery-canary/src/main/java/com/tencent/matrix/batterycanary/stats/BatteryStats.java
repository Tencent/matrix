package com.tencent.matrix.batterycanary.stats;

import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.monitor.feature.BlueToothMonitorFeature.BlueToothSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.CompositeMonitors;
import com.tencent.matrix.batterycanary.monitor.feature.CpuStatFeature;
import com.tencent.matrix.batterycanary.monitor.feature.CpuStatFeature.CpuStateSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature.BatteryTmpSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot.ThreadJiffiesEntry;
import com.tencent.matrix.batterycanary.monitor.feature.LocationMonitorFeature.LocationSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.batterycanary.monitor.feature.WifiMonitorFeature.WifiSnapshot;
import com.tencent.matrix.batterycanary.stats.BatteryRecord.ReportRecord.EntryInfo;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.Consumer;
import com.tencent.matrix.batterycanary.utils.PowerProfile;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.Locale;

/**
 * @author Kaede
 * @since 2021/12/15
 */
public interface BatteryStats {

    BatteryRecord.AppStatRecord statsAppStat(int appStat);

    BatteryRecord.DevStatRecord statsDevStat(int devStat);

    BatteryRecord.SceneStatRecord statsScene(String scene);

    BatteryRecord.EventStatRecord statsEvent(String event);

    BatteryRecord.ReportRecord statsMonitors(final CompositeMonitors monitors);


    class BatteryStatsImpl implements BatteryStats {

        @Override
        public BatteryRecord.AppStatRecord statsAppStat(int appStat) {
            BatteryRecord.AppStatRecord statRecord = new BatteryRecord.AppStatRecord();
            statRecord.appStat = appStat;
            return statRecord;
        }

        @Override
        public BatteryRecord.DevStatRecord statsDevStat(int devStat) {
            BatteryRecord.DevStatRecord statRecord = new BatteryRecord.DevStatRecord();
            statRecord.devStat = devStat;
            return statRecord;
        }

        @Override
        public BatteryRecord.SceneStatRecord statsScene(String scene) {
            BatteryRecord.SceneStatRecord statRecord = new BatteryRecord.SceneStatRecord();
            statRecord.scene = scene;
            return statRecord;
        }

        @Override
        public BatteryRecord.EventStatRecord statsEvent(String event) {
            BatteryRecord.EventStatRecord statRecord = new BatteryRecord.EventStatRecord();
            statRecord.event = event;
            return statRecord;
        }

        @Override
        public BatteryRecord.ReportRecord statsMonitors(final CompositeMonitors monitors) {
            final BatteryRecord.ReportRecord statRecord = new BatteryRecord.ReportRecord();
            final AppStats appStats = monitors.getAppStats();
            if (appStats == null) {
                return statRecord;
            }

            statRecord.scope = monitors.getScope();
            statRecord.windowMillis = appStats.duringMillis;

            // Thread Entry
            final Delta<JiffiesSnapshot> jiffiesDelta = monitors.getDelta(JiffiesSnapshot.class);
            if (jiffiesDelta != null) {
                statRecord.threadInfoList = new ArrayList<>();
                for (ThreadJiffiesEntry threadJiffies : jiffiesDelta.dlt.threadEntries.getList().subList(0, Math.min(jiffiesDelta.dlt.threadEntries.getList().size(), 5))) {
                    BatteryRecord.ReportRecord.ThreadInfo threadInfo = new BatteryRecord.ReportRecord.ThreadInfo();
                    threadInfo.stat = threadJiffies.stat;
                    threadInfo.tid = threadJiffies.tid;
                    threadInfo.name = threadJiffies.name;
                    threadInfo.jiffies = threadJiffies.get();
                    statRecord.threadInfoList.add(threadInfo);
                }
            }

            statRecord.entryList = new ArrayList<>();

            // DevStat Entry
            createEntryInfo(new Consumer<EntryInfo>() {
                @Override
                public void accept(final EntryInfo entryInfo) {
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
                            monitors.getSamplingResult(BatteryTmpSnapshot.class, new Consumer<MonitorFeature.Snapshot.Sampler.Result>() {
                                @Override
                                public void accept(MonitorFeature.Snapshot.Sampler.Result result) {
                                    double maxTemp = result.sampleMax;
                                    double minTemp = result.sampleMin;
                                    entryInfo.entries.put("最大电池温度", String.format(Locale.US, "%.1f ℃", maxTemp / 10.0f));
                                    entryInfo.entries.put("电池温度变化", String.format(Locale.US, "%.1f ℃", (maxTemp - minTemp) / 10.0f));
                                }
                            });
                        }
                    });

                    statRecord.entryList.add(entryInfo);
                }
            });

            // AppStat Entry
            createEntryInfo(new Consumer<EntryInfo>() {
                @Override
                public void accept(final EntryInfo entryInfo) {
                    entryInfo.name = "App 状态";
                    entryInfo.entries = new LinkedHashMap<>();

                    entryInfo.entries.put("前台时间占比", appStats.appFgRatio + "%");
                    entryInfo.entries.put("后台时间占比", appStats.appBgRatio + "%");
                    entryInfo.entries.put("前台服务时间占比", appStats.appFgSrvRatio + "%");
                    entryInfo.entries.put("充电时间占比", appStats.devChargingRatio + "%");
                    entryInfo.entries.put("息屏时间占比 (排除充电)", appStats.devSceneOffRatio + "%");

                    statRecord.entryList.add(entryInfo);
                }
            });

            // SystemService Entry
            createEntryInfo(new Consumer<EntryInfo>() {
                @Override
                public void accept(final EntryInfo entryInfo) {
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

                    statRecord.entryList.add(entryInfo);
                }
            });

            return statRecord;
        }

        protected void createEntryInfo(Consumer<EntryInfo> consumer) {
            EntryInfo entryInfo = new EntryInfo();
            consumer.accept(entryInfo);
        }
    }
}
