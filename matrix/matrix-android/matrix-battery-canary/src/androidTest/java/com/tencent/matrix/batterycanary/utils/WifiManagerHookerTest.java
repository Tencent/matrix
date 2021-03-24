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
import android.net.wifi.WifiManager;
import android.support.annotation.Nullable;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;

import com.tencent.matrix.batterycanary.TestUtils;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.lang.reflect.Method;
import java.util.concurrent.atomic.AtomicInteger;


@RunWith(AndroidJUnit4.class)
public class WifiManagerHookerTest {
    static final String TAG = "Matrix.test.BleManagerHookerTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
    }

    @After
    public void shutDown() {
    }

    @Test
    public void testScanning() throws Exception {
        if (TestUtils.isAssembleTest()) return;

        final AtomicInteger scanInc = new AtomicInteger();
        final AtomicInteger getScanInc = new AtomicInteger();
        SystemServiceBinderHooker hooker = new SystemServiceBinderHooker(Context.WIFI_SERVICE, "android.net.wifi.IWifiManager", new SystemServiceBinderHooker.HookCallback() {
            @Override
            public void onServiceMethodInvoke(Method method, Object[] args) {
                Assert.assertNotNull(method);
                if ("startScan".equals(method.getName())) {
                    scanInc.incrementAndGet();
                } else if ("getScanResults".equals(method.getName())) {
                    getScanInc.incrementAndGet();
                }
            }

            @Nullable
            @Override
            public Object onServiceMethodIntercept(Object receiver, Method method, Object[] args) throws Throwable {
                return null;
            }
        });

        hooker.doHook();
        Assert.assertEquals(0 ,scanInc.get());
        Assert.assertEquals(0 ,getScanInc.get());

        WifiManager wifiManager = (WifiManager) mContext.getSystemService(Context.WIFI_SERVICE);
        wifiManager.startScan();

        Assert.assertEquals(1 ,scanInc.get());
        Assert.assertEquals(0 ,getScanInc.get());

        wifiManager.getScanResults();
        Assert.assertEquals(1 ,scanInc.get());
        Assert.assertEquals(1 ,getScanInc.get());

        hooker.doUnHook();
    }

    @Test
    public void testWifiCounting() {
        final AtomicInteger scanInc = new AtomicInteger();
        final AtomicInteger getScanInc = new AtomicInteger();

        WifiManagerServiceHooker.addListener(new WifiManagerServiceHooker.IListener() {
            @Override
            public void onStartScan() {
                scanInc.incrementAndGet();
            }
            @Override
            public void onGetScanResults() {
                getScanInc.incrementAndGet();
            }
        });

        for (int i = 0; i < 100; i++) {
            Assert.assertEquals(i, scanInc.get());
            Assert.assertEquals(i, getScanInc.get());

            WifiManager wifiManager = (WifiManager) mContext.getSystemService(Context.WIFI_SERVICE);
            wifiManager.startScan();

            Assert.assertEquals(i + 1, scanInc.get());
            Assert.assertEquals(i, getScanInc.get());

            wifiManager.getScanResults();
            Assert.assertEquals(i + 1, scanInc.get());
            Assert.assertEquals(i + 1, getScanInc.get());
        }

        WifiManagerServiceHooker.release();
    }

}

