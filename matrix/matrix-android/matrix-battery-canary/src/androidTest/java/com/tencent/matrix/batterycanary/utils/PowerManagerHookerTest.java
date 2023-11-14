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

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.IBinder;
import android.os.PowerManager;
import android.os.WorkSource;
import android.text.TextUtils;
import android.util.Log;

import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;


@RunWith(AndroidJUnit4.class)
public class PowerManagerHookerTest {
    static final String TAG = "Matrix.test.MonitorFeatureWakeLockTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    @After
    public void shutDown() {
        PowerManagerServiceHooker.release();
    }

    @Ignore
    @Test
    public void testAcquireWakeup() throws Exception {
        final AtomicBoolean hasAcquired = new AtomicBoolean();
        final AtomicBoolean hasRelease = new AtomicBoolean();
        final AtomicReference<PowerManager.WakeLock> wakLockRef = new AtomicReference<>();
        PowerManagerServiceHooker.addListener(new PowerManagerServiceHooker.IListener() {
            @Override
            public void onAcquireWakeLock(IBinder token, int flags, String tag, String packageName, WorkSource workSource, String historyTag) {
                try {
                    Field mToken = PowerManager.WakeLock.class.getDeclaredField("mToken");
                    mToken.setAccessible(true);
                    Assert.assertEquals(mToken.get(wakLockRef.get()), token);
                    Assert.assertEquals(PowerManager.PARTIAL_WAKE_LOCK, flags);
                    Assert.assertEquals(TAG, tag);
                    hasAcquired.set(true);
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }

            @Override
            public void onReleaseWakeLock(IBinder token, int flags) {
                try {
                    Field mToken = PowerManager.WakeLock.class.getDeclaredField("mToken");
                    mToken.setAccessible(true);
                    Assert.assertEquals(mToken.get(wakLockRef.get()), token);
                    Assert.assertEquals(0, flags);
                    hasRelease.set(true);
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        });

        PowerManager pm = (PowerManager) mContext.getSystemService(Context.POWER_SERVICE);
        PowerManager.WakeLock wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, TAG);
        wakLockRef.set(wakeLock);
        String wakeLockTag = getWakeLockTag(wakeLock);
        Assert.assertEquals(TAG, wakeLockTag);
        Assert.assertNotNull(wakeLock);

        wakeLock.acquire();
        Thread.sleep(100L);
        Assert.assertTrue(hasAcquired.get());
        Assert.assertFalse(hasRelease.get());
        wakeLock.release();
        Thread.sleep(100L);
        Assert.assertTrue(hasAcquired.get());
        Assert.assertTrue(hasRelease.get());
    }

    private static String getWakeLockTag(PowerManager.WakeLock wakeLock) {
        String tag = null;
        try {
            @SuppressWarnings("JavaReflectionMemberAccess")
            @SuppressLint({"SoonBlockedPrivateApi"})
            Method method = wakeLock.getClass().getDeclaredMethod("getTag");
            method.setAccessible(true);
            tag = (String) method.invoke(wakeLock);
        } catch (Throwable e) {
            Log.e(TAG, "getTag err: " + e);
        }
        if (TextUtils.isEmpty(tag)) {
            try {
                @SuppressWarnings("JavaReflectionMemberAccess")
                @SuppressLint("DiscouragedPrivateApi")
                Field field = wakeLock.getClass().getDeclaredField("mTag");
                field.setAccessible(true);
                tag = (String) field.get(wakeLock);
            } catch (Throwable e) {
                Log.e(TAG, "mTag err: " + e);
            }
        }
        if (TextUtils.isEmpty(tag)) {
            try {
                @SuppressWarnings("JavaReflectionMemberAccess")
                @SuppressLint({"SoonBlockedPrivateApi"})
                Field field = wakeLock.getClass().getDeclaredField("mTraceName");
                field.setAccessible(true);
                tag = (String) field.get(wakeLock);
                if (tag != null && tag.startsWith("WakeLock (") && tag.endsWith(")")) {
                    int idxBgn = tag.indexOf("WakeLock (") + "WakeLock (".length();
                    int idxEnd = tag.lastIndexOf(")");
                    if (idxBgn < idxEnd) {
                        tag = tag.substring(idxBgn, idxEnd);
                    }
                }
            } catch (Throwable e) {
                Log.e(TAG, "mTraceName err: " + e);
            }
        }

        Log.i(TAG, "getWakeLockTag: " + tag);
        return tag;
    }

    @Ignore
    @Test
    public void testAcquireWakeupTimeout() throws Exception {
        final AtomicBoolean hasAcquired = new AtomicBoolean();
        final AtomicBoolean hasRelease = new AtomicBoolean();
        final AtomicReference<PowerManager.WakeLock> wakLockRef = new AtomicReference<>();
        PowerManagerServiceHooker.addListener(new PowerManagerServiceHooker.IListener() {
            @Override
            public void onAcquireWakeLock(IBinder token, int flags, String tag, String packageName, WorkSource workSource, String historyTag) {
                try {
                    Field mToken = PowerManager.WakeLock.class.getDeclaredField("mToken");
                    mToken.setAccessible(true);
                    Assert.assertEquals(mToken.get(wakLockRef.get()), token);
                    Assert.assertEquals(PowerManager.PARTIAL_WAKE_LOCK, flags);
                    Assert.assertEquals(TAG, tag);
                    hasAcquired.set(true);
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }

            @Override
            public void onReleaseWakeLock(IBinder token, int flags) {
                try {
                    Field mToken = PowerManager.WakeLock.class.getDeclaredField("mToken");
                    mToken.setAccessible(true);
                    Assert.assertEquals(mToken.get(wakLockRef.get()), token);
                    Assert.assertEquals(65536, flags);
                    hasRelease.set(true);
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        });

        PowerManager pm = (PowerManager) mContext.getSystemService(Context.POWER_SERVICE);
        PowerManager.WakeLock wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, TAG);
        wakLockRef.set(wakeLock);
        Assert.assertNotNull(wakeLock);

        wakeLock.acquire(1000L);
        Thread.sleep(100L);
        Assert.assertTrue(hasAcquired.get());
        Assert.assertFalse(hasRelease.get());
        Thread.sleep(1000L);
        Assert.assertTrue(hasAcquired.get());
        Assert.assertTrue(hasRelease.get());
    }
}
