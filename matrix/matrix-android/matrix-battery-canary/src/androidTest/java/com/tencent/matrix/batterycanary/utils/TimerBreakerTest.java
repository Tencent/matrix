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

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Objects;

import static com.tencent.matrix.batterycanary.utils.TimeBreaker.Stamp;
import static com.tencent.matrix.batterycanary.utils.TimeBreaker.TimePortions;
import static com.tencent.matrix.batterycanary.utils.TimeBreaker.configurePortions;
import static com.tencent.matrix.batterycanary.utils.TimeBreaker.gcList;


@RunWith(AndroidJUnit4.class)
public class TimerBreakerTest {
    static final String TAG = "Matrix.test.TimerBreakerTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
    }

    @After
    public void shutDown() {
    }

    @Test
    public void testPortions() throws InterruptedException {
        List<Stamp> stampList = new ArrayList<>();
        stampList.add(0, new Stamp("1"));
        Thread.sleep(100);
        stampList.add(0, new Stamp("2"));
        Thread.sleep(100);
        stampList.add(0, new Stamp("1"));
        Thread.sleep(100);
        stampList.add(0, new Stamp("3"));
        Thread.sleep(100);
        stampList.add(0, new Stamp("1"));

        int delta = 2;

        TimePortions snapshot = configurePortions(stampList, 0L);
        Assert.assertEquals(400, snapshot.totalUptime, 10);
        Assert.assertEquals(50, snapshot.getRatio("1"), delta);
        Assert.assertEquals(25, snapshot.getRatio("2"), delta);
        Assert.assertEquals(25, snapshot.getRatio("3"), delta);
        Assert.assertEquals("1", Objects.requireNonNull(snapshot.top1()).first);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, Long.MIN_VALUE);
        Assert.assertEquals(400, snapshot.totalUptime, 10);
        Assert.assertEquals(50, snapshot.getRatio("1"), delta);
        Assert.assertEquals(25, snapshot.getRatio("2"), delta);
        Assert.assertEquals(25, snapshot.getRatio("3"), delta);
        Assert.assertEquals("1", Objects.requireNonNull(snapshot.top1()).first);
        Assert.assertTrue(snapshot.isValid());

        // last 50 millis
        snapshot = configurePortions(stampList, 50L);
        Assert.assertEquals(50, snapshot.totalUptime, 10);
        Assert.assertEquals(0, snapshot.getRatio("1"), delta);
        Assert.assertEquals(0, snapshot.getRatio("2"), delta);
        Assert.assertEquals(100, snapshot.getRatio("3"), delta);
        Assert.assertEquals("3", Objects.requireNonNull(snapshot.top1()).first);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 100L);
        Assert.assertEquals(100, snapshot.totalUptime, 10);
        Assert.assertEquals(0, snapshot.getRatio("1"), delta);
        Assert.assertEquals(0, snapshot.getRatio("2"), delta);
        Assert.assertEquals(100, snapshot.getRatio("3"), delta);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 150L);
        Assert.assertEquals(150, snapshot.totalUptime, 10);
        Assert.assertEquals(33.3, snapshot.getRatio("1"), delta);
        Assert.assertEquals(0, snapshot.getRatio("2"), delta);
        Assert.assertEquals(66.6, snapshot.getRatio("3"), delta);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 200L);
        Assert.assertEquals(200, snapshot.totalUptime, 10);
        Assert.assertEquals(50, snapshot.getRatio("1"), delta);
        Assert.assertEquals(0, snapshot.getRatio("2"), delta);
        Assert.assertEquals(50, snapshot.getRatio("3"), delta);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 250L);
        Assert.assertEquals(250, snapshot.totalUptime, 10);
        Assert.assertEquals(100 * 100 / 250f, snapshot.getRatio("1"), delta);
        Assert.assertEquals(100 * 50 / 250f, snapshot.getRatio("2"), delta);
        Assert.assertEquals(100 * 100 / 250f, snapshot.getRatio("3"), delta);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 300L);
        Assert.assertEquals(300f, snapshot.totalUptime, 10);
        Assert.assertEquals(100 * 100 / 300f, snapshot.getRatio("1"), delta);
        Assert.assertEquals(100 * 100 / 300f, snapshot.getRatio("2"), delta);
        Assert.assertEquals(100 * 100 / 300f, snapshot.getRatio("3"), delta);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 350L);
        Assert.assertEquals(350f, snapshot.totalUptime, 10);
        Assert.assertEquals(100 * 150 / 350f, snapshot.getRatio("1"), delta);
        Assert.assertEquals(100 * 100 / 350f, snapshot.getRatio("2"), delta);
        Assert.assertEquals(100 * 100 / 350f, snapshot.getRatio("3"), delta);
        Assert.assertEquals("1", Objects.requireNonNull(snapshot.top1()).first);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 400L);
        Assert.assertEquals(400f, snapshot.totalUptime, 10);
        Assert.assertEquals(100 * 200 / 400f, snapshot.getRatio("1"), delta);
        Assert.assertEquals(100 * 100 / 400f, snapshot.getRatio("2"), delta);
        Assert.assertEquals(100 * 100 / 400f, snapshot.getRatio("3"), delta);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 500L);
        Assert.assertEquals(400f, snapshot.totalUptime, 10);
        Assert.assertEquals(100 * 200 / 400f, snapshot.getRatio("1"), delta);
        Assert.assertEquals(100 * 100 / 400f, snapshot.getRatio("2"), delta);
        Assert.assertEquals(100 * 100 / 400f, snapshot.getRatio("3"), delta);
        Assert.assertFalse(snapshot.isValid());
        snapshot = configurePortions(stampList, Long.MAX_VALUE);
        Assert.assertEquals(400f, snapshot.totalUptime, 10);
        Assert.assertEquals(100 * 200 / 400f, snapshot.getRatio("1"), delta);
        Assert.assertEquals(100 * 100 / 400f, snapshot.getRatio("2"), delta);
        Assert.assertEquals(100 * 100 / 400f, snapshot.getRatio("3"), delta);
        Assert.assertFalse(snapshot.isValid());
    }

    @Test
    public void testListGc() {
        List<Stamp> list = Collections.emptyList();
        gcList(list);
        Assert.assertSame(Collections.EMPTY_LIST, list);

        list = new ArrayList<>();
        for (int i = 0; i < 1; i++) {
            list.add(0, new Stamp(String.valueOf(i)));
        }
        gcList(list);
        Assert.assertEquals(1, list.size());
        Assert.assertEquals("0", list.get(list.size() - 1).key);

        list = new ArrayList<>();
        for (int i = 0; i < 2; i++) {
            list.add(0, new Stamp(String.valueOf(i)));
        }
        gcList(list);
        Assert.assertEquals(2, list.size());
        Assert.assertEquals("0", list.get(list.size() - 1).key);

        list = new ArrayList<>();
        for (int i = 0; i < 3; i++) {
            list.add(0, new Stamp(String.valueOf(i)));
        }
        gcList(list);
        Assert.assertEquals(2, list.size());
        Assert.assertEquals("0", list.get(list.size() - 1).key);

        list = new ArrayList<>();
        for (int i = 0; i < 4; i++) {
            list.add(0, new Stamp(String.valueOf(i)));
        }
        gcList(list);
        Assert.assertEquals(3, list.size());
        Assert.assertEquals("0", list.get(list.size() - 1).key);

        list = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
            list.add(0, new Stamp(String.valueOf(i)));
        }
        gcList(list);
        Assert.assertEquals(3, list.size());
        Assert.assertEquals("0", list.get(list.size() - 1).key);

        for (Integer item : Arrays.asList(10, 100, 1024, 2333, 65535)) {
            list = new ArrayList<>();
            for (int i = 0; i < item; i++) {
                list.add(0, new Stamp(String.valueOf(i)));
            }
            gcList(list);
            Assert.assertEquals(item - (item / 2) + (item % 2 == 0 ? 1 : 0), list.size());
            Assert.assertEquals("0", list.get(list.size() - 1).key);
        }
    }

    @Test
    public void testConcurrentBenchmark() throws InterruptedException {
        final List<Stamp> stampList = new ArrayList<>();
        List<Thread> threadList = new ArrayList<>();
        for (int i = 0; i < 100; i++) {
            final int finalI = i;
            Thread thread = new Thread(new Runnable() {
                @Override
                public void run() {
                    synchronized (TimerBreakerTest.class) {
                        stampList.add(0, new Stamp(String.valueOf(finalI)));
                    }
                }
            });
            thread.start();
            threadList.add(thread);
        }

        for (int i = 0; i < 100; i++) {
            Thread thread = new Thread(new Runnable() {
                @Override
                public void run() {
                    synchronized (TimerBreakerTest.class) {
                        TimeBreaker.gcList(stampList);
                    }
                }
            });
            thread.start();
            threadList.add(thread);
        }

        for (Thread item : threadList) {
            item.join();
        }
    }
}
