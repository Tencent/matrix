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
import android.os.Parcelable;
import android.os.SystemClock;
import android.util.ArrayMap;

import com.tencent.mmkv.MMKV;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

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
        BatteryRecord.EventStatRecord record = new BatteryRecord.EventStatRecord();
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
            BatteryRecord.EventStatRecord recordLoaded = BatteryRecord.EventStatRecord.CREATOR.createFromParcel(parcel);
            Assert.assertNotNull(recordLoaded);

        } finally {
            parcel.recycle();
            mmkv.remove(key);
        }
    }

    @Test
    public void testReportRecordIOManipulation() {
        BatteryRecord.ReportRecord record = new BatteryRecord.ReportRecord();
        record.id = record.hashCode();
        record.threadInfoList = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
            BatteryRecord.ReportRecord.ThreadInfo threadInfo = new BatteryRecord.ReportRecord.ThreadInfo();
            threadInfo.stat = "R" + i;
            threadInfo.tid = 10000 + i;
            threadInfo.name = "ThreadName_" + i;
            threadInfo.jiffies = SystemClock.currentThreadTimeMillis() / 10;
            record.threadInfoList.add(0, threadInfo);
        }
        record.entryList = new ArrayList<>();
        for (int i = 0; i < 4; i++) {
            BatteryRecord.ReportRecord.EntryInfo entryInfo = new BatteryRecord.ReportRecord.EntryInfo();
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
            BatteryRecord.ReportRecord recordLoaded = BatteryRecord.ReportRecord.CREATOR.createFromParcel(parcel);
            Assert.assertNotNull(recordLoaded);

        } finally {
            parcel.recycle();
            mmkv.remove(key);
        }
    }

    @Test
    public void testUniversalRecordIOManipulation() {
        MMKV mmkv = MMKV.defaultMMKV();

        BatteryRecord.EventStatRecord eventRecord = new BatteryRecord.EventStatRecord();
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

        BatteryRecord.ReportRecord reportRecord = new BatteryRecord.ReportRecord();
        reportRecord.id = 33;
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
            BatteryRecord.EventStatRecord recordLoaded = BatteryRecord.EventStatRecord.CREATOR.createFromParcel(parcel);
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
            BatteryRecord.ReportRecord recordLoaded = BatteryRecord.ReportRecord.CREATOR.createFromParcel(parcel);
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
            BatteryRecord.EventStatRecord record = new BatteryRecord.EventStatRecord();
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
                BatteryRecord.EventStatRecord recordLoaded = BatteryRecord.EventStatRecord.CREATOR.createFromParcel(parcel);
                Assert.assertNotNull(recordLoaded);

            } finally {
                if (parcel != null) {
                    parcel.recycle();
                }
                mmkv.remove(key);
            }
        }
    }

    @Test
    public void testRecordWithType() {
        BatteryRecord.DevStatRecord devStatRecord = new BatteryRecord.DevStatRecord();
        devStatRecord.devStat = 5;
        byte[] bytes = BatteryRecord.encode(devStatRecord);
        Assert.assertNotNull(devStatRecord);
        BatteryRecord record = BatteryRecord.decode(bytes);
        Assert.assertTrue(record instanceof BatteryRecord.DevStatRecord);
        Assert.assertEquals(devStatRecord.version, devStatRecord.version);
        Assert.assertEquals(devStatRecord.millis, devStatRecord.millis);
        Assert.assertEquals(devStatRecord.devStat, devStatRecord.devStat);

        MMKV mmkv = MMKV.defaultMMKV();
        String key = "test_key";
        mmkv.encode(key, bytes);
        bytes = mmkv.decodeBytes(key);
        record = BatteryRecord.decode(bytes);
        Assert.assertEquals(devStatRecord.version, devStatRecord.version);
        Assert.assertEquals(devStatRecord.millis, devStatRecord.millis);
        Assert.assertEquals(devStatRecord.devStat, devStatRecord.devStat);
    }

    @Test
    public void testVersionControl() {
        Version0 record = new Version0();
        record.id = 22;
        record.event = "Version0";

        byte[] bytes = null;
        Parcel parcel = null;
        try {
            parcel = Parcel.obtain();
            record.writeToParcel(parcel, 0);
            bytes = parcel.marshall();
        } finally {
            if (parcel != null) {
                parcel.recycle();
            }
        }

        Assert.assertNotNull(bytes);

        try {
            parcel = Parcel.obtain();
            parcel.unmarshall(bytes, 0, bytes.length);
            parcel.setDataPosition(0);
            Version1 recordLoaded = Version1.CREATOR.createFromParcel(parcel);
            Assert.assertNotNull(recordLoaded);
            Assert.assertEquals(record.id, recordLoaded.id);
            Assert.assertEquals(record.event, recordLoaded.event);
            Assert.assertSame(recordLoaded.extras, Collections.<String, Object>emptyMap());
        } finally {
            parcel.recycle();
        }

        Version1 recordNew = new Version1();
        recordNew.id = 22;
        recordNew.event = "Version1";
        recordNew.extras = new HashMap<>();
        recordNew.extras.put("xxx", "yyy");
        recordNew.extras.put("zzz", 1000);

        bytes = null;
        try {
            parcel = Parcel.obtain();
            recordNew.writeToParcel(parcel, 0);
            bytes = parcel.marshall();
        } finally {
            parcel.recycle();
        }

        Assert.assertNotNull(bytes);

        try {
            parcel = Parcel.obtain();
            parcel.unmarshall(bytes, 0, bytes.length);
            parcel.setDataPosition(0);
            Version1 recordLoaded = Version1.CREATOR.createFromParcel(parcel);
            Assert.assertNotNull(recordLoaded);
            Assert.assertEquals(recordNew.id, recordLoaded.id);
            Assert.assertEquals(recordNew.event, recordLoaded.event);
            Assert.assertEquals(recordNew.extras, recordLoaded.extras);
            Assert.assertNotSame(recordLoaded.extras, Collections.<String, Object>emptyMap());
        } finally {
            parcel.recycle();
        }

        try {
            parcel = Parcel.obtain();
            parcel.unmarshall(bytes, 0, bytes.length);
            parcel.setDataPosition(0);
            Version0 recordLoaded = Version0.CREATOR.createFromParcel(parcel);
            Assert.assertNotNull(recordLoaded);
            Assert.assertEquals(recordNew.id, recordLoaded.id);
            Assert.assertEquals(recordNew.event, recordLoaded.event);
        } finally {
            parcel.recycle();
        }
    }


    public static class Version0 extends BatteryRecord implements Parcelable {
        public static final int VERSION = 0;

        public long id;
        public String event;

        public Version0() {
            id = 0;
            version = VERSION;
        }

        protected Version0(Parcel in) {
            super(in);
            id = in.readLong();
            event = in.readString();
        }

        public static final Creator<Version0> CREATOR = new Creator<Version0>() {
            @Override
            public Version0 createFromParcel(Parcel in) {
                return new Version0(in);
            }

            @Override
            public Version0[] newArray(int size) {
                return new Version0[size];
            }
        };

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            super.writeToParcel(dest, flags);
            dest.writeLong(id);
            dest.writeString(event);
        }
    }

    public static class Version1 extends BatteryRecord implements Parcelable {
        public static final int VERSION = 1;

        public long id;
        public String event;
        public Map<String, Object> extras = Collections.emptyMap();  // Since version 1

        public Version1() {
            id = 0;
            version = VERSION;
        }

        protected Version1(Parcel in) {
            super(in);
            id = in.readLong();
            event = in.readString();
            if (version >= 1) {
                extras = new HashMap<>();
                in.readMap(extras, getClass().getClassLoader());
            }
        }

        public static final Creator<Version1> CREATOR = new Creator<Version1>() {
            @Override
            public Version1 createFromParcel(Parcel in) {
                return new Version1(in);
            }

            @Override
            public Version1[] newArray(int size) {
                return new Version1[size];
            }
        };

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            super.writeToParcel(dest, flags);
            dest.writeLong(id);
            dest.writeString(event);
            dest.writeMap(extras);
        }
    }
}
