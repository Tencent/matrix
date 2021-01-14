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

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.os.IBinder;
import android.os.IInterface;
import android.os.PowerManager;
import android.os.WorkSource;
import android.support.annotation.Nullable;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;
import android.util.Log;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;


@RunWith(AndroidJUnit4.class)
public class BleManagerHookerTest {
    static final String TAG = "Matrix.test.BleManagerHookerTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
    }

    @After
    public void shutDown() {
        PowerManagerServiceHooker.release();
    }

    @Test
    public void testScan() throws Exception {
        final AtomicInteger scanInc = new AtomicInteger();
        SystemServiceBinderHooker hooker = new SystemServiceBinderHooker("bluetooth_manager", "android.bluetooth.IBluetoothManager", new SystemServiceBinderHooker.HookCallback() {
            @Override
            public void onServiceMethodInvoke(Method method, Object[] args) {
                if ("getBluetoothGatt".equals(method.getName())) {
                    Assert.assertNotNull(method);
                } else if ("registerAdapter".equals(method.getName())) {
                    Assert.assertNotNull(method);
                }
            }

            @Nullable
            @Override
            public Object onServiceMethodIntercept(Object receiver, Method method, Object[] args) throws Throwable {
                if ("getBluetoothGatt".equals(method.getName())) {
                    Object blueToothGatt = method.invoke(receiver, args);
                    return proxyBluetoothGatt(blueToothGatt);
                }
                return null;
            }

            private Object proxyBluetoothGatt(final Object delegate) {
                Object proxy = null;
                try {
                    final Class<?> clazz = Class.forName("android.bluetooth.IBluetoothGatt");
                    final Class<?>[] interfaces = new Class<?>[]{IBinder.class, IInterface.class, clazz};
                    final ClassLoader loader = delegate.getClass().getClassLoader();
                    final InvocationHandler handler = new InvocationHandler() {
                        @Override
                        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
                            if ("startScan".equals(method.getName())) {
                                scanInc.incrementAndGet();
                                if (args.length > 0) {
                                    if (args[0] instanceof Integer) {
                                        int scanId = (int) args[0];
                                        ScanSettings scanSettings = null;
                                        for (Object item : args) {
                                            if (item instanceof ScanSettings) {
                                                scanSettings = (ScanSettings) item;
                                            }
                                        }

                                        Assert.assertTrue( scanId > 0);
                                        Assert.assertNotNull( scanSettings);

                                        // Sdk level compact checking needed
                                        // String callingPackage = null;
                                        // if (args[args.length - 1] instanceof String) {
                                        //     callingPackage = (String) args[args.length - 1];
                                        // }
                                        // Assert.assertEquals(mContext.getPackageName(), callingPackage);
                                    }
                                }
                            }
                            return method.invoke(delegate, args);
                        }
                    };
                    proxy = Proxy.newProxyInstance(loader, interfaces, handler);
                } catch (Exception e) {
                    e.printStackTrace();
                }
                return proxy;
            }
        });

        hooker.doHook();
        BluetoothManager bluetoothManager = (BluetoothManager) mContext.getSystemService(Context.BLUETOOTH_SERVICE);
        BluetoothAdapter adapter = bluetoothManager.getAdapter();
        adapter.startDiscovery();

        Assert.assertEquals(0, scanInc.get());

        adapter.startLeScan(new BluetoothAdapter.LeScanCallback() {
            @Override
            public void onLeScan(BluetoothDevice device, int rssi, byte[] scanRecord) {
                Assert.assertNotNull(device);
            }
        });

        Assert.assertEquals(1, scanInc.get());

        BluetoothLeScanner scanner = adapter.getBluetoothLeScanner();
        scanner.startScan(new ScanCallback() {
            @Override
            public void onScanResult(int callbackType, ScanResult result) {
                super.onScanResult(callbackType, result);
                Assert.assertNotNull(result);
            }
        });

        Assert.assertEquals(2, scanInc.get());
    }
}

