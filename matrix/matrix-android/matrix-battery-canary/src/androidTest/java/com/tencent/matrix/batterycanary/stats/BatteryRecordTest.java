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
import android.os.Parcel;
import android.os.SystemClock;
import android.util.ArrayMap;

import com.tencent.mmkv.MMKV;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

/**
 * Instrumented test, which will execute on an Android device.
 *
 * @see <a href="http://d.android.com/tools/testing">Testing documentation</a>
 */
@RunWith(AndroidJUnit4.class)
public class BatteryRecordTest {
    static final String TAG = "Matrix.test.BatteryRecordTest";

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
    public void testEventRecordIOManipulation() {
        BatteryRecorder.Record.EventStatRecord record = new BatteryRecorder.Record.EventStatRecord();
        record.id = 22;
        record.event = "EVENT";

        MMKV mmkv = MMKV.defaultMMKV();
        String key = "stats_event_" + record.id;

        Parcel parcel = null;
        try {
            parcel = Parcel.obtain();
            record.writeToParcel(parcel, 0);
            mmkv.encode(key, parcel.marshall());
        } finally {
            if (parcel != null) {
                parcel.recycle();
            }
        }

        byte[] bytes = mmkv.decodeBytes(key);
        Assert.assertNotNull(bytes);

        try {
            parcel = Parcel.obtain();
            parcel.unmarshall(bytes, 0, bytes.length);
            parcel.setDataPosition(0);
            BatteryRecorder.Record.EventStatRecord recordLoaded = BatteryRecorder.Record.EventStatRecord.CREATOR.createFromParcel(parcel);
            Assert.assertNotNull(recordLoaded);

        } finally {
            parcel.recycle();
            mmkv.remove(key);
        }
    }

    @Test
    public void testReportRecordIOManipulation() {
        BatteryRecorder.Record.ReportRecord record = new BatteryRecorder.Record.ReportRecord();
        record.id = record.hashCode();
        record.threadInfoList = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
            BatteryRecorder.Record.ReportRecord.ThreadInfo threadInfo = new BatteryRecorder.Record.ReportRecord.ThreadInfo();
            threadInfo.stat = "R" + i;
            threadInfo.tid = 10000 + i;
            threadInfo.name = "ThreadName_" + i;
            threadInfo.jiffies = SystemClock.currentThreadTimeMillis() / 10;
            record.threadInfoList.add(0, threadInfo);
        }
        record.entryList = new ArrayList<>();
        for (int i = 0; i < 4; i++) {
            BatteryRecorder.Record.ReportRecord.EntryInfo entryInfo = new BatteryRecorder.Record.ReportRecord.EntryInfo();
            entryInfo.name = "Entry Name " + i;
            entryInfo.entries = new ArrayMap<>();
            entryInfo.entries.put("Key 1", "Value 1");
            entryInfo.entries.put("Key 2", "Value 2");
            entryInfo.entries.put("Key 3", "Value 3");
            record.entryList.add(entryInfo);
        }

        MMKV mmkv = MMKV.defaultMMKV();
        String key = "stats_report_" + record.id;

        Parcel parcel = null;
        try {
            parcel = Parcel.obtain();
            record.writeToParcel(parcel, 0);
            mmkv.encode(key, parcel.marshall());
        } finally {
            if (parcel != null) {
                parcel.recycle();
            }
        }

        byte[] bytes = mmkv.decodeBytes(key);
        Assert.assertNotNull(bytes);

