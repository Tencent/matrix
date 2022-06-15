/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.tencent.matrix.batterycanary;

import android.app.Application;
import android.content.Context;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.AlarmMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.AppStatMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.BlueToothMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.CompositeMonitors;
import com.tencent.matrix.batterycanary.monitor.feature.CpuStatFeature;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.LocationMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.LooperTaskMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.NotificationMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.TrafficMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WifiMonitorFeature;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.Consumer;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;


@RunWith(AndroidJUnit4.class)
public class Examples {
    static final String TAG = "Matrix.test.Examples";

    Context mContext;

    /**
     * 计算 Cpu Load
     */
    @Test
    public void exampleForCpuLoad() {
        if (TestUtils.isAssembleTest()) {
            return;
        } else {
            mockSetup();
        }

        if (Matrix.isInstalled()) {
            BatteryMonitorPlugin monitor = Matrix.with().getPluginByClass(BatteryMonitorPlugin.class);
            if (monitor != null) {
                CompositeMonitors compositor = new CompositeMonitors(monitor.core());
                compositor.metric(JiffiesMonitorFeature.JiffiesSnapshot.class);
                compositor.start();

                doSomething();

                compositor.finish();
                int cpuLoad = compositor.getCpuLoad();
                Assert.assertTrue("cpuLoad: " + cpuLoad, cpuLoad >= 0);
            }
        }
    }

    /**
     * CpuFreq 采样
     */
    @Test
    public void exampleForCpuFreqSampling() {
        if (TestUtils.isAssembleTest()) {
            return;
        } else {
            mockSetup();
        }

        if (Matrix.isInstalled()) {
            BatteryMonitorPlugin monitor = Matrix.with().getPluginByClass(BatteryMonitorPlugin.class);
            if (monitor != null) {
                CompositeMonitors compositor = new CompositeMonitors(monitor.core());
                compositor.sample(DeviceStatMonitorFeature.CpuFreqSnapshot.class, 10L);
                compositor.start();

                doSomething();

                compositor.finish();
                MonitorFeature.Snapshot.Sampler.Result result = compositor.getSamplingResult(DeviceStatMonitorFeature.CpuFreqSnapshot.class);
                Assert.assertNotNull(result);
                Assert.assertTrue(result.sampleAvg > 0);
            }
        }
    }

    /**
     * 计算 Cpu Load（叠加 CpuFreq 采样权重）
     */
    @Test
    public void exampleForCpuLoadNormalize() {
        if (TestUtils.isAssembleTest()) {
            return;
        } else {
            mockSetup();
        }

        if (Matrix.isInstalled()) {
            BatteryMonitorPlugin monitor = Matrix.with().getPluginByClass(BatteryMonitorPlugin.class);
            if (monitor != null) {
                CompositeMonitors compositor = new CompositeMonitors(monitor.core());
                compositor.metric(JiffiesMonitorFeature.JiffiesSnapshot.class);
                compositor.sample(DeviceStatMonitorFeature.CpuFreqSnapshot.class, 10L);
                compositor.start();

                doSomething();

                compositor.finish();
                int cpuLoad = compositor.getCpuLoad();
                Assert.assertTrue("cpuLoad: " + cpuLoad, cpuLoad >= 0);

                MonitorFeature.Snapshot.Sampler.Result result = compositor.getSamplingResult(DeviceStatMonitorFeature.CpuFreqSnapshot.class);
                List<int[]> cpuFreqSteps = BatteryCanaryUtil.getCpuFreqSteps();
                long sumMax = 0;
                for (int[] item : cpuFreqSteps) {
                    sumMax += item[item.length - 1];
                }
                Assert.assertTrue("cpuFreqSumAvg: " + result.sampleAvg + "vs cpuFreqSumMax: " + sumMax, sumMax >= result.sampleAvg);
                int cpuLoadNormalized  = (int) (cpuLoad * result.sampleAvg / sumMax);
                Assert.assertTrue("cpuLoadNormalized: " + cpuLoadNormalized + "vs cpuLoad: " + sumMax, cpuLoad >= cpuLoadNormalized);

                Assert.assertEquals(cpuLoadNormalized, compositor.getNorCpuLoad());
            }
        }
    }

