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

package com.tencent.matrix.batterycanary;

import android.os.SystemClock;

import com.google.gson.Gson;
import com.google.gson.reflect.TypeToken;
import com.tencent.matrix.batterycanary.utils.TimeBreaker;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnitRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;

import mockit.Mock;
import mockit.MockUp;

import static com.tencent.matrix.batterycanary.utils.TimeBreaker.configurePortions;
import static com.tencent.matrix.batterycanary.utils.TimeBreaker.gcList;

/**
 * Example local unit test, which will execute on the development machine (host).
 *
 * @see <a href="http://d.android.com/tools/testing">Testing documentation</a>
 */
@RunWith(MockitoJUnitRunner.class)
public class TimeBreakerTest {

    private static class SystemMock extends MockUp<SystemClock> {
        @Mock
        public static long uptimeMillis() {
            return System.currentTimeMillis();
        }
    }

    static {
        new SystemMock();
    }

    /**
     * Need mocking {@link SystemClock#uptimeMillis()}
     */
    @Test
    public void testPortions() throws InterruptedException {
        //           100s       200s       300s       400s
        //            |          |          |          |
        // +----------+----------+----------+----------+
        // |    1     |     2    |     1    |     3    |
        // 1          2          1          3          1


        List<TimeBreaker.Stamp> stampList = new ArrayList<>();
        TimeBreaker.Stamp curr = new TimeBreaker.Stamp("MOCK");
        stampList.add(0, new TimeBreaker.Stamp("1", curr.upTime));
        stampList.add(0, new TimeBreaker.Stamp("2", curr.upTime + 100 * 1000L));
        stampList.add(0, new TimeBreaker.Stamp("1", curr.upTime + 200 * 1000L));
        stampList.add(0, new TimeBreaker.Stamp("3", curr.upTime + 300 * 1000L));
        stampList.add(0, new TimeBreaker.Stamp("1", curr.upTime + 400 * 1000L));

        int delta = 10;
        int deltaRatio = 1;

        TimeBreaker.TimePortions snapshot = configurePortions(stampList, 0L);
        Assert.assertEquals(400 * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(50, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(25, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(25, snapshot.getRatio("3"), deltaRatio);
        Assert.assertEquals("1", Objects.requireNonNull(snapshot.top1()).key);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, Long.MIN_VALUE);
        Assert.assertEquals(400 * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(50, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(25, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(25, snapshot.getRatio("3"), deltaRatio);
        Assert.assertEquals("1", Objects.requireNonNull(snapshot.top1()).key);
        Assert.assertTrue(snapshot.isValid());

        // last 50 seconds
        snapshot = configurePortions(stampList, 50L * 1000L);
        Assert.assertEquals(50 * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(0, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(0, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(100, snapshot.getRatio("3"), deltaRatio);
        Assert.assertEquals("3", Objects.requireNonNull(snapshot.top1()).key);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 100L * 1000L);
        Assert.assertEquals(100 * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(0, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(0, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(100, snapshot.getRatio("3"), deltaRatio);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 150L * 1000L);
        Assert.assertEquals(150 * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(33.3, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(0, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(66.6, snapshot.getRatio("3"), deltaRatio);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 200L * 1000L);
        Assert.assertEquals(200 * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(50, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(0, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(50, snapshot.getRatio("3"), deltaRatio);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 250L * 1000L);
        Assert.assertEquals(250 * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(100 * 100 / 250f, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(100 * 50 / 250f, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(100 * 100 / 250f, snapshot.getRatio("3"), deltaRatio);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 300L * 1000L);
        Assert.assertEquals(300f * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(100 * 100 / 300f, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(100 * 100 / 300f, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(100 * 100 / 300f, snapshot.getRatio("3"), deltaRatio);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 350L * 1000L);
        Assert.assertEquals(350f * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(100 * 150 / 350f, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(100 * 100 / 350f, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(100 * 100 / 350f, snapshot.getRatio("3"), deltaRatio);
        Assert.assertEquals("1", Objects.requireNonNull(snapshot.top1()).key);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 400L * 1000L);
        Assert.assertEquals(400f * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(100 * 200 / 400f, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(100 * 100 / 400f, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(100 * 100 / 400f, snapshot.getRatio("3"), deltaRatio);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 500L * 1000L);
        Assert.assertEquals(400f * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(100 * 200 / 400f, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(100 * 100 / 400f, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(100 * 100 / 400f, snapshot.getRatio("3"), deltaRatio);
        Assert.assertFalse(snapshot.isValid());
        snapshot = configurePortions(stampList, Long.MAX_VALUE);
        Assert.assertEquals(400f * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(100 * 200 / 400f, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(100 * 100 / 400f, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(100 * 100 / 400f, snapshot.getRatio("3"), deltaRatio);
        Assert.assertFalse(snapshot.isValid());
    }


    /**
     * Need mocking {@link SystemClock#uptimeMillis()}
     */
    @Test
    public void testPortionsV2() throws InterruptedException {
        //          100s       200s       300s       400s
        //            |          |          |          |
        // +----------+----------+----------+--------------------------------
        // |    1     |     2    |     1    |              3
        // 1          2          1          3

        List<TimeBreaker.Stamp> stampList = new ArrayList<>();
        TimeBreaker.Stamp curr = new TimeBreaker.Stamp("MOCK");
        stampList.add(0, new TimeBreaker.Stamp("1", curr.upTime - 400 * 1000L));
        stampList.add(0, new TimeBreaker.Stamp("2", curr.upTime - 300 * 1000L));
        stampList.add(0, new TimeBreaker.Stamp("1", curr.upTime - 200 * 1000L));
        stampList.add(0, new TimeBreaker.Stamp("3", curr.upTime - 100 * 1000L));

        int delta = 10;
        int deltaRatio = 1;

        TimeBreaker.TimePortions snapshot = configurePortions(stampList, 0L);
        Assert.assertEquals(400 * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(50, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(25, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(25, snapshot.getRatio("3"), deltaRatio);
        Assert.assertEquals("1", Objects.requireNonNull(snapshot.top1()).key);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, Long.MIN_VALUE);
        Assert.assertEquals(400 * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(50, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(25, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(25, snapshot.getRatio("3"), deltaRatio);
        Assert.assertEquals("1", Objects.requireNonNull(snapshot.top1()).key);
        Assert.assertTrue(snapshot.isValid());

        // last 50 seconds
        snapshot = configurePortions(stampList, 50L * 1000L);
        Assert.assertEquals(50 * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(0, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(0, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(100, snapshot.getRatio("3"), deltaRatio);
        Assert.assertEquals("3", Objects.requireNonNull(snapshot.top1()).key);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 100L * 1000L);
        Assert.assertEquals(100 * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(0, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(0, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(100, snapshot.getRatio("3"), deltaRatio);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 150L * 1000L);
        Assert.assertEquals(150 * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(33.3, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(0, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(66.6, snapshot.getRatio("3"), deltaRatio);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 200L * 1000L);
        Assert.assertEquals(200 * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(50, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(0, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(50, snapshot.getRatio("3"), deltaRatio);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 250L * 1000L);
        Assert.assertEquals(250 * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(100 * 100 / 250f, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(100 * 50 / 250f, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(100 * 100 / 250f, snapshot.getRatio("3"), deltaRatio);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 300L * 1000L);
        Assert.assertEquals(300f * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(100 * 100 / 300f, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(100 * 100 / 300f, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(100 * 100 / 300f, snapshot.getRatio("3"), deltaRatio);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 350L * 1000L);
        Assert.assertEquals(350f * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(100 * 150 / 350f, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(100 * 100 / 350f, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(100 * 100 / 350f, snapshot.getRatio("3"), deltaRatio);
        Assert.assertEquals("1", Objects.requireNonNull(snapshot.top1()).key);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 400L * 1000L);
        Assert.assertEquals(400f * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(100 * 200 / 400f, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(100 * 100 / 400f, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(100 * 100 / 400f, snapshot.getRatio("3"), deltaRatio);
        Assert.assertTrue(snapshot.isValid());
        snapshot = configurePortions(stampList, 500L * 1000L);
        Assert.assertEquals(400f * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(100 * 200 / 400f, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(100 * 100 / 400f, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(100 * 100 / 400f, snapshot.getRatio("3"), deltaRatio);
        Assert.assertFalse(snapshot.isValid());
        snapshot = configurePortions(stampList, Long.MAX_VALUE);
        Assert.assertEquals(400f * 1000L, snapshot.totalUptime, delta);
        Assert.assertEquals(100 * 200 / 400f, snapshot.getRatio("1"), deltaRatio);
        Assert.assertEquals(100 * 100 / 400f, snapshot.getRatio("2"), deltaRatio);
        Assert.assertEquals(100 * 100 / 400f, snapshot.getRatio("3"), deltaRatio);
        Assert.assertFalse(snapshot.isValid());
    }


    /**
     * Need mocking {@link SystemClock#uptimeMillis()}
     */
    @Test
    public void testPortionsV3() throws InterruptedException {
        List<TimeBreaker.Stamp> stampList = new ArrayList<>();
        stampList.add(0, new TimeBreaker.Stamp("1", 0));
        stampList.add(0, new TimeBreaker.Stamp("2", 100));
        stampList.add(0, new TimeBreaker.Stamp("3", 149));
        stampList.add(0, new TimeBreaker.Stamp("4", 181));

        TimeBreaker.TimePortions snapshot = configurePortions(stampList, 40L, 10L, new TimeBreaker.Stamp.Stamper() {
            @Override
            public TimeBreaker.Stamp stamp(String key) {
                return new TimeBreaker.Stamp(key, 181);
            }
        });

        Assert.assertEquals(40L, snapshot.totalUptime, 1);
    }

    @Test
    public void testConcurrentBenchmark() throws InterruptedException {
        final List<TimeBreaker.Stamp> stampList = new ArrayList<>();
        List<Thread> threadList = new ArrayList<>();
        for (int i = 0; i < 100; i++) {
            final int finalI = i;
            Thread thread = new Thread(new Runnable() {
                @Override
                public void run() {
                    synchronized (TimeBreakerTest.class) {
                        stampList.add(0, new TimeBreaker.Stamp(String.valueOf(finalI)));
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
                    synchronized (TimeBreakerTest.class) {
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

    @Test
    public void testListGc() {
        List<TimeBreaker.Stamp> list = Collections.emptyList();
        gcList(list);
        Assert.assertSame(Collections.EMPTY_LIST, list);

        list = new ArrayList<>();
        for (int i = 0; i < 1; i++) {
            list.add(0, new TimeBreaker.Stamp(String.valueOf(i)));
        }
        gcList(list);
        Assert.assertEquals(1, list.size());
        Assert.assertEquals("0", list.get(list.size() - 1).key);

        list = new ArrayList<>();
        for (int i = 0; i < 2; i++) {
            list.add(0, new TimeBreaker.Stamp(String.valueOf(i)));
        }
        gcList(list);
        Assert.assertEquals(2, list.size());
        Assert.assertEquals("0", list.get(list.size() - 1).key);

        list = new ArrayList<>();
        for (int i = 0; i < 3; i++) {
            list.add(0, new TimeBreaker.Stamp(String.valueOf(i)));
        }
        gcList(list);
        Assert.assertEquals(2, list.size());
        Assert.assertEquals("0", list.get(list.size() - 1).key);

        list = new ArrayList<>();
        for (int i = 0; i < 4; i++) {
            list.add(0, new TimeBreaker.Stamp(String.valueOf(i)));
        }
        gcList(list);
        Assert.assertEquals(3, list.size());
        Assert.assertEquals("0", list.get(list.size() - 1).key);

        list = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
            list.add(0, new TimeBreaker.Stamp(String.valueOf(i)));
        }
        gcList(list);
        Assert.assertEquals(3, list.size());
        Assert.assertEquals("0", list.get(list.size() - 1).key);

        for (Integer item : Arrays.asList(10, 100, 1024, 2333, 65535)) {
            list = new ArrayList<>();
            for (int i = 0; i < item; i++) {
                list.add(0, new TimeBreaker.Stamp(String.valueOf(i)));
            }
            gcList(list);
            Assert.assertEquals(item - (item / 2) + (item % 2 == 0 ? 1 : 0), list.size());
            Assert.assertEquals("0", list.get(list.size() - 1).key);
        }
    }
}