        try {
            parcel = Parcel.obtain();
            parcel.unmarshall(bytes, 0, bytes.length);
            parcel.setDataPosition(0);
            BatteryRecorder.Record.ReportRecord recordLoaded = BatteryRecorder.Record.ReportRecord.CREATOR.createFromParcel(parcel);
            Assert.assertNotNull(recordLoaded);

        } finally {
            parcel.recycle();
            mmkv.remove(key);
        }
    }

    @Test
    public void testUniversalRecordIOManipulation() {
        MMKV mmkv = MMKV.defaultMMKV();

        BatteryRecorder.Record.EventStatRecord eventRecord = new BatteryRecorder.Record.EventStatRecord();
        eventRecord.id = 22;
        eventRecord.event = "EVENT";

        String key1 = "stats_" + eventRecord.id;
        Parcel parcel = null;
        try {
            parcel = Parcel.obtain();
            parcel.writeInt(1);
            eventRecord.writeToParcel(parcel, 0);
            mmkv.encode(key1, parcel.marshall());
        } finally {
            if (parcel != null) {
                parcel.recycle();
            }
        }

        BatteryRecorder.Record.ReportRecord reportRecord = new BatteryRecorder.Record.ReportRecord();
        reportRecord.id = 33;
        reportRecord.threadInfoList = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
            BatteryRecorder.Record.ReportRecord.ThreadInfo threadInfo = new BatteryRecorder.Record.ReportRecord.ThreadInfo();
            threadInfo.stat = "R" + i;
            threadInfo.tid = 10000 + i;
            threadInfo.name = "ThreadName_" + i;
            threadInfo.jiffies = SystemClock.currentThreadTimeMillis() / 10;
            reportRecord.threadInfoList.add(0, threadInfo);
        }
        reportRecord.entryList = new ArrayList<>();
        for (int i = 0; i < 4; i++) {
            BatteryRecorder.Record.ReportRecord.EntryInfo entryInfo = new BatteryRecorder.Record.ReportRecord.EntryInfo();
            entryInfo.name = "Entry Name " + i;
            entryInfo.entries = new ArrayMap<>();
            entryInfo.entries.put("Key 1", "Value 1");
            entryInfo.entries.put("Key 2", "Value 2");
            entryInfo.entries.put("Key 3", "Value 3");
            reportRecord.entryList.add(entryInfo);
        }

        String key2 = "stats_" + reportRecord.id;
        try {
            parcel = Parcel.obtain();
            parcel.writeInt(2);
            reportRecord.writeToParcel(parcel, 0);
            mmkv.encode(key2, parcel.marshall());
        } finally {
            parcel.recycle();
        }

        byte[] bytes = mmkv.decodeBytes(key1);
        Assert.assertNotNull(bytes);
        try {
            parcel = Parcel.obtain();
            parcel.unmarshall(bytes, 0, bytes.length);
            parcel.setDataPosition(0);
            int type = parcel.readInt();
            Assert.assertEquals(1, type);
            BatteryRecorder.Record.EventStatRecord recordLoaded = BatteryRecorder.Record.EventStatRecord.CREATOR.createFromParcel(parcel);
            Assert.assertNotNull(recordLoaded);

        } finally {
            parcel.recycle();
            mmkv.remove(key1);
        }

        bytes = mmkv.decodeBytes(key2);
        Assert.assertNotNull(bytes);
        try {
            parcel = Parcel.obtain();
            parcel.unmarshall(bytes, 0, bytes.length);
            parcel.setDataPosition(0);
            int type = parcel.readInt();
            Assert.assertEquals(2, type);
            BatteryRecorder.Record.ReportRecord recordLoaded = BatteryRecorder.Record.ReportRecord.CREATOR.createFromParcel(parcel);
            Assert.assertNotNull(recordLoaded);

        } finally {
            parcel.recycle();
            mmkv.remove(key1);
        }
    }

    @Test
    public void testEventRecordIOManipulationBenchmark() {
        MMKV mmkv = MMKV.defaultMMKV();
        int count = 1000;

        for (int i = 0; i < count; i++) {
            String key = "stats_event_" + i;
            BatteryRecorder.Record.EventStatRecord record = new BatteryRecorder.Record.EventStatRecord();
            record.id = i;
            record.event = "EVENT_" + i;

            Parcel parcel = null;
            try {
                parcel = Parcel.obtain();
                record.writeToParcel(parcel, 0);
                mmkv.encode(key, parcel.marshall());
            } finally {
                if (parcel != null) {
                    parcel.recycle();
                }
            }
        }

        for (int i = 0; i < count; i++) {
            String key = "stats_event_" + i;
            byte[] bytes = mmkv.decodeBytes(key);
            Assert.assertNotNull(bytes);

            Parcel parcel = null;
            try {
                parcel = Parcel.obtain();
                parcel.unmarshall(bytes, 0, bytes.length);
                parcel.setDataPosition(0);
                BatteryRecorder.Record.EventStatRecord recordLoaded = BatteryRecorder.Record.EventStatRecord.CREATOR.createFromParcel(parcel);
                Assert.assertNotNull(recordLoaded);

            } finally {
                if (parcel != null) {
                    parcel.recycle();
                }
                mmkv.remove(key);
            }
        }
    }
}
