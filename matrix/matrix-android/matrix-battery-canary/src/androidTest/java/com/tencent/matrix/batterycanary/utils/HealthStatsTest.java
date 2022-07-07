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
import android.os.health.HealthStats;
import android.os.health.PidHealthStats;
import android.os.health.SystemHealthManager;
import android.os.health.TimerStat;
import android.os.health.UidHealthStats;

import com.tencent.matrix.Matrix;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;


@RunWith(AndroidJUnit4.class)
public class HealthStatsTest {
    static final String TAG = "Matrix.test.HealthStatsTest";

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
    public void testLiterateStats() {
        SystemHealthManager manager = (SystemHealthManager) mContext.getSystemService(Context.SYSTEM_HEALTH_SERVICE);
        HealthStats healthStats = manager.takeMyUidSnapshot();
        Assert.assertNotNull(healthStats);
        Assert.assertEquals("UidHealthStats", healthStats.getDataType());

        List<HealthStats> statsList = new ArrayList<>();
        statsList.add(healthStats);

        int statsKeyCount = healthStats.getStatsKeyCount();
        Assert.assertTrue(statsKeyCount > 0);

        for (int i = 0; i < statsKeyCount; i++) {
            int key = healthStats.getStatsKeyAt(i);
            if (healthStats.hasStats(key)) {
                Map<String, HealthStats> stats = healthStats.getStats(key);
                for (HealthStats item : stats.values()) {
                    Assert.assertTrue(Arrays.asList(
                            "PidHealthStats",
                            "ProcessHealthStats",
                            "PackageHealthStats",
                            "ServiceHealthStats"
                    ).contains(item.getDataType()));

                    statsList.add(item);

                    int subKeyCount = item.getStatsKeyCount();
                    if (subKeyCount > 0) {
                        for (int j = 0; j < subKeyCount; j++) {
                            int subKey = item.getStatsKeyAt(j);
                            if (item.hasStats(subKey)) {
                                statsList.addAll(item.getStats(subKey).values());
                            }
                        }
                    }
                }
            }
        }

        for (HealthStats item : statsList) {
            int count = item.getMeasurementKeyCount();
            for (int i = 0; i < count; i++) {
                int key = item.getMeasurementKeyAt(i);
                long value = item.getMeasurement(key);
                Assert.assertTrue(item.getDataType() + ": " + key + "=" + value, value >= 0);
            }
        }
        for (HealthStats item : statsList) {
            int count = item.getMeasurementsKeyCount();
            for (int i = 0; i < count; i++) {
                int key = item.getMeasurementsKeyAt(i);
                Map<String, Long> values = item.getMeasurements(key);
                for (Map.Entry<String, Long> entry : values.entrySet()) {
                    Assert.assertTrue(item.getDataType() + ": " + entry.getKey() + "=" + entry.getValue(), entry.getValue() >= 0);
                }
            }
        }

        for (HealthStats item : statsList) {
            int count = item.getTimerKeyCount();
            for (int i = 0; i < count; i++) {
                int key = item.getTimerKeyAt(i);
                TimerStat timerStat = item.getTimer(key);
                Assert.assertTrue(item.getDataType() + ": " + key + "=" + timerStat.getCount() + "," + timerStat.getTime(), timerStat.getCount() >= 0);
                Assert.assertTrue(item.getDataType() + ": " + key + "=" + timerStat.getCount() + "," + timerStat.getTime(), timerStat.getTime() >= 0);
            }
        }
        for (HealthStats item : statsList) {
            int count = item.getTimersKeyCount();
            for (int i = 0; i < count; i++) {
                int key = item.getTimersKeyAt(i);
                Map<String, TimerStat> timerStatMap = item.getTimers(key);
                for (Map.Entry<String, TimerStat> entry : timerStatMap.entrySet()) {
                    Assert.assertTrue(item.getDataType() + ": " + entry.getKey() + "=" + entry.getValue().getCount() + "," + entry.getValue().getTime(), entry.getValue().getCount() >= 0);
                    Assert.assertTrue(item.getDataType() + ": " + entry.getKey() + "=" + entry.getValue().getCount() + "," + entry.getValue().getTime(), entry.getValue().getTime() >= 0);
                }
            }
        }
    }
}