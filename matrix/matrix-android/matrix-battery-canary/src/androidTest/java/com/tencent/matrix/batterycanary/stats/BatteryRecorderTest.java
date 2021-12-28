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
import android.os.Process;
import android.os.SystemClock;
import android.util.ArrayMap;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.mmkv.MMKV;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Arrays;
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
    @SuppressWarnings("ArraysAsListWithZeroOrOneArgument")
    public void testRecorderWithProcSet() {
        MMKV mmkv = MMKV.defaultMMKV();
        mmkv.clearAll();
        BatteryRecorder.MMKVRecorder recorder = new BatteryRecorder.MMKVRecorder(mmkv);
        Assert.assertTrue(recorder.getProcSet().isEmpty());

        recorder.updateProc("main");
        Assert.assertArrayEquals(Arrays.asList("main").toArray(), recorder.getProcSet().toArray());

        recorder.updateProc("sub1");
        recorder.updateProc("sub2");
        Assert.assertEquals(3, recorder.getProcSet().size());
        Assert.assertTrue(recorder.getProcSet().containsAll(Arrays.asList("main", "sub1", "sub2")));
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

    @Test
    public void testCleanByDayLimit() {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder().enable(JiffiesMonitorFeature.class).build();
        BatteryMonitorCore core = new BatteryMonitorCore(config);
        core.start();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(config);
        Matrix.with().getPlugins().add(plugin);

        MMKV mmkv = MMKV.defaultMMKV();
        mmkv.clearAll();
        BatteryRecorder.MMKVRecorder recorder = new BatteryRecorder.MMKVRecorder(mmkv);
        Assert.assertTrue(recorder.getProcSet().isEmpty());

        int dayLimit = 7;
        BatteryRecord.EventStatRecord record = new BatteryRecord.EventStatRecord();
        record.id = 22;
        record.event = "KEEP";
        recorder.write(BatteryRecorder.MMKVRecorder.getDateString(0), record);
        recorder.write(BatteryRecorder.MMKVRecorder.getDateString(-dayLimit + 1), record);
        record.event = "EXPIRED";
        recorder.write(BatteryRecorder.MMKVRecorder.getDateString(-dayLimit), record);
        recorder.flush();

        List<BatteryRecord> records = recorder.read(BatteryRecorder.MMKVRecorder.getDateString(0), "main");
        Assert.assertEquals(1, records.size());
        Assert.assertTrue(records.get(0) instanceof BatteryRecord.EventStatRecord);
        Assert.assertEquals(22, ((BatteryRecord.EventStatRecord) records.get(0)).id);
        Assert.assertEquals("KEEP", ((BatteryRecord.EventStatRecord) records.get(0)).event);

        records = recorder.read(BatteryRecorder.MMKVRecorder.getDateString(-dayLimit + 1), "main");
        Assert.assertEquals(1, records.size());
        Assert.assertTrue(records.get(0) instanceof BatteryRecord.EventStatRecord);
        Assert.assertEquals(22, ((BatteryRecord.EventStatRecord) records.get(0)).id);
        Assert.assertEquals("KEEP", ((BatteryRecord.EventStatRecord) records.get(0)).event);

        records = recorder.read(BatteryRecorder.MMKVRecorder.getDateString(-dayLimit), "main");
        Assert.assertEquals(1, records.size());
        Assert.assertTrue(records.get(0) instanceof BatteryRecord.EventStatRecord);
        Assert.assertEquals(22, ((BatteryRecord.EventStatRecord) records.get(0)).id);
        Assert.assertEquals("EXPIRED", ((BatteryRecord.EventStatRecord) records.get(0)).event);

        recorder.clean(dayLimit + 1);

        records = recorder.read(BatteryRecorder.MMKVRecorder.getDateString(0), "main");
        Assert.assertEquals(1, records.size());
        Assert.assertTrue(records.get(0) instanceof BatteryRecord.EventStatRecord);
        Assert.assertEquals(22, ((BatteryRecord.EventStatRecord) records.get(0)).id);
        Assert.assertEquals("KEEP", ((BatteryRecord.EventStatRecord) records.get(0)).event);

        records = recorder.read(BatteryRecorder.MMKVRecorder.getDateString(-dayLimit + 1), "main");
        Assert.assertEquals(1, records.size());
        Assert.assertTrue(records.get(0) instanceof BatteryRecord.EventStatRecord);
        Assert.assertEquals(22, ((BatteryRecord.EventStatRecord) records.get(0)).id);
        Assert.assertEquals("KEEP", ((BatteryRecord.EventStatRecord) records.get(0)).event);

        records = recorder.read(BatteryRecorder.MMKVRecorder.getDateString(-dayLimit), "main");
        Assert.assertEquals(1, records.size());
        Assert.assertTrue(records.get(0) instanceof BatteryRecord.EventStatRecord);
        Assert.assertEquals(22, ((BatteryRecord.EventStatRecord) records.get(0)).id);

        recorder.clean(dayLimit);

        records = recorder.read(BatteryRecorder.MMKVRecorder.getDateString(0), "main");
        Assert.assertEquals(1, records.size());
        Assert.assertTrue(records.get(0) instanceof BatteryRecord.EventStatRecord);
        Assert.assertEquals(22, ((BatteryRecord.EventStatRecord) records.get(0)).id);
        Assert.assertEquals("KEEP", ((BatteryRecord.EventStatRecord) records.get(0)).event);

        records = recorder.read(BatteryRecorder.MMKVRecorder.getDateString(-dayLimit + 1), "main");
        Assert.assertEquals(1, records.size());
        Assert.assertTrue(records.get(0) instanceof BatteryRecord.EventStatRecord);
        Assert.assertEquals(22, ((BatteryRecord.EventStatRecord) records.get(0)).id);
        Assert.assertEquals("KEEP", ((BatteryRecord.EventStatRecord) records.get(0)).event);

        records = recorder.read(BatteryRecorder.MMKVRecorder.getDateString(-dayLimit), "main");
        Assert.assertEquals(0, records.size());

        recorder.clean(1);

        records = recorder.read(BatteryRecorder.MMKVRecorder.getDateString(0), "main");
        Assert.assertEquals(1, records.size());
        Assert.assertTrue(records.get(0) instanceof BatteryRecord.EventStatRecord);
        Assert.assertEquals(22, ((BatteryRecord.EventStatRecord) records.get(0)).id);
        Assert.assertEquals("KEEP", ((BatteryRecord.EventStatRecord) records.get(0)).event);

        records = recorder.read(BatteryRecorder.MMKVRecorder.getDateString(-dayLimit + 1), "main");
        Assert.assertEquals(0, records.size());

        records = recorder.read(BatteryRecorder.MMKVRecorder.getDateString(-dayLimit), "main");
        Assert.assertEquals(0, records.size());
    }

    @Test
    public void testUniversalRecords() throws InterruptedException {
        MMKV mmkv = MMKV.defaultMMKV();
        mmkv.clearAll();

        String date = BatteryStatsFeature.getDateString(0);
        String proc = "main";
        BatteryRecorder.MMKVRecorder recorder = new BatteryRecorder.MMKVRecorder(mmkv);
        List<BatteryRecord> reads = recorder.read(date, proc);
        Assert.assertTrue(reads.isEmpty());
        int count = 0;

        BatteryRecord.ProcStatRecord procStatRecord = new BatteryRecord.ProcStatRecord();
        procStatRecord.pid = Process.myPid();
        procStatRecord.procStat = 22;
        recorder.write(date, procStatRecord, proc);
        Thread.sleep(10L);
        reads = recorder.read(date, proc);
        Assert.assertFalse(reads.isEmpty());
        Assert.assertTrue(reads.get(reads.size() - 1) instanceof BatteryRecord.ProcStatRecord);
        Assert.assertEquals(procStatRecord.pid, ((BatteryRecord.ProcStatRecord) reads.get(reads.size() - 1)).pid);
        Assert.assertEquals(procStatRecord.procStat, ((BatteryRecord.ProcStatRecord) reads.get(reads.size() - 1)).procStat);

        BatteryRecord.AppStatRecord appStat = new BatteryRecord.AppStatRecord();
        appStat.appStat = AppStats.APP_STAT_FOREGROUND;
        recorder.write(date, appStat, proc);
        Thread.sleep(10L);
        reads = recorder.read(date, proc);
        Assert.assertFalse(reads.isEmpty());
        Assert.assertTrue(reads.get(reads.size() - 1) instanceof BatteryRecord.AppStatRecord);
        Assert.assertEquals(appStat.appStat, ((BatteryRecord.AppStatRecord) reads.get(reads.size() - 1)).appStat);

        BatteryRecord.SceneStatRecord sceneStatRecord = new BatteryRecord.SceneStatRecord();
        sceneStatRecord.scene = "Activity 1";
        recorder.write(date, sceneStatRecord, proc);
        Thread.sleep(10L);
        reads = recorder.read(date, proc);
        Assert.assertFalse(reads.isEmpty());
        Assert.assertTrue(reads.get(reads.size() - 1) instanceof BatteryRecord.SceneStatRecord);
        Assert.assertEquals(sceneStatRecord.scene, ((BatteryRecord.SceneStatRecord) reads.get(reads.size() - 1)).scene);

        BatteryRecord.DevStatRecord devStatRecord = new BatteryRecord.DevStatRecord();
        devStatRecord.devStat = AppStats.DEV_STAT_CHARGING;
        recorder.write(date, devStatRecord, proc);
        Thread.sleep(10L);
        reads = recorder.read(date, proc);
        Assert.assertFalse(reads.isEmpty());
        Assert.assertTrue(reads.get(reads.size() - 1) instanceof BatteryRecord.DevStatRecord);
        Assert.assertEquals(devStatRecord.devStat, ((BatteryRecord.DevStatRecord) reads.get(reads.size() - 1)).devStat);

        BatteryRecord.ReportRecord reportRecord = new BatteryRecord.ReportRecord();
        reportRecord.scope = "xxx";
        reportRecord.threadInfoList = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
            BatteryRecord.ReportRecord.ThreadInfo threadInfo = new BatteryRecord.ReportRecord.ThreadInfo();
            threadInfo.stat = "R" + i;
            threadInfo.tid = 10000 + i;
            threadInfo.name = "ThreadName_" + i;
            threadInfo.jiffies = SystemClock.currentThreadTimeMillis() / 10;
            reportRecord.threadInfoList.add(0, threadInfo);
        }
        reportRecord.entryList = new ArrayList<>();
        for (int i = 0; i < 4; i++) {
            BatteryRecord.ReportRecord.EntryInfo entryInfo = new BatteryRecord.ReportRecord.EntryInfo();
            entryInfo.name = "Entry Name " + i;
            entryInfo.entries = new ArrayMap<>();
            entryInfo.entries.put("Key 1", "Value 1");
            entryInfo.entries.put("Key 2", "Value 2");
            entryInfo.entries.put("Key 3", "Value 3");
            reportRecord.entryList.add(entryInfo);
        }
        recorder.write(date, reportRecord, proc);
        Thread.sleep(10L);
        reads = recorder.read(date, proc);
        Assert.assertFalse(reads.isEmpty());
        Assert.assertTrue(reads.get(reads.size() - 1) instanceof BatteryRecord.ReportRecord);
        Assert.assertEquals(reportRecord.scope, ((BatteryRecord.ReportRecord) reads.get(reads.size() - 1)).scope);
        Assert.assertEquals(5, ((BatteryRecord.ReportRecord) reads.get(reads.size() - 1)).threadInfoList.size());
        Assert.assertEquals(4, ((BatteryRecord.ReportRecord) reads.get(reads.size() - 1)).entryList.size());
    }
}
