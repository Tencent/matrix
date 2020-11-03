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

package com.tencent.matrix.batterycanary.utils;

import android.app.Application;
import android.content.Context;
import android.os.IBinder;
import android.os.PowerManager;
import android.os.WorkSource;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;
import android.text.TextUtils;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.lang.reflect.Field;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;


@RunWith(AndroidJUnit4.class)
public class CanaryUtilsTest {
    static final String TAG = "Matrix.test.MonitorFeatureWakeLockTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
        if (!Matrix.isInstalled()) {
            Matrix.init(new Matrix.Builder(((Application) mContext.getApplicationContext())).build());
        }
    }

    @After
    public void shutDown() {
    }

    @Test
    public void testGetProcName() {
        try {
            BatteryCanaryUtil.getProcessName();
            Assert.fail("should fail");
        } catch (Exception e) {
            Assert.assertTrue(e instanceof IllegalStateException);
        }

        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(WakeLockMonitorFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(false)
                .wakelockTimeout(1000)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(1000)
                .build();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(config);
        plugin.init(((Application)mContext.getApplicationContext()), null);
        Matrix.with().getPlugins().add(plugin);

        String processName = BatteryCanaryUtil.getProcessName();
        Assert.assertFalse(TextUtils.isEmpty(processName));
        Assert.assertEquals(mContext.getPackageName(), processName);
    }

    @Test
    public void testGetPkgName() {
        try {
            BatteryCanaryUtil.getPackageName();
            Assert.fail("should fail");
        } catch (Exception e) {
            Assert.assertTrue(e instanceof IllegalStateException);
        }

        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(WakeLockMonitorFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(false)
                .wakelockTimeout(1000)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(1000)
                .build();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(config);
        plugin.init(((Application)mContext.getApplicationContext()), null);
        Matrix.with().getPlugins().add(plugin);

        String pkg = BatteryCanaryUtil.getPackageName();
        Assert.assertFalse(TextUtils.isEmpty(pkg));
        Assert.assertEquals(mContext.getPackageName(), pkg);
    }

    @Test
    public void testGetThrowableStack() {
        try {
            BatteryCanaryUtil.getThrowableStack(new Throwable());
            Assert.fail("should fail");
        } catch (Exception e) {
            Assert.assertTrue(e instanceof IllegalStateException);
        }

        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(WakeLockMonitorFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(false)
                .wakelockTimeout(1000)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(1000)
                .build();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(config);
        plugin.init(((Application)mContext.getApplicationContext()), null);
        Matrix.with().getPlugins().add(plugin);

        String throwableStack = BatteryCanaryUtil.getThrowableStack(new Throwable());
        Assert.assertFalse(TextUtils.isEmpty(throwableStack));
    }

    @Test
    public void testGetCpuFreq() throws InterruptedException {
        for (int i = 0; i < 5; i++) {
            int[] ints = BatteryCanaryUtil.getCpuCurrentFreq();
            Assert.assertNotNull(ints);
            Thread.sleep(100L);
        }
    }
}
