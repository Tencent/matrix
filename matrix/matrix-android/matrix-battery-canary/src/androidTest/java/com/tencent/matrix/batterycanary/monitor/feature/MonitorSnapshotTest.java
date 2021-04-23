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
import android.util.Pair;

import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Differ.BeanDiffer;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Differ.DigitDiffer;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Differ.ListDiffer;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.BeanEntry;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.DigitEntry;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.ListEntry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;

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
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    @After
    public void shutDown() {
    }

    @Test
    public void getSnapshotEntry() {
        //noinspection rawtypes
        Entry<Entry> mock = new Entry<Entry>() {
        };
        Assert.assertTrue(mock.isValid());
        Assert.assertFalse(mock.setValid(false).isValid());
    }

    @Test
    public void testDigitEntry() {
        DigitEntry<?> digitEntry = DigitEntry.of(Integer.MAX_VALUE);
        Assert.assertEquals(Integer.MAX_VALUE, digitEntry.get());
        Assert.assertTrue(digitEntry.isValid());

        digitEntry = DigitEntry.of(Long.MAX_VALUE);
        Assert.assertEquals(Long.MAX_VALUE, digitEntry.get());
        Assert.assertTrue(digitEntry.isValid());

        digitEntry = DigitEntry.of(Float.MAX_VALUE);
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
        Assert.assertArrayEquals(
                ListEntry.ofDigits(new Double[]{Double.MAX_VALUE, Double.MIN_VALUE}).getList().toArray(),
                ListEntry.ofDigits(new Double[]{Double.MAX_VALUE, Double.MIN_VALUE}).getList().toArray()
        );
    }

    @Test
    public void testDigitDiffer() {
        // MonitorFeature.Snapshot.Differ.DigitDiffer<Number> digitDiffer = new MonitorFeature.Snapshot.Differ.DigitDiffer<>();
        // digitDiffer.diff(Entry.DigitEntry.of(33), Entry.DigitEntry.of(22))
        Assert.assertEquals(33 - 22,
                DigitDiffer.globalDiff(DigitEntry.of(22), DigitEntry.of(33)).get().intValue()
        );
        Assert.assertEquals((long) 33 - 22,
                DigitDiffer.globalDiff(DigitEntry.of(22L), DigitEntry.of(33L)).get().longValue()
        );
        Assert.assertEquals(Float.valueOf(33 - 22),
                DigitDiffer.globalDiff(DigitEntry.of(22F), DigitEntry.of(33F)).get()
        );
        Assert.assertEquals(Double.valueOf(33 - 22),
                DigitDiffer.globalDiff(DigitEntry.of(22D), DigitEntry.of(33D)).get()
        );
    }

    @Test
    public void testDigitArrayDiffer() {
        ListEntry<DigitEntry<Integer>> diffListEntry = ListDiffer.globalDiff(ListEntry.ofDigits(new int[]{1, 2, 3}), ListEntry.ofDigits(new int[]{1 + 3, 2 + 2, 3 + 1}));
        Assert.assertArrayEquals(
                ListEntry.ofDigits(new Integer[]{3, 2, 1}).getList().toArray(),
                diffListEntry.getList().toArray()
        );
        for (DigitEntry<?> item : diffListEntry.getList()) {
            Assert.assertTrue(item.isValid());
        }

        diffListEntry = ListDiffer.globalDiff(ListEntry.ofDigits(new int[]{1, 2}), ListEntry.ofDigits(new int[]{1 + 3, 2 + 2, 3 + 1}));
        Assert.assertArrayEquals(
                ListEntry.ofDigits(new Integer[]{3, 2, 3 + 1}).getList().toArray(),
                diffListEntry.getList().toArray()
        );
        Assert.assertFalse(diffListEntry.getList().get(2).isValid());
    }

    @Test
    public void testBeanDigit() {
        Assert.assertNull(BeanEntry.sEmpty.get());
        Assert.assertEquals("StringBean", BeanEntry.of("StringBean").get());
        Pair<Integer, String> pairEntry = new Pair<>(2233, "String");
        Assert.assertEquals(pairEntry.toString(), new Pair<>(2233, "String").toString());
    }

    @Test
    public void testBeanDigitDiffer() {
        String bean1 = "BEAN_1";
        String bean2 = "BEAN_2";
        Assert.assertEquals(BeanEntry.sEmpty, BeanDiffer.globalDiff(BeanEntry.of(bean1), BeanEntry.of(bean1)));
        Assert.assertEquals(BeanEntry.sEmpty, BeanDiffer.globalDiff(BeanEntry.of(bean2), BeanEntry.of(bean2)));
        Assert.assertEquals(BeanEntry.of(bean2), BeanDiffer.globalDiff(BeanEntry.of(bean1), BeanEntry.of(bean2)));
        Assert.assertEquals(BeanEntry.of(bean1), BeanDiffer.globalDiff(BeanEntry.of(bean2), BeanEntry.of(bean1)));
    }

    @Test
    public void testBeanDigitArrayDiffer() {
        String bean1 = "BEAN_1";
        String bean2 = "BEAN_2";
        String bean3 = "BEAN_3";

        ListEntry<BeanEntry<String>> diffListEntry = ListDiffer.globalDiff(
                ListEntry.of(Arrays.asList(BeanEntry.of(bean1), BeanEntry.of(bean2), BeanEntry.of(bean3))),
                ListEntry.of(Arrays.asList(BeanEntry.of(bean1), BeanEntry.of(bean2), BeanEntry.of(bean3)))
        );
        Assert.assertEquals(ListEntry.ofEmpty().getList(), diffListEntry.getList());

        diffListEntry = ListDiffer.globalDiff(
                ListEntry.of(Arrays.asList(BeanEntry.of(bean2), BeanEntry.of(bean3))),
                ListEntry.of(Arrays.asList(BeanEntry.of(bean1), BeanEntry.of(bean2), BeanEntry.of(bean3)))
        );
        Assert.assertEquals(
                ListEntry.of(Arrays.asList(BeanEntry.of(bean1))).getList(),
                diffListEntry.getList()
        );

        diffListEntry = ListDiffer.globalDiff(
                ListEntry.of(Arrays.asList(BeanEntry.of(bean3))),
                ListEntry.of(Arrays.asList(BeanEntry.of(bean1), BeanEntry.of(bean2)))
        );
        Assert.assertEquals(
                ListEntry.of(Arrays.asList(BeanEntry.of(bean1), BeanEntry.of(bean2))).getList(),
                diffListEntry.getList()
        );

        diffListEntry = ListDiffer.globalDiff(
                ListEntry.of(Arrays.asList(BeanEntry.of(bean1), BeanEntry.of(bean2))),
                ListEntry.of(Arrays.asList(BeanEntry.of(bean3)))
        );
        Assert.assertEquals(
                ListEntry.of(Arrays.asList(BeanEntry.of(bean3))).getList(),
                diffListEntry.getList()
        );

        diffListEntry = ListDiffer.globalDiff(
                ListEntry.<BeanEntry<String>>ofEmpty(),
                ListEntry.of(Arrays.asList(BeanEntry.of(bean1), BeanEntry.of(bean2), BeanEntry.of(bean3)))
        );
        Assert.assertEquals(
                ListEntry.of(Arrays.asList(BeanEntry.of(bean1), BeanEntry.of(bean2), BeanEntry.of(bean3))).getList(),
                diffListEntry.getList()
        );

    }
}
