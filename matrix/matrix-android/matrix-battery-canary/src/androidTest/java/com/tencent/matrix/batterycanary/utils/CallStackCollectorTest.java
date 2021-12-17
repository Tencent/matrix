package com.tencent.matrix.batterycanary.utils;

import android.content.Context;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;

import com.tencent.matrix.batterycanary.TestUtils;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

/**
 * @author Kaede
 * @since 2021/12/17
 */
@RunWith(AndroidJUnit4.class)
public class CallStackCollectorTest {
    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    @Test
    public void testUnwindWithThrowable() {
        final RuntimeException[] throwable = new RuntimeException[1];
        Thread testThread = new Thread(new Runnable() {
            @Override
            public void run() {
                throwable[0] = new RuntimeException();
            }
        }, "test-thread");
        testThread.start();

        while (throwable[0] == null) {}
        StackTraceElement[] stackTrace = throwable[0].getStackTrace();
        Assert.assertNotNull(stackTrace);

        String mine = Log.getStackTraceString(new Throwable());
        String they = Log.getStackTraceString(throwable[0]);
        Assert.assertNotEquals(mine, they);
        Assert.assertFalse(mine.contains("at "+ CallStackCollectorTest.class.getName() +"$1.run("));
        Assert.assertTrue(they.contains("at "+ CallStackCollectorTest.class.getName() +"$1.run("));

        if (!TestUtils.isAssembleTest()) {
            throw throwable[0];
        }
    }


    @RunWith(AndroidJUnit4.class)
    public static class Benchmark {

        Context mContext;

        @Before
        public void setUp() {
            mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        }

        @After
        public void shutDown() {
        }

        @Test
        @SuppressWarnings("ThrowableNotThrown")
        public void testProcStatBenchmark() {
            if (TestUtils.isAssembleTest()) return;

            long delta1, delta2;
            long bgnMillis, endMillis;

            int loopCount = 100000;

            bgnMillis = SystemClock.uptimeMillis();
            for (int i = 0; i < loopCount; i++) {
                new Throwable().getStackTrace();
            }
            endMillis = SystemClock.uptimeMillis();
            delta1 = endMillis - bgnMillis;

            bgnMillis = SystemClock.uptimeMillis();
            for (int i = 0; i < loopCount; i++) {
                Thread.currentThread().getStackTrace();
                // Looper.getMainLooper().getThread().getStackTrace();
            }
            endMillis = SystemClock.uptimeMillis();
            delta2 = endMillis - bgnMillis;

            Assert.fail("TIME CONSUMED: " + delta1 + " vs " + delta2);
        }
    }
}
