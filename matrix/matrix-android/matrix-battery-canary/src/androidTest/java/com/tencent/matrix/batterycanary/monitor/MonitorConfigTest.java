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

package com.tencent.matrix.batterycanary.monitor;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;

import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.LooperTaskMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature;
import com.tencent.matrix.plugin.Plugin;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;


@RunWith(AndroidJUnit4.class)
public class MonitorConfigTest {
    static final String TAG = "Matrix.test.foo";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
    }

    @After
    public void shutDown() {
    }

    @Test
    public void testCreateConfig() {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder().build();
        Assert.assertTrue(config.isBuiltinForegroundNotifyEnabled);
        Assert.assertTrue(config.isForegroundModeEnabled);
        Assert.assertEquals(BatteryMonitorConfig.DEF_WAKELOCK_TIMEOUT, config.wakelockTimeout);
        Assert.assertEquals(BatteryMonitorConfig.DEF_JIFFIES_DELAY, config.greyTime);
        Assert.assertEquals(BatteryMonitorConfig.DEF_FOREGROUND_SCHEDULE_TIME, config.foregroundLoopCheckTime);
        Assert.assertSame(BatteryMonitorCallback.BatteryPrinter.class, config.callback.getClass());

        BatteryMonitorCore core = new BatteryMonitorCore(config);
        Assert.assertNull(core.getMonitorFeature(JiffiesMonitorFeature.class));
        Assert.assertNull(core.getMonitorFeature(WakeLockMonitorFeature.class));
        Assert.assertNull(core.getMonitorFeature(LooperTaskMonitorFeature.class));

        BatteryMonitorCallback.BatteryPrinter callback = new BatteryMonitorCallback.BatteryPrinter();
        config = new BatteryMonitorConfig.Builder()
                .enable(JiffiesMonitorFeature.class)
                .enable(WakeLockMonitorFeature.class)
                .enable(LooperTaskMonitorFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(false)
                .setCallback(callback)
                .wakelockTimeout(2 * 60 * 1000)
                .greyJiffiesTime(30 * 1000)
                .foregroundLoopCheckTime(20 * 60 * 1000)
                .build();
        Assert.assertFalse(config.isBuiltinForegroundNotifyEnabled);
        Assert.assertFalse(config.isForegroundModeEnabled);
        Assert.assertEquals(2 * 60 * 1000, config.wakelockTimeout);
        Assert.assertEquals(30 * 1000, config.greyTime);
        Assert.assertEquals(20 * 60 * 1000, config.foregroundLoopCheckTime);
        Assert.assertSame(callback, config.callback);

        core = new BatteryMonitorCore(config);
        Assert.assertNotNull(core.getMonitorFeature(JiffiesMonitorFeature.class));
        Assert.assertNotNull(core.getMonitorFeature(WakeLockMonitorFeature.class));
        Assert.assertNotNull(core.getMonitorFeature(LooperTaskMonitorFeature.class));
    }
}
