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
import android.arch.core.util.Function;
import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;
import android.text.TextUtils;
import android.util.Log;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.AlarmMonitorFeature.AlarmRecord;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.BeanEntry;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.atomic.AtomicInteger;


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
    public void testPolishStack() {
        String stackTrace = Log.getStackTraceString(new Throwable());
        String stack = BatteryCanaryUtil.polishStack(stackTrace, "at android.app");
        Assert.assertFalse(TextUtils.isEmpty(stack));
    }

    @Test
    public void testSetAlarm() throws InterruptedException {
        AlarmMonitorFeature feature = new AlarmMonitorFeature();
        feature.configure(mockMonitor());

        AlarmMonitorFeature.AlarmSnapshot snapshot = feature.currentAlarms();
        Assert.assertEquals(0, (long) snapshot.totalCount.get());
        Assert.assertEquals(0, (long) snapshot.tracingCount.get());

        for (int i = 0; i < 50; i++) {
            Intent intent = new Intent();
            intent.setAction("ALARM_ACTION(" + 10000 + ")");
            intent.putExtra("extra_pid", 2233);
            PendingIntent pendingIntent = PendingIntent.getBroadcast(mContext, i, intent, 33); // differ pending intent
            feature.onAlarmSet(AlarmManager.RTC, 0 , 0 , 0, 0, pendingIntent, null);
            snapshot = feature.currentAlarms();
            Assert.assertEquals(i + 1, (long) snapshot.totalCount.get());
            Assert.assertEquals(i + 1, (long) snapshot.tracingCount.get());
            Assert.assertEquals(0, (long) snapshot.duplicatedGroup.get());
            Assert.assertEquals(0, (long) snapshot.duplicatedCount.get());
        }

        feature.onTurnOff();
        snapshot = feature.currentAlarms();
        Assert.assertEquals(0,(long) snapshot.totalCount.get());
        Assert.assertEquals(0,(long) snapshot.tracingCount.get());
    }

    @Test
    public void testSetAlarmDuplicated() throws InterruptedException {
        AlarmMonitorFeature feature = new AlarmMonitorFeature();
        feature.configure(mockMonitor());

        AlarmMonitorFeature.AlarmSnapshot snapshot = feature.currentAlarms();
        Assert.assertEquals(0, (long) snapshot.totalCount.get());
        Assert.assertEquals(0, (long) snapshot.tracingCount.get());

        for (int i = 0; i < 50; i++) {
            Intent intent = new Intent();
            intent.setAction("ALARM_ACTION(" + 10000 + ")");
            intent.putExtra("extra_pid", 2233);
            PendingIntent pendingIntent = PendingIntent.getBroadcast(mContext, 22, intent, 33); // same pending intent bcs input args consist
            feature.onAlarmSet(AlarmManager.RTC, 0 , 0 , 0, 0, pendingIntent, null);
            snapshot = feature.currentAlarms();
            Assert.assertEquals(i + 1 , (long) snapshot.totalCount.get());
            Assert.assertEquals(i + 1, (long) snapshot.tracingCount.get());
            Assert.assertEquals( 0, (long) snapshot.duplicatedGroup.get());
            Assert.assertEquals( 0, (long) snapshot.duplicatedCount.get());
        }

        feature.onTurnOff();
        snapshot = feature.currentAlarms();
        Assert.assertEquals(0, (long) snapshot.totalCount.get());
        Assert.assertEquals(0, (long) snapshot.tracingCount.get());
    }

    @Test
    public void setAlarmRecordAggregate() {
        final AtomicInteger inc = new AtomicInteger(0);
        Function<String, AlarmRecord> supplier = new Function<String, AlarmRecord>() {
            @Override
            public AlarmRecord apply(String input) {
                return new AlarmRecord(0, inc.incrementAndGet(), 0, 0, 0, input);
            }
        };

        AlarmMonitorFeature feature = new AlarmMonitorFeature();
        feature.configure(mockMonitor());
        AlarmMonitorFeature.AlarmSnapshot bgn = feature.currentAlarms();
        AlarmMonitorFeature.AlarmSnapshot end = feature.currentAlarms();
        Assert.assertTrue(bgn.records.getList().isEmpty());
        Assert.assertTrue(end.records.getList().isEmpty());

        AlarmRecord mock = supplier.apply("mock_1");
        end.records.getList().add(BeanEntry.of(mock));
        Assert.assertEquals(1, end.records.getList().size());
        Assert.assertEquals(1, end.diff(bgn).dlt.records.getList().size());
        Assert.assertEquals("mock_1", end.diff(bgn).dlt.records.getList().get(0).value.stack);

        bgn.records.getList().add(BeanEntry.of(mock));
        Assert.assertEquals(0, end.diff(bgn).dlt.records.getList().size());

        for (int i = 0; i < 100; i++) {
            AlarmRecord mock2 = supplier.apply("mock_2");
            bgn.records.getList().add(BeanEntry.of(mock2));
            end.records.getList().add(BeanEntry.of(mock2));
        }
        Assert.assertEquals(0, end.diff(bgn).dlt.records.getList().size());

        for (int i = 0; i < 100; i++) {
            AlarmRecord mock3 = supplier.apply("mock_3");
            end.records.getList().add(BeanEntry.of(mock3));
        }
        Assert.assertEquals(100, end.diff(bgn).dlt.records.getList().size());
        for (BeanEntry<AlarmRecord> item : end.diff(bgn).dlt.records.getList()) {
            Assert.assertEquals("mock_3", item.value.stack);
        }

        for (int i = 0; i < 100; i++) {
            bgn.records.getList().add(BeanEntry.of(supplier.apply("mock_3")));
            end.records.getList().add(BeanEntry.of(supplier.apply("mock_3")));
        }
        Assert.assertEquals(200, end.diff(bgn).dlt.records.getList().size());
        for (BeanEntry<AlarmRecord> item : end.diff(bgn).dlt.records.getList()) {
            Assert.assertEquals("mock_3", item.value.stack);
        }
    }
}
