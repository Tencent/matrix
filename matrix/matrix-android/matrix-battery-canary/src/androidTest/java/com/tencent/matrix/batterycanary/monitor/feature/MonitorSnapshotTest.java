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

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;
import android.support.v4.util.Pair;

import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.ListEntry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Instrumented test, which will execute on an Android device.
 *
 * @see <a href="http://d.android.com/tools/testing">Testing documentation</a>
 */
@RunWith(AndroidJUnit4.class)
public class MonitorSnapshotTest {
    static final String TAG = "Matrix.test.MonitorSnapshotTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
    }

    @After
    public void shutDown() {
    }

    @Test
    public void getSnapshotEntry() {
        //noinspection rawtypes
        Entry<Entry> mock = new Entry<Entry>() {};
        Assert.assertTrue(mock.isValid());
        Assert.assertFalse(mock.setValid(false).isValid());
    }

    @Test
    public void testDigitEntry() {
        Entry.DigitEntry<?> digitEntry = Entry.DigitEntry.of(Integer.MAX_VALUE);
        Assert.assertEquals(Integer.MAX_VALUE, digitEntry.get());
        Assert.assertTrue(digitEntry.isValid());

        digitEntry = Entry.DigitEntry.of(Long.MAX_VALUE);
        Assert.assertEquals(Long.MAX_VALUE, digitEntry.get());
        Assert.assertTrue(digitEntry.isValid());

        digitEntry = Entry.DigitEntry.of(Float.MAX_VALUE);
        Assert.assertEquals(Float.MAX_VALUE, digitEntry.get());
        Assert.assertTrue(Float.MAX_VALUE - Float.parseFloat(digitEntry.get().toString()) < 0.01);
        Assert.assertTrue(digitEntry.isValid());
    }

    @Test
    public void testDigitArrayEntry() {
        Assert.assertArrayEquals(
                ListEntry.ofDigits(new int[]{Integer.MAX_VALUE, Integer.MIN_VALUE}).getList().toArray(),
                ListEntry.ofDigits(new Integer[]{Integer.MAX_VALUE, Integer.MIN_VALUE}).getList().toArray()
        );
        Assert.assertArrayEquals(
                ListEntry.ofDigits(new long[]{Long.MAX_VALUE, Long.MIN_VALUE}).getList().toArray(),
                ListEntry.ofDigits(new Long[]{Long.MAX_VALUE, Long.MIN_VALUE}).getList().toArray()
        );
        Assert.assertArrayEquals(
                ListEntry.ofDigits(new float[]{Float.MAX_VALUE, Float.MIN_VALUE}).getList().toArray(),
                ListEntry.ofDigits(new Float[]{Float.MAX_VALUE, Float.MIN_VALUE}).getList().toArray()
        );
        // Assert.assertArrayEquals(
        //         new Double[]{Double.MAX_VALUE, Double.MIN_VALUE},
        //         ListEntry.<Double>of(new Double[]{Double.MAX_VALUE, Double.MIN_VALUE}).getList().toArray()
        // );
    }

    @Test
    public void testBeanDigit() {
        Assert.assertNull(Entry.BeanEntry.sEmpty.get());
        Assert.assertEquals("StringBean", Entry.BeanEntry.of("StringBean").get());
        Pair<Integer, String> pairEntry = new Pair<>(2233, "String");
        Assert.assertEquals(pairEntry.toString(), new Pair<>(2233, "String").toString());
    }
}
