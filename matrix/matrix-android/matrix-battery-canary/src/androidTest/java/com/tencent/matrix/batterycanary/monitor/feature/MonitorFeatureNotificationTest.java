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
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCallback;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;

import androidx.test.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;


@RunWith(AndroidJUnit4.class)
public class MonitorFeatureNotificationTest {
    static final String TAG = "Matrix.test.MonitorFeatureNotificationTest";

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

    private BatteryMonitorCore mockMonitor() {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(WakeLockMonitorFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(false)
                .wakelockTimeout(1000)
                .greyJiffiesTime(100)
                .enableAmsHook(true)
                .foregroundLoopCheckTime(1000)
                .build();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(config);
        plugin.init(((Application)mContext.getApplicationContext()), null);
        Matrix.with().getPlugins().add(plugin);
        return new BatteryMonitorCore(config);
    }


    @Test
    public void testNotify() throws InterruptedException {
        final AtomicReference<String> textRef = new AtomicReference<>();
        final AtomicLong bgMillisRef = new AtomicLong();
        final CountDownLatch latch = new CountDownLatch(1);
        final CountDownLatch latch2 = new CountDownLatch(1);

        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(NotificationMonitorFeature.class)
                .setCallback(new BatteryMonitorCallback.BatteryPrinter() {
                    @Override
                    public void onNotify(NotificationMonitorFeature.BadNotification badNotification) {
                        textRef.set(badNotification.content);
                        bgMillisRef.set(badNotification.bgMillis);
                        if (badNotification.bgMillis > 0) {
                            latch2.countDown();
                        } else {
                            latch.countDown();
                        }
                    }
                }).build();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(config);
        plugin.init(((Application)mContext.getApplicationContext()), null);
        Matrix.with().getPlugins().add(plugin);
        BatteryMonitorCore core = plugin.core();
        core.start();
        NotificationMonitorFeature feature = core.getMonitorFeature(NotificationMonitorFeature.class);

        feature.onForeground(true);
        feature.checkNotifyContent(null, "foo");
        latch.await();

        Assert.assertEquals("foo", textRef.get());
        Assert.assertEquals(0, bgMillisRef.get());

        feature.onForeground(false);
        Thread.sleep(10L);
        feature.checkNotifyContent(null, "foo2");
        latch2.await();

        Assert.assertEquals("foo2", textRef.get());
        Assert.assertTrue(bgMillisRef.get() > 0);
    }
}