    /**
     * 电量温度采样
     */
    @Test
    public void exampleForTemperatureSampling() {
        if (TestUtils.isAssembleTest()) {
            return;
        } else {
            mockSetup();
        }

        if (Matrix.isInstalled()) {
            BatteryMonitorPlugin monitor = Matrix.with().getPluginByClass(BatteryMonitorPlugin.class);
            if (monitor != null) {
                CompositeMonitors compositor = new CompositeMonitors(monitor.core());
                compositor.sample(DeviceStatMonitorFeature.BatteryTmpSnapshot.class, 10L);
                compositor.start();

                doSomething();

                compositor.finish();
                MonitorFeature.Snapshot.Sampler.Result result = compositor.getSamplingResult(DeviceStatMonitorFeature.BatteryTmpSnapshot.class);
                Assert.assertNotNull(result);
                Assert.assertTrue(result.sampleAvg > 0);
            }
        }
    }

    /**
     * 综合监控示例
     */
    @Test
    public void exampleForGeneralUseCase() {
        if (TestUtils.isAssembleTest()) {
            return;
        } else {
            mockSetup();
        }

        if (Matrix.isInstalled()) {
            BatteryMonitorPlugin monitor = Matrix.with().getPluginByClass(BatteryMonitorPlugin.class);
            if (monitor != null) {
                // 设置监控器
                CompositeMonitors compositor = new CompositeMonitors(monitor.core())
                        .metric(JiffiesMonitorFeature.JiffiesSnapshot.class)
                        .metric(CpuStatFeature.CpuStateSnapshot.class)
                        .sample(DeviceStatMonitorFeature.BatteryTmpSnapshot.class)
                        .sample(DeviceStatMonitorFeature.CpuFreqSnapshot.class, 10L);

                // 开始监控
                compositor.start();

                doSomething();

                // 结束监控
                compositor.finish();

                // 获取监控数据:
                // 1. 获取 App & Dev 状态
                compositor.getAppStats(new Consumer<AppStats>() {
                    @Override
                    public void accept(AppStats appStats) {
                        if (appStats.isValid) {
                            long minute = appStats.getMinute();  // 时间窗口(分钟)

                            int appStat = appStats.getAppStat();     // App 状态
                            int fgRatio = appStats.appFgRatio;       // 前台时间占比
                            int bgRatio = appStats.appBgRatio;       // 后台时间占比
                            int fgSrvRatio = appStats.appFgSrvRatio; // 前台服务时间占比
                            int floatRatio = appStats.appFloatRatio; // 浮窗时间占比

                            int devStat = appStats.getDevStat();               // Device 状态
                            int unChargingRatio = appStats.devUnChargingRatio; // 未充电状态时间占比
                            int screenOff = appStats.devSceneOffRatio;         // 息屏状态时间占比
                            int lowEnergyRatio = appStats.devLowEnergyRatio;   // 低电耗状态时间占比
                            int chargingRatio = appStats.devChargingRatio;     // 充电状态时间占比

                            String scene = appStats.sceneTop1;         // Top1 Activity
                            int sceneRatio = appStats.sceneTop1Ratio;  // Top1 Activity 占比
                            String scene2 = appStats.sceneTop2;        // Top2 Activity
                            int scene2Ratio = appStats.sceneTop2Ratio; // Top2 Activity 占比

                            if (appStats.isForeground()) {
                                // 监控期间 App 是否前台
                            }
                            if (appStats.isCharging()) {
                                // 监控期间 Dev 是否充电
                            }
                        }
                    }
                });

                // 2. 获取采样数据
                // 2.1 电池温度
                compositor.getSamplingResult(DeviceStatMonitorFeature.BatteryTmpSnapshot.class, new Consumer<MonitorFeature.Snapshot.Sampler.Result>() {
                    @Override
                    public void accept(MonitorFeature.Snapshot.Sampler.Result sampling) {
                        long duringMillis = sampling.duringMillis; // 时间窗口
                        long interval = sampling.interval   ;      // 采样周期
                        int count = sampling.count;                // 采样次数
                        double sampleFst = sampling.sampleFst;     // 第一次采样
                        double sampleLst = sampling.sampleLst;     // 最后一次采样
                        double sampleMax = sampling.sampleMax;     // 最大采样值
                        double sampleMin = sampling.sampleMin;     // 最小采样值
                        double sampleAvg = sampling.sampleAvg;     // 平均采样值
                    }
                });
                // 2.3 CpuFreq
                compositor.getSamplingResult(DeviceStatMonitorFeature.BatteryTmpSnapshot.class, new Consumer<MonitorFeature.Snapshot.Sampler.Result>() {
                    @Override
                    public void accept(MonitorFeature.Snapshot.Sampler.Result sampling) {
                        long duringMillis = sampling.duringMillis; // 时间窗口
                        long interval = sampling.interval   ;      // 采样周期
                        int count = sampling.count;                // 采样次数
                        double sampleFst = sampling.sampleFst;     // 第一次采样
                        double sampleLst = sampling.sampleLst;     // 最后一次采样
                        double sampleMax = sampling.sampleMax;     // 最大采样值
                        double sampleMin = sampling.sampleMin;     // 最小采样值
                        double sampleAvg = sampling.sampleAvg;     // 平均采样值
                    }
                });

                // 3. 进程 & 线程 Cpu Load
                // 3.1 进程 Cpu Load, 值区间 [0, Cpu Core Num * 100]
                final int procCpuLoad = compositor.getCpuLoad();
                // 3.2 获取线程数据
                compositor.getDelta(JiffiesMonitorFeature.JiffiesSnapshot.class, new Consumer<MonitorFeature.Snapshot.Delta<JiffiesMonitorFeature.JiffiesSnapshot>>() {
                    @Override
                    public void accept(MonitorFeature.Snapshot.Delta<JiffiesMonitorFeature.JiffiesSnapshot> procDelta) {
                        long totalJiffies = procDelta.dlt.totalJiffies.get();
                        for (JiffiesMonitorFeature.JiffiesSnapshot.ThreadJiffiesEntry threadEntry : procDelta.dlt.threadEntries.getList()) {
                            String name = threadEntry.name;   // 线程名
                            int tid = threadEntry.tid;        // tid
                            String status = threadEntry.stat; // 线程状态
                            long jiffies = threadEntry.get(); // 线程在这段时间内的 Jiffies
                            int threadCpuLoad = (int) (procCpuLoad * ((float) jiffies / totalJiffies));
                        }
                    }
                });
            }
        }
    }

