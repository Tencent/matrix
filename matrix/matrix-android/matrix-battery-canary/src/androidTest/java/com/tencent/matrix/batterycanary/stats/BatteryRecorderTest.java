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

package com.tencent.matrix.batterycanary.stats;

import android.app.Application;
import android.content.Context;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.mmkv.MMKV;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

/**
 * Instrumented test, which will execute on an Android device.
 *
 * @see <a href="http://d.android.com/tools/testing">Testing documentation</a>
 */
@RunWith(AndroidJUnit4.class)
public class BatteryRecorderTest {
    static final String TAG = "Matrix.test.BatteryRecorderTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        if (!Matrix.isInstalled()) {
            Matrix.init(new Matrix.Builder(((Application) mContext.getApplicationContext())).build());
        }
        String rootDir = MMKV.initialize(mContext);
        System.out.println("mmkv root: " + rootDir);
    }

    @After
    public void shutDown() {
    }

    @Test
    public void testRecorderWithEventRecord() {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder().enable(JiffiesMonitorFeature.class).build();
        BatteryMonitorCore core = new BatteryMonitorCore(config);
        core.start();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(config);
        Matrix.with().getPlugins().add(plugin);

        BatteryRecord.EventStatRecord record = new BatteryRecord.EventStatRecord();
        record.id = 22;
        record.event = "EVENT";

        String date = BatteryStatsFeature.getDateString(0);
        MMKV mmkv = MMKV.defaultMMKV();
        mmkv.clearAll();
        BatteryRecorder.MMKVRecorder recorder = new BatteryRecorder.MMKVRecorder(mmkv);
        recorder.clean(date, "main");
        Assert.assertTrue(recorder.read(date, "main").isEmpty());

        recorder.write(date, record);
        Assert.assertTrue(recorder.read(BatteryStatsFeature.getDateString(-1), "main").isEmpty());

        List<BatteryRecord> records = recorder.read(date, "main");
        Assert.assertEquals(1, records.size());
        Assert.assertTrue(records.get(0) instanceof BatteryRecord.EventStatRecord);
        Assert.assertEquals(record.id, ((BatteryRecord.EventStatRecord) records.get(0)).id);
        Assert.assertEquals(record.event, ((BatteryRecord.EventStatRecord) records.get(0)).event);

        recorder.clean(BatteryStatsFeature.getDateString(-1), "main");
        Assert.assertFalse(recorder.read(date, "main").isEmpty());
        recorder.clean(date, "main");
        Assert.assertTrue(recorder.read(date, "main").isEmpty());
    }

    @Test
    public void testRecorderWithEventRecordWithMultiProc() {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder().enable(JiffiesMonitorFeature.class).build();
        BatteryMonitorCore core = new BatteryMonitorCore(config);
        core.start();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(config);
        Matrix.with().getPlugins().add(plugin);



        String date = BatteryStatsFeature.getDateString(0);
        MMKV mmkv = MMKV.defaultMMKV();
        mmkv.clearAll();

        BatteryRecorder.MMKVRecorder recorder = new BatteryRecorder.MMKVRecorder(mmkv);
        Assert.assertTrue(recorder.read(date, "main").isEmpty());

        BatteryRecord.EventStatRecord record = new BatteryRecord.EventStatRecord();
        record.id = 22;
        record.event = "EVENT";

        recorder.write(date, record);
        Assert.assertTrue(recorder.read(BatteryStatsFeature.getDateString(-1), "main").isEmpty());

        List<BatteryRecord> records = recorder.read(date, "main");
        Assert.assertEquals(1, records.size());
        Assert.assertTrue(records.get(0) instanceof BatteryRecord.EventStatRecord);
        Assert.assertEquals(record.id, ((BatteryRecord.EventStatRecord) records.get(0)).id);
        Assert.assertEquals(record.event, ((BatteryRecord.EventStatRecord) records.get(0)).event);

        Assert.assertTrue(recorder.read(BatteryStatsFeature.getDateString(0), "sub1").isEmpty());
        Assert.assertTrue(recorder.read(BatteryStatsFeature.getDateString(0), "sub2").isEmpty());

        recorder.clean(BatteryStatsFeature.getDateString(-1), "main");
        Assert.assertFalse(recorder.read(date, "main").isEmpty());

        recorder.clean(date, "sub1");
        recorder.clean(date, "sub2");
        Assert.assertFalse(recorder.read(date, "main").isEmpty());
        records = recorder.read(date, "main");
        Assert.assertEquals(1, records.size());
        Assert.assertTrue(records.get(0) instanceof BatteryRecord.EventStatRecord);
        Assert.assertEquals(record.id, ((BatteryRecord.EventStatRecord) records.get(0)).id);
        Assert.assertEquals(record.event, ((BatteryRecord.EventStatRecord) records.get(0)).event);

        recorder.clean(date, "main");
        Assert.assertTrue(recorder.read(date, "main").isEmpty());

        mmkv.clearAll();
    }
}
