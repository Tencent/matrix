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
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCallback;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.LooperTaskMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature;
import com.tencent.matrix.plugin.Plugin;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;

/**
 * Instrumented test, which will execute on an Android device.
 *
 * @see <a href="http://d.android.com/tools/testing">Testing documentation</a>
 */
@RunWith(AndroidJUnit4.class)
public class BatteryEventDelegateTest {
    static final String TAG = "Matrix.test.BatteryEventDelegateTest";

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
        BatteryEventDelegate.release();
    }

    private BatteryMonitorCore mockMonitor() {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(JiffiesMonitorFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(false)
                .wakelockTimeout(1000)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(1000)
                .build();
        return new BatteryMonitorCore(config);
    }

    @Test
    public void testInit() throws Exception {
        try {
            BatteryEventDelegate.getInstance();
            fail();
        } catch (Exception e) {
            e.printStackTrace();
        }

        BatteryEventDelegate.init((Application) mContext.getApplicationContext());
        BatteryEventDelegate.getInstance();
    }

    @Test
    public void testQueryBatteryState() throws Exception {
        BatteryEventDelegate.init((Application) mContext.getApplicationContext());
        BatteryMonitorCore monitor = mockMonitor();
        BatteryEventDelegate.getInstance().attach(monitor);

        BatteryEventDelegate.BatteryState batteryState = BatteryEventDelegate.getInstance().currentState();
        Assert.assertNotNull(batteryState);
        assertTrue(batteryState.isCharging());

        monitor.onForeground(true);
        assertEquals(0, batteryState.getBackgroundTimeMillis());

        monitor.onForeground(false);
        Thread.sleep(100L);
        assertTrue(batteryState.getBackgroundTimeMillis() >= 100L);

        monitor.onForeground(true);
        assertEquals(0, batteryState.getBackgroundTimeMillis());
    }

    @Test
    public void testBatteryStateChangedListener() throws Exception {
        final AtomicBoolean hasChanged = new AtomicBoolean();
        final AtomicLong millis = new AtomicLong();

        BatteryEventDelegate.init((Application) mContext.getApplicationContext());
        BatteryEventDelegate.getInstance().addListener(new BatteryEventDelegate.Listener() {
            @Override
            public boolean onStateChanged(String event) {
                assertTrue(BatteryEventDelegate.getInstance().currentState().isCharging());
                hasChanged.set(true);
                return false;
            }

            @Override
            public boolean onAppLowEnergy(BatteryEventDelegate.BatteryState batteryState, long backgroundMillis) {
                millis.set(backgroundMillis);
                return false;
            }
        });

        assertFalse(hasChanged.get());
        assertEquals(0L, millis.get());

        BatteryEventDelegate.getInstance().dispatchSateChangedEvent(spy(Intent.class));
        assertTrue(hasChanged.get());
        assertEquals(0L, millis.get());

        BatteryEventDelegate.getInstance().dispatchAppLowEnergyEvent(10086L);
        assertTrue(hasChanged.get());
        assertEquals(10086L, millis.get());
    }
}
