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

import android.app.Application;
import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryCanary;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.feature.AlarmMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.LooperTaskMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.atomic.AtomicReference;


@RunWith(AndroidJUnit4.class)
public class BatteryCanaryApiTest {
    static final String TAG = "Matrix.test.BatteryCanaryApiTest";

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
    public void testGetCurrentJiffies() {
        final AtomicReference<JiffiesMonitorFeature.JiffiesSnapshot> ref = new AtomicReference<>(null);

        BatteryCanary.currentJiffies(new BatteryMonitorCore.Callback<JiffiesMonitorFeature.JiffiesSnapshot>() {
            @Override
            public void onGetJiffies(JiffiesMonitorFeature.JiffiesSnapshot snapshot) {
                ref.set(snapshot);
                synchronized (ref) {
                    ref.notifyAll();
                }
            }
        });

        synchronized (ref) {
            try {
                ref.wait(100L);
            } catch (InterruptedException ignored) {
            }
        }

        Assert.assertNull(ref.get());
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder().enable(JiffiesMonitorFeature.class).build();
        BatteryMonitorCore core = new BatteryMonitorCore(config);
        core.start();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(config);
        Matrix.with().getPlugins().add(plugin);

        BatteryCanary.currentJiffies(new BatteryMonitorCore.Callback<JiffiesMonitorFeature.JiffiesSnapshot>() {
            @Override
            public void onGetJiffies(JiffiesMonitorFeature.JiffiesSnapshot snapshot) {
                ref.set(snapshot);
                synchronized (ref) {
                    ref.notifyAll();
                }
            }
        });

        synchronized (ref) {
            try {
                ref.wait();
            } catch (InterruptedException ignored) {
            }
        }
        Assert.assertNotNull(ref.get());
    }
}
