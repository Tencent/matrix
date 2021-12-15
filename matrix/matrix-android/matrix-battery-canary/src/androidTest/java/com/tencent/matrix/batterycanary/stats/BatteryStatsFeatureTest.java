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

package com.tencent.matrix.batterycanary.stats;

import android.app.Application;
import android.content.Context;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryCanary;
import com.tencent.matrix.batterycanary.BatteryEventDelegate;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.TestUtils;
import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCallback;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.AppStatMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.utils.Consumer;
import com.tencent.mmkv.MMKV;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;
import java.util.concurrent.CountDownLatch;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;


@RunWith(AndroidJUnit4.class)
public class BatteryStatsFeatureTest {
    static final String TAG = "Matrix.test.BatteryStatsFeatureTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        if (!Matrix.isInstalled()) {
            Matrix.init(new Matrix.Builder(((Application) mContext.getApplicationContext())).build());
        }
        if (!BatteryEventDelegate.isInit()) {
            BatteryEventDelegate.init((Application) mContext.getApplicationContext());
        }
        String rootDir = MMKV.initialize(mContext);
        System.out.println("mmkv root: " + rootDir);
    }

    @After
    public void shutDown() {
    }

    @Test
    public void testBatteryStatsWithMonitors() throws InterruptedException {
        if (TestUtils.isAssembleTest()) {
            return;
        }

        final CountDownLatch latch = new CountDownLatch(11);

        MMKV mmkv = MMKV.defaultMMKV();
        mmkv.clearAll();
        BatteryRecorder.MMKVRecorder recorder = new BatteryRecorder.MMKVRecorder(mmkv) {
            @Override
            public void write(String date, BatteryRecord record) {
                super.write(date, record);
                latch.countDown();
            }
        };
        recorder.clean(BatteryStatsFeature.getDateString(0), "main");

        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(JiffiesMonitorFeature.class)
                .enable(AppStatMonitorFeature.class)
                .enable(DeviceStatMonitorFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(false)
                .greyJiffiesTime(1)
                .enable(BatteryStatsFeature.class)
                .setRecorder(recorder)
                .setCallback(new BatteryMonitorCallback.BatteryPrinter())
                .build();

        final BatteryMonitorCore monitor = new BatteryMonitorCore(config);
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(monitor.getConfig());
        Matrix.with().getPlugins().add(plugin);
        monitor.enableForegroundLoopCheck(false);

        BatteryStatsFeature batteryStatsFeature = BatteryCanary.getMonitorFeature(BatteryStatsFeature.class);
        Assert.assertNotNull(batteryStatsFeature);
        List<BatteryRecord> records = batteryStatsFeature.readRecords(0, "main");
        Assert.assertTrue(records.isEmpty());

        monitor.start();

        batteryStatsFeature.onForeground(true);
        batteryStatsFeature.statsScene("UI 1");
        batteryStatsFeature.statsScene("UI 2");
        batteryStatsFeature.statsScene("UI 3");
        batteryStatsFeature.onForeground(false);
        batteryStatsFeature.statsDevStat(AppStats.DEV_STAT_CHARGING);
        batteryStatsFeature.statsDevStat(AppStats.DEV_STAT_SCREEN_OFF);
        batteryStatsFeature.statsDevStat(AppStats.DEV_STAT_UN_CHARGING);
        batteryStatsFeature.onForeground(true);

        monitor.stop();

        latch.await();
        BatteryCanary.getMonitorFeature(BatteryStatsFeature.class, new Consumer<BatteryStatsFeature>() {
            @Override
            public void accept(BatteryStatsFeature batteryStatsFeature) {
                List<BatteryRecord> records = batteryStatsFeature.readRecords(0, "main");
                Assert.assertEquals("Records list: " + records,11, records.size());
            }
        });
    }
}
