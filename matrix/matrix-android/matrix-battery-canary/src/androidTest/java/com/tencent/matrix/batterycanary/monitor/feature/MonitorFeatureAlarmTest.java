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

import android.app.AlarmManager;
import android.app.Application;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;


@RunWith(AndroidJUnit4.class)
public class MonitorFeatureAlarmTest {
    static final String TAG = "Matrix.test.MonitorFeatureAlarmTest";

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
    public void testSetAlarm() throws InterruptedException {
        AlarmMonitorFeature feature = new AlarmMonitorFeature();
        feature.configure(mockMonitor());

        AlarmMonitorFeature.AlarmSnapshot snapshot = feature.currentAlarms();
        Assert.assertEquals(0, snapshot.totalCount);
        Assert.assertEquals(0, snapshot.tracingCount);

        for (int i = 0; i < 50; i++) {
            Intent intent = new Intent();
            intent.setAction("ALARM_ACTION(" + 10000 + ")");
            intent.putExtra("extra_pid", 2233);
            PendingIntent pendingIntent = PendingIntent.getBroadcast(mContext, i, intent, 33); // differ pending intent
            feature.onAlarmSet(AlarmManager.RTC, 0 , 0 , 0, 0, pendingIntent, null);
            snapshot = feature.currentAlarms();
            Assert.assertEquals(i + 1, snapshot.totalCount);
            Assert.assertEquals(i + 1, snapshot.tracingCount);
            Assert.assertEquals(0, snapshot.duplicatedGroup);
            Assert.assertEquals(0, snapshot.duplicatedCount);
        }

        feature.onTurnOff();
        snapshot = feature.currentAlarms();
        Assert.assertEquals(0, snapshot.totalCount);
        Assert.assertEquals(0, snapshot.tracingCount);
    }

    @Test
    public void testSetAlarmDuplicated() throws InterruptedException {
        AlarmMonitorFeature feature = new AlarmMonitorFeature();
        feature.configure(mockMonitor());

        AlarmMonitorFeature.AlarmSnapshot snapshot = feature.currentAlarms();
        Assert.assertEquals(0, snapshot.totalCount);
        Assert.assertEquals(0, snapshot.tracingCount);

        for (int i = 0; i < 50; i++) {
            Intent intent = new Intent();
            intent.setAction("ALARM_ACTION(" + 10000 + ")");
            intent.putExtra("extra_pid", 2233);
            PendingIntent pendingIntent = PendingIntent.getBroadcast(mContext, 22, intent, 33); // same pending intent bcs input args consist
            feature.onAlarmSet(AlarmManager.RTC, 0 , 0 , 0, 0, pendingIntent, null);
            snapshot = feature.currentAlarms();
            Assert.assertEquals(i + 1 , snapshot.totalCount);
            Assert.assertEquals(1, snapshot.tracingCount);
            Assert.assertEquals(i >= 1 ? 1 : 0, snapshot.duplicatedGroup);
            Assert.assertEquals(i >= 1 ? i + 1 : 0, snapshot.duplicatedCount);
        }

        feature.onTurnOff();
        snapshot = feature.currentAlarms();
        Assert.assertEquals(0, snapshot.totalCount);
        Assert.assertEquals(0, snapshot.tracingCount);
    }
}
