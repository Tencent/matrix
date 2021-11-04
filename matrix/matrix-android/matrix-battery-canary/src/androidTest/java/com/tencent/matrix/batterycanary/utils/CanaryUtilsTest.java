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
import android.app.ActivityManager.RunningAppProcessInfo;
import android.app.Application;
import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;

import androidx.core.app.NotificationCompat;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import android.text.TextUtils;
import android.util.Log;
import android.widget.Toast;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.R;
import com.tencent.matrix.batterycanary.TestUtils;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.lang.reflect.InvocationTargetException;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

import static android.content.Context.ACTIVITY_SERVICE;


@RunWith(AndroidJUnit4.class)
public class CanaryUtilsTest {
    static final String TAG = "Matrix.test.CanaryUtilsTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
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
            int temperature = BatteryCanaryUtil.getBatteryTemperatureImmediately(mContext);
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

    @Test
    public void testLaunchForegroundServiceInBackground() {
        if (TestUtils.isAssembleTest()) return;

        Log.w(TAG, "#testLaunchForegroundServiceInBackground");
        mContext.startService(new Intent(mContext, SpyForeGroundService.class));

        ActivityManager am = (ActivityManager) mContext.getSystemService(ACTIVITY_SERVICE);
        for (ActivityManager.RunningAppProcessInfo item : am.getRunningAppProcesses()) {
            if (item.processName.startsWith(mContext.getPackageName())) {
                // foreground service
                Log.w(TAG, item.processName + ": " + item.importance);
                Assert.assertTrue(item.importance >= 125);
            }
        }

        try {
            Thread.sleep(1000000000L);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    @Test
    public void testHandlerPostDelay() throws InterruptedException {
        if (TestUtils.isAssembleTest()) return;
        final AtomicBoolean callback = new AtomicBoolean();
        new Handler(Looper.getMainLooper()).postDelayed(new Runnable() {
            @Override
            public void run() {
                callback.set(true);
            }
        }, Long.MAX_VALUE);
        Thread.sleep(100L);
        Assert.assertFalse("should never callback", callback.get());
    }

    @Test
    public void testProcessEnableConfigs() throws InterruptedException {
        if (TestUtils.isAssembleTest()) return;
        int flag = 0B01111111;
        Assert.assertEquals(127, flag);
        Assert.assertEquals(0B00000001, flag & 0B00000001);
        Assert.assertEquals(0B00000010, flag & 0B00000010);
        Assert.assertEquals(0B00000100, flag & 0B00000100);
        Assert.assertEquals(0B00001000, flag & 0B00001000);
        Assert.assertEquals(0B00000000, flag & 0B10000000);
        // Assert.fail(flag + " vs " + Integer.toBinaryString(flag) ); // 127
    }

    @Test
    public void testExpireRef() throws InterruptedException {
        for (int i = 0; i < 10; i++) {
            BatteryCanaryUtil.Proxy.ExpireRef ref = new BatteryCanaryUtil.Proxy.ExpireRef(22, 100);
            Assert.assertEquals(22, ref.value);
            Assert.assertFalse(ref.isExpired());
            Thread.sleep(100);
            Assert.assertTrue(ref.isExpired());
        }
    }

    private static boolean diceWithBase(int base) {
        double dice = Math.random();
        if (base >= 1 && dice < (1 / ((double) base))) {
            Log.i(TAG, "dice hit, go kv stat!");
            return true;
        }
        return false;
    }

    @Test
    public void testReadAppForegroundStat() throws NoSuchMethodException, InvocationTargetException, IllegalAccessException, NoSuchFieldException {
        ActivityManager am = (ActivityManager) mContext.getSystemService(Context.ACTIVITY_SERVICE);
        if (am == null) {
            return;
        }
        List<RunningAppProcessInfo> runningAppProcesses = am.getRunningAppProcesses();
        if (runningAppProcesses == null) {
            return;
        }
        List<RunningAppProcessInfo> myRunningApp = new LinkedList<>();
        for (RunningAppProcessInfo item : runningAppProcesses) {
            if (!TextUtils.isEmpty(item.processName) && item.processName.startsWith(mContext.getPackageName())) {
                myRunningApp.add(item);
            }
        }

        Assert.assertEquals(1, myRunningApp.size());
        Assert.assertTrue(myRunningApp.get(0).importance >= RunningAppProcessInfo.IMPORTANCE_FOREGROUND_SERVICE); // 125
        int procState = (int) myRunningApp.get(0).getClass().getDeclaredField("processState").get(myRunningApp.get(0));
        Assert.assertTrue(procState >= 4); // ActivityManager#PROCESS_STATE_BOUND_FOREGROUND_SERVICE, PROCESS_STATE_IMPORTANT_FOREGROUND
    }

    public static class SpyService extends Service {
        @Override
        public IBinder onBind(Intent intent) {
            return null;
        }
    }

    public static class SpyForeGroundService extends Service {

        @Override
        public void onCreate() {
            super.onCreate();
            ActivityManager am = (ActivityManager) this.getSystemService(ACTIVITY_SERVICE);
            for (ActivityManager.RunningAppProcessInfo item : am.getRunningAppProcesses()) {
                if (item.processName.startsWith(this.getPackageName())) {
                    Toast.makeText(this, item.processName + ": " + item.importance, Toast.LENGTH_SHORT).show();
                    Log.w(TAG, "" + item.processName + ": " + item.importance);
                }
            }

            Intent intent = new Intent(this, SpyService.class);
            PendingIntent pendingIntent = PendingIntent.getActivity(this, 0, intent, 0);

            Notification notification = new NotificationCompat.Builder(this)
                    .setSmallIcon(R.drawable.ic_launcher)
                 // .setContentTitle("My Awesome App")
                 // .setContentText("Doing some work...")
                    .setContentIntent(pendingIntent).build();

            startForeground(1337, notification);

            for (ActivityManager.RunningAppProcessInfo item : am.getRunningAppProcesses()) {
                if (item.processName.startsWith(this.getPackageName())) {
                    Log.w(TAG, item.processName + ": " + item.importance);
                    Toast.makeText(this, item.processName + ": " + item.importance, Toast.LENGTH_SHORT).show();
                }
            }
            final Handler handler = new Handler(Looper.getMainLooper());
            handler.post(new Runnable() {
                @Override
                public void run() {
                    handler.post(this);
                }
            });
        }

        @Override
        public IBinder onBind(Intent intent) {
            return null;
        }
    }
}
