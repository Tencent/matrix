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

package com.tencent.matrix.batterycanary.monitor.feature;

import android.app.Application;
import android.content.Context;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryEventDelegate;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.Callable;
import java.util.concurrent.atomic.AtomicInteger;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;


@RunWith(AndroidJUnit4.class)
public class SamplerTest {
    static final String TAG = "Matrix.test.SamplerTest";

    Context mContext;

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

    @Test
    public void testInvalidSamplingResult() {
        Number result = Integer.MIN_VALUE;
        Assert.assertTrue(result.equals(Integer.MIN_VALUE));
        Assert.assertEquals(Integer.MIN_VALUE, result);
    }

    @Test
    public void testSampling() throws InterruptedException {
        final BatteryMonitorCore monitor = mockMonitor();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(monitor.getConfig());
        Matrix.with().getPlugins().add(plugin);
        monitor.enableForegroundLoopCheck(true);
        monitor.start();

        final int samplingCount = 10;

        final AtomicInteger counter = new AtomicInteger(0);
        final MonitorFeature.Snapshot.Sampler sampler = new MonitorFeature.Snapshot.Sampler("test-1", monitor.getHandler(), new Callable<Integer>() {
            @Override
            public Integer call() throws InterruptedException {
                int count = counter.incrementAndGet();
                if (count <= samplingCount) {
                    return count;
                }
                synchronized (counter) {
                    counter.notifyAll();
                }
                throw new InterruptedException("stop");
            }
        });

        sampler.setInterval(1L);
        sampler.start();
        if (sampler.mCount < samplingCount) {
            synchronized (counter) {
                counter.wait();
            }
        }
        sampler.pause();
        MonitorFeature.Snapshot.Sampler.Result result = sampler.getResult();
        Assert.assertNotNull(result);
        Assert.assertEquals(samplingCount, result.count);
        Assert.assertEquals(1d, result.sampleFst, 0.1);
        Assert.assertEquals(10d, result.sampleLst, 0.1);
        Assert.assertEquals(10d, result.sampleMax, 0.1);
        Assert.assertEquals(10d, result.sampleMax, 0.1);
        Assert.assertEquals(1d, result.sampleMin, 0.1);
        Assert.assertEquals(55d / samplingCount, result.sampleAvg, 0.1);

        final AtomicInteger counterDec = new AtomicInteger(0);
        final MonitorFeature.Snapshot.Sampler samplerDec = new MonitorFeature.Snapshot.Sampler("test-2", monitor.getHandler(), new Callable<Integer>() {
            @Override
            public Integer call() throws InterruptedException {
                int count = counterDec.incrementAndGet();
                if (count <= samplingCount) {
                    return samplingCount + 1 - count;
                }
                synchronized (counterDec) {
                    counterDec.notifyAll();
                }
                throw new InterruptedException("stop");
            }
        });

        samplerDec.setInterval(1L);
        samplerDec.start();
        if (samplerDec.mCount < samplingCount) {
            synchronized (counterDec) {
                counterDec.wait();
            }
        }
        samplerDec.pause();
        result = samplerDec.getResult();
        Assert.assertNotNull(result);
        Assert.assertEquals(samplingCount, result.count);
        Assert.assertEquals(10d, result.sampleFst, 0.1);
        Assert.assertEquals(1d, result.sampleLst, 0.1);
        Assert.assertEquals(10d, result.sampleMax, 0.1);
        Assert.assertEquals(10d, result.sampleMax, 0.1);
        Assert.assertEquals(1d, result.sampleMin, 0.1);
        Assert.assertEquals(55d / samplingCount, result.sampleAvg, 0.1);
    }
}
