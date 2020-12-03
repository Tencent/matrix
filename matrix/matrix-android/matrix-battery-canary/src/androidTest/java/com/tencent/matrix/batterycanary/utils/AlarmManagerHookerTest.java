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

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;


@RunWith(AndroidJUnit4.class)
public class AlarmManagerHookerTest {
    static final String TAG = "Matrix.test.MonitorFeatureWakeLockTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
    }

    @After
    public void shutDown() {
        AlarmManagerServiceHooker.release();
    }

    @Test
    public void testSetAlarm() {
        final AtomicBoolean hasSet = new AtomicBoolean();
        final AtomicBoolean hasRemove = new AtomicBoolean();
        final AtomicReference<PendingIntent> operationRef = new AtomicReference<>();
        final long triggerTime = System.currentTimeMillis();
        AlarmManagerServiceHooker.addListener(new AlarmManagerServiceHooker.IListener() {
            @Override
            public void onAlarmSet(int type, long triggerAtMillis, long windowMillis, long intervalMillis, int flags, PendingIntent operation, AlarmManager.OnAlarmListener onAlarmListener) {
                hasSet.set(true);
                Assert.assertSame(operationRef.get(), operation);
                Assert.assertEquals(triggerTime, triggerAtMillis);
            }

            @Override
            public void onAlarmRemove(PendingIntent operation, AlarmManager.OnAlarmListener onAlarmListener) {
                hasRemove.set(true);
                Assert.assertSame(operationRef.get(), operation);
            }
        });

        final AlarmManager am = (AlarmManager) mContext.getSystemService(Context.ALARM_SERVICE);
        Intent intent = new Intent();
        intent.setAction("ALARM_ACTION(" + 10000 + ")");
        intent.putExtra("extra_pid", 2233);
        PendingIntent pendingIntent = PendingIntent.getBroadcast(mContext, 22, intent, 33);
        operationRef.set(pendingIntent);
        Assert.assertFalse(hasSet.get());
        Assert.assertFalse(hasRemove.get());

        am.set(AlarmManager.RTC, triggerTime, pendingIntent);
        Assert.assertTrue(hasSet.get());
        Assert.assertFalse(hasRemove.get());

        am.cancel(pendingIntent);
        Assert.assertTrue(hasSet.get());
        Assert.assertTrue(hasRemove.get());
    }

    @Test
    public void testSetAlarm2() {
        final AtomicBoolean hasSet = new AtomicBoolean();
        final AtomicBoolean hasRemove = new AtomicBoolean();
        final AtomicReference<PendingIntent> operationRef = new AtomicReference<>();
        final long windowsStart = System.currentTimeMillis();
        final long windowsLength = 1000L;
        AlarmManagerServiceHooker.addListener(new AlarmManagerServiceHooker.IListener() {
            @Override
            public void onAlarmSet(int type, long triggerAtMillis, long windowMillis, long intervalMillis, int flags, PendingIntent operation, AlarmManager.OnAlarmListener onAlarmListener) {
                hasSet.set(true);
                Assert.assertSame(operationRef.get(), operation);
                Assert.assertEquals(windowsStart, triggerAtMillis);
                Assert.assertEquals(windowsLength, windowMillis);
            }

            @Override
            public void onAlarmRemove(PendingIntent operation, AlarmManager.OnAlarmListener onAlarmListener) {
                hasRemove.set(true);
                Assert.assertSame(operationRef.get(), operation);
            }
        });

        final AlarmManager am = (AlarmManager) mContext.getSystemService(Context.ALARM_SERVICE);
        Intent intent = new Intent();
        intent.setAction("ALARM_ACTION(" + 10000 + ")");
        intent.putExtra("extra_pid", 2233);
        PendingIntent pendingIntent = PendingIntent.getBroadcast(mContext, 22, intent, 33);
        operationRef.set(pendingIntent);
        Assert.assertFalse(hasSet.get());
        Assert.assertFalse(hasRemove.get());

        am.setWindow(AlarmManager.RTC, windowsStart, windowsLength, pendingIntent);
        Assert.assertTrue(hasSet.get());
        Assert.assertFalse(hasRemove.get());

        am.cancel(pendingIntent);
        Assert.assertTrue(hasSet.get());
        Assert.assertTrue(hasRemove.get());
    }

    @Test
    public void testSetAlarm3() {
        final AtomicBoolean hasSet = new AtomicBoolean();
        final AtomicBoolean hasRemove = new AtomicBoolean();
        final AtomicReference<PendingIntent> operationRef = new AtomicReference<>();
        final long triggerTime = System.currentTimeMillis();
        final long interval = 1000L;
        AlarmManagerServiceHooker.addListener(new AlarmManagerServiceHooker.IListener() {
            @Override
            public void onAlarmSet(int type, long triggerAtMillis, long windowMillis, long intervalMillis, int flags, PendingIntent operation, AlarmManager.OnAlarmListener onAlarmListener) {
                hasSet.set(true);
                Assert.assertSame(operationRef.get(), operation);
                Assert.assertEquals(triggerTime, triggerAtMillis);
                Assert.assertEquals(interval, intervalMillis);
            }

            @Override
            public void onAlarmRemove(PendingIntent operation, AlarmManager.OnAlarmListener onAlarmListener) {
                hasRemove.set(true);
                Assert.assertSame(operationRef.get(), operation);
            }
        });

        final AlarmManager am = (AlarmManager) mContext.getSystemService(Context.ALARM_SERVICE);
        Intent intent = new Intent();
        intent.setAction("ALARM_ACTION(" + 10000 + ")");
        intent.putExtra("extra_pid", 2233);
        PendingIntent pendingIntent = PendingIntent.getBroadcast(mContext, 22, intent, 33);
        operationRef.set(pendingIntent);
        Assert.assertFalse(hasSet.get());
        Assert.assertFalse(hasRemove.get());

        am.setRepeating(AlarmManager.RTC, triggerTime, interval, pendingIntent);
        Assert.assertTrue(hasSet.get());
        Assert.assertFalse(hasRemove.get());

        am.cancel(pendingIntent);
        Assert.assertTrue(hasSet.get());
        Assert.assertTrue(hasRemove.get());
    }
}