    private void doSomething() {
        Thread Thread = new Thread(new Runnable() {
            @Override
            public void run() {
                for (;;) {}
            }
        }, "CpuLoadTest");
        try {
            Thread.sleep(2000L);
        } catch (InterruptedException ignored) {
        }
    }

    @Before
    public void setUp() {
        System.setProperty("org.mockito.android.target", ApplicationProvider.getApplicationContext().getCacheDir().getPath());
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        if (!Matrix.isInstalled()) {
            Matrix.init(new Matrix.Builder(((Application) mContext.getApplicationContext())).build());
        }
        if (!BatteryEventDelegate.isInit()) {
            BatteryEventDelegate.init((Application) mContext.getApplicationContext());
        }
    }

    @After
    public void shutDown() {
    }

    private void mockSetup() {
        final BatteryMonitorCore monitor = mockMonitor();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(monitor.getConfig());
        Matrix.with().getPlugins().add(plugin);
        monitor.enableForegroundLoopCheck(true);
        monitor.start();
    }

    private BatteryMonitorCore mockMonitor() {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(JiffiesMonitorFeature.class)
                .enable(LooperTaskMonitorFeature.class)
                .enable(WakeLockMonitorFeature.class)
                .enable(DeviceStatMonitorFeature.class)
                .enable(AlarmMonitorFeature.class)
                .enable(AppStatMonitorFeature.class)
                .enable(BlueToothMonitorFeature.class)
                .enable(WifiMonitorFeature.class)
                .enable(LocationMonitorFeature.class)
                .enable(TrafficMonitorFeature.class)
                .enable(NotificationMonitorFeature.class)
                .enable(CpuStatFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(true)
                .wakelockTimeout(1000)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(1000)
                .build();
        return new BatteryMonitorCore(config);
    }
}
