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

import android.app.ActivityManager;
import android.app.Application;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;
import android.text.TextUtils;
import android.util.Log;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.TestUtils;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;

import static android.content.Context.ACTIVITY_SERVICE;


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
        if (TestUtils.isAssembleTest()) return;

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
        plugin.init(((Application) mContext.getApplicationContext()), null);
        Matrix.with().getPlugins().add(plugin);

        String processName = BatteryCanaryUtil.getProcessName();
        Assert.assertFalse(TextUtils.isEmpty(processName));
        Assert.assertEquals(mContext.getPackageName(), processName);
    }

    @Test
    public void testGetPkgName() {
        if (TestUtils.isAssembleTest()) return;

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
        plugin.init(((Application) mContext.getApplicationContext()), null);
        Matrix.with().getPlugins().add(plugin);

        String pkg = BatteryCanaryUtil.getPackageName();
        Assert.assertFalse(TextUtils.isEmpty(pkg));
        Assert.assertEquals(mContext.getPackageName(), pkg);
    }

    @Test
    public void testGetThrowableStack() {
        if (TestUtils.isAssembleTest()) return;

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
        plugin.init(((Application) mContext.getApplicationContext()), null);
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

    @Test
    public void testGetBatteryTemps() throws InterruptedException {
        for (int i = 0; i < 5; i++) {
            int temperature = BatteryCanaryUtil.getBatteryTemperature(mContext);
            Assert.assertTrue(temperature > 0);
            Thread.sleep(100L);
        }
    }

    @Test
    public void testCheckIfDeviceCharging() {
        Assert.assertTrue(BatteryCanaryUtil.isDeviceCharging(mContext));
    }

    @Test
    public void testDicing() {
        if (TestUtils.isAssembleTest()) return;

        long loopCount = 1000L;
        int totalRollCount = 0;
        for (int i = 0; i < loopCount; i++) {
            int base = 10000;
            do {
                totalRollCount++;
            } while (!diceWithBase(base));
        }

        Assert.fail("AVG ROLL COUNT: " + totalRollCount / loopCount);
    }

    @Test
    public void testCheckDeviceScreenOn() {
        boolean screenOn = BatteryCanaryUtil.isDeviceScreenOn(mContext);
        Assert.assertTrue(screenOn);
    }

    @Test
    public void testCheckDeviceOnPowerSaveMode() {
        boolean result  = BatteryCanaryUtil.isDeviceOnPowerSave(mContext);
        Assert.assertFalse(result);
    }

    @Test
    public void testCheckAppForeGroundService() {
        boolean hasRunningService = false;
        ActivityManager am = (ActivityManager) mContext.getSystemService(ACTIVITY_SERVICE);
        List<ActivityManager.RunningServiceInfo> runningServices = am.getRunningServices(Integer.MAX_VALUE);
        for (ActivityManager.RunningServiceInfo runningServiceInfo : runningServices) {
            if (runningServiceInfo.process.startsWith(mContext.getPackageName())) {
                hasRunningService = true;
            }
        }
        Assert.assertFalse(hasRunningService);

        mContext.startService(new Intent(mContext, SpyService.class));
        runningServices = am.getRunningServices(Integer.MAX_VALUE);
        for (ActivityManager.RunningServiceInfo runningServiceInfo : runningServices) {
            if (runningServiceInfo.process.startsWith(mContext.getPackageName())) {
                hasRunningService = true;
                Assert.assertTrue(runningServiceInfo.started);
                Assert.assertFalse(runningServiceInfo.foreground);
            }
        }
        Assert.assertTrue(hasRunningService);

        Assert.assertFalse(BatteryCanaryUtil.hasForegroundService(mContext));
        Assert.assertTrue(BatteryCanaryUtil.listForegroundServices(mContext).isEmpty());
    }

    private static boolean diceWithBase(int base) {
        double dice = Math.random();
        if (base >= 1 && dice < (1 / ((double) base))) {
            Log.i(TAG, "dice hit, go kv stat!");
            return true;
        }
        return false;
    }

    public static class SpyService extends Service {
        @Override
        public IBinder onBind(Intent intent) {
            return null;
        }
    }
}
