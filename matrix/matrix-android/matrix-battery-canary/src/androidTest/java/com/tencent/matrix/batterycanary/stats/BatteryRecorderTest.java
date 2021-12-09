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

import android.content.Context;
import android.os.Process;

import com.tencent.matrix.util.MatrixLog;
import com.tencent.mmkv.MMKV;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

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
        String rootDir = MMKV.initialize(mContext);
        System.out.println("mmkv root: " + rootDir);
    }

    @After
    public void shutDown() {
    }

    @Test
    public void testRecorderWithEventRecord() {
        BatteryStatsFeature.Record.EventStatRecord record = new BatteryStatsFeature.Record.EventStatRecord();
        record.id = 22;
        record.event = "EVENT";

        String date = BatteryStatsFeature.getDateString(0);
        MMKVRecorder recorder = new MMKVRecorder(MMKV.defaultMMKV());
        recorder.clean(date);
        Assert.assertTrue(recorder.read(date).isEmpty());

        recorder.write(date, record);
        Assert.assertTrue(recorder.read(BatteryStatsFeature.getDateString(-1)).isEmpty());

        List<BatteryStatsFeature.Record> records = recorder.read(date);
        Assert.assertEquals(1, records.size());
        Assert.assertTrue(records.get(0) instanceof BatteryStatsFeature.Record.EventStatRecord);
        Assert.assertEquals(record.id, ((BatteryStatsFeature.Record.EventStatRecord) records.get(0)).id);
        Assert.assertEquals(record.event, ((BatteryStatsFeature.Record.EventStatRecord) records.get(0)).event);

        recorder.clean(BatteryStatsFeature.getDateString(-1));
        Assert.assertFalse(recorder.read(date).isEmpty());
        recorder.clean(date);
        Assert.assertTrue(recorder.read(date).isEmpty());
    }

    public static class MMKVRecorder implements BatteryStatsFeature.BatteryRecorder {
        static final String TAG = "Matrix.battery.recorder";

        final int pid = Process.myPid();
        AtomicInteger inc = new AtomicInteger(0);
        final MMKV mmkv;

        public MMKVRecorder(MMKV mmkv) {
            this.mmkv = mmkv;
        }

        @Override
        public void write(String date, BatteryStatsFeature.Record record) {
            String key = "bs-" + date + "-"  + pid +  "-" + inc.getAndIncrement();
            try {
                byte[] bytes = BatteryStatsFeature.Record.encode(record);
                mmkv.encode(key, bytes);
            } catch (Exception e) {
                MatrixLog.w(TAG, "record encode failed: " + e.getMessage());
            }
        }

        @Override
        public List<BatteryStatsFeature.Record> read(String date) {
            String[] keys = mmkv.allKeys();
            if (keys == null || keys.length == 0) {
                return Collections.emptyList();
            }
            List<BatteryStatsFeature.Record> records = new ArrayList<>(Math.min(16, keys.length));
            for (String item : keys) {
                if (item.startsWith("bs-" + date + "-")) {
                    try {
                        byte[] bytes = mmkv.decodeBytes(item);
                        if (bytes != null) {
                            BatteryStatsFeature.Record record = BatteryStatsFeature.Record.decode(bytes);
                            records.add(record);
                        }
                    } catch (Exception e) {
                        MatrixLog.w(TAG, "record decode failed: " + e.getMessage());
                    }
                }
            }
            return records;
        }

        @Override
        public void clean(String date) {
            String[] keys = mmkv.allKeys();
            if (keys == null || keys.length == 0) {
                return;
            }
            for (String item : keys) {
                if (item.startsWith("bs-" + date + "-")) {
                    try {
                        mmkv.remove(item);
                    } catch (Exception e) {
                        MatrixLog.w(TAG, "record clean failed: " + e.getMessage());
                    }
                }
            }
        }
    }
}
