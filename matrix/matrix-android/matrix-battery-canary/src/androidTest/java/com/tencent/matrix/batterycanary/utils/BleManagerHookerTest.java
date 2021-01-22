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

import android.app.PendingIntent;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.os.IInterface;
import android.support.annotation.BinderThread;
import android.support.annotation.Nullable;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;
import android.support.v4.content.ContextCompat;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.concurrent.atomic.AtomicInteger;


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
    public void testDiscovery() {
        BluetoothManager bluetoothManager = (BluetoothManager) mContext.getSystemService(Context.BLUETOOTH_SERVICE);
        BluetoothAdapter adapter = bluetoothManager.getAdapter();
        boolean discovery = adapter.startDiscovery();
        Assert.assertEquals(ContextCompat.checkSelfPermission(mContext, "android.permission.ACCESS_FINE_LOCATION") == 0, discovery);
    }

    @Test
    public void testScanning() throws Exception {
        final AtomicInteger discInc = new AtomicInteger();
        final AtomicInteger regsInc = new AtomicInteger();
        final AtomicInteger scanInc = new AtomicInteger();
        final AtomicInteger scanForIntentInc = new AtomicInteger();
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
                if ("registerAdapter".equals(method.getName())) {
                    Object blueTooth = method.invoke(receiver, args);
                    return proxyBluetooth(blueTooth);
                } else if("getBluetoothGatt".equals(method.getName())) {
                    Object blueToothGatt = method.invoke(receiver, args);
                    return proxyBluetoothGatt(blueToothGatt);
                }
                return null;
            }

            private Object proxyBluetooth(final Object delegate) {
                Object proxy = null;
                try {
                    final Class<?> clazz = Class.forName("android.bluetooth.IBluetooth");
                    final Class<?>[] interfaces = new Class<?>[]{IBinder.class, IInterface.class, clazz};
                    final ClassLoader loader = delegate.getClass().getClassLoader();
                    final InvocationHandler handler = new InvocationHandler() {
                        @Override
                        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
                            if ("startDiscovery".equals(method.getName())) {
                                discInc.incrementAndGet();
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

            private Object proxyBluetoothGatt(final Object delegate) {
                Object proxy = null;
                try {
                    final Class<?> clazz = Class.forName("android.bluetooth.IBluetoothGatt");
                    final Class<?>[] interfaces = new Class<?>[]{IBinder.class, IInterface.class, clazz};
                    final ClassLoader loader = delegate.getClass().getClassLoader();
                    final InvocationHandler handler = new InvocationHandler() {
                        @Override
                        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
                            if ("registerScanner".equals(method.getName())) {
                                regsInc.incrementAndGet();

                            } else if ("startScan".equals(method.getName())) {
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
                            } else if ("startScanForIntent".equals(method.getName())) {
                                scanForIntentInc.incrementAndGet();
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

        Assert.assertEquals(0, discInc.get());
        Assert.assertEquals(0, regsInc.get());
        Assert.assertEquals(0, scanInc.get());
        Assert.assertEquals(0, scanForIntentInc.get());

        adapter.startDiscovery();

        Assert.assertEquals(1, discInc.get(), 1);
        Assert.assertEquals(0, regsInc.get());
        Assert.assertEquals(0, scanInc.get());
        Assert.assertEquals(0, scanForIntentInc.get());

        adapter.startLeScan(new BluetoothAdapter.LeScanCallback() {
            @Override
            public void onLeScan(BluetoothDevice device, int rssi, byte[] scanRecord) {
                Assert.assertNotNull(device);
            }
        });

        Assert.assertEquals(1, discInc.get(), 1);
        Assert.assertEquals(1, regsInc.get(), 1);
        Assert.assertEquals(1, scanInc.get(), 1);
        Assert.assertEquals(0, scanForIntentInc.get());

        BluetoothLeScanner scanner = adapter.getBluetoothLeScanner();
        scanner.startScan(new ScanCallback() {
            @Override
            public void onScanResult(int callbackType, ScanResult result) {
                super.onScanResult(callbackType, result);
                Assert.assertNotNull(result);
            }
        });

        Assert.assertEquals(1, discInc.get(), 1);
        Assert.assertEquals(2, regsInc.get(), 2);
        Assert.assertEquals(2, scanInc.get(), 2);
        Assert.assertEquals(0, scanForIntentInc.get());

        Intent intent = new Intent();
        intent.setAction("ALARM_ACTION(" + 10000 + ")");
        intent.putExtra("extra_pid", 2233);
        PendingIntent pendingIntent = PendingIntent.getBroadcast(mContext, 22, intent, 33);
        scanner.startScan(null, null, pendingIntent);

        Assert.assertEquals(1, discInc.get(), 1);
        Assert.assertEquals(2, regsInc.get(), 2);
        Assert.assertEquals(2, scanInc.get(), 2);
        Assert.assertEquals(1, scanForIntentInc.get(), 1);

        hooker.doUnHook();
    }


    @Test
    public void testBleHookerCounting() {
        final AtomicInteger regsInc = new AtomicInteger();
        final AtomicInteger scanInc = new AtomicInteger();
        SystemServiceBinderHooker hooker = new SystemServiceBinderHooker("bluetooth_manager", "android.bluetooth.IBluetoothManager", new SystemServiceBinderHooker.HookCallback() {
            @Override
            public void onServiceMethodInvoke(Method method, Object[] args) {}

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
                            if ("registerScanner".equals(method.getName())) {
                                regsInc.incrementAndGet();
                            } else if ("startScan".equals(method.getName())) {
                                scanInc.incrementAndGet();
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

        for (int i = 0; i < 1000; i++) {
            Assert.assertTrue(regsInc.get() <= i);
            Assert.assertTrue(scanInc.get() <= regsInc.get());

            adapter.startLeScan(new BluetoothAdapter.LeScanCallback() {
                @Override
                public void onLeScan(BluetoothDevice device, int rssi, byte[] scanRecord) {
                    Assert.assertNotNull(device);
                }
            });
        }

        hooker.doUnHook();
    }

    @Test
    public void testBleHooker() throws InterruptedException {
        final AtomicInteger discInc = new AtomicInteger();
        final AtomicInteger regsInc = new AtomicInteger();
        final AtomicInteger scanInc = new AtomicInteger();
        final AtomicInteger scanForIntentInc = new AtomicInteger();
        BluetoothManagerServiceHooker.addListener(new BluetoothManagerServiceHooker.IListener() {
            @Override
            public void onStartDiscovery() {
                discInc.incrementAndGet();
            }

            @Override
            public void onRegisterScanner() {
                regsInc.incrementAndGet();
            }

            @BinderThread
            @Override
            public void onStartScan(int scanId, @Nullable ScanSettings scanSettings) {
                scanInc.incrementAndGet();

            }

            @Override
            public void onStartScanForIntent(@Nullable ScanSettings scanSettings) {
                scanForIntentInc.incrementAndGet();
            }
        });

        Assert.assertEquals(0, discInc.get());
        Assert.assertEquals(0, regsInc.get());
        Assert.assertEquals(0, scanInc.get());
        Assert.assertEquals(0, scanForIntentInc.get());

        BluetoothManager bluetoothManager = (BluetoothManager) mContext.getSystemService(Context.BLUETOOTH_SERVICE);
        BluetoothAdapter adapter = bluetoothManager.getAdapter();

        adapter.startDiscovery();

        Assert.assertEquals(1, discInc.get(), 1);
        Assert.assertEquals(0, regsInc.get());
        Assert.assertEquals(0, scanInc.get());
        Assert.assertEquals(0, scanForIntentInc.get());

        adapter.startLeScan(new BluetoothAdapter.LeScanCallback() {
            @Override
            public void onLeScan(BluetoothDevice device, int rssi, byte[] scanRecord) {
                Assert.assertNotNull(device);
            }
        });

        Assert.assertEquals(1, discInc.get(), 1);
        Assert.assertEquals(1, regsInc.get(), 1);
        Assert.assertEquals(1, scanInc.get(), 1);
        Assert.assertEquals(0, scanForIntentInc.get());

        BluetoothLeScanner scanner = adapter.getBluetoothLeScanner();
        scanner.startScan(new ScanCallback() {
            @Override
            public void onScanResult(int callbackType, ScanResult result) {
                super.onScanResult(callbackType, result);
                Assert.assertNotNull(result);
            }
        });

        Assert.assertEquals(1, discInc.get(), 1);
        Assert.assertEquals(2, regsInc.get(), 2);
        Assert.assertEquals(2, scanInc.get(), 2);
        Assert.assertEquals(0, scanForIntentInc.get());

        Intent intent = new Intent();
        intent.setAction("ALARM_ACTION(" + 10000 + ")");
        intent.putExtra("extra_pid", 2233);
        PendingIntent pendingIntent = PendingIntent.getBroadcast(mContext, 22, intent, 33);
        scanner.startScan(null, null, pendingIntent);

        Assert.assertEquals(1, discInc.get(), 1);
        Assert.assertEquals(2, regsInc.get(), 2);
        Assert.assertEquals(2, scanInc.get(), 2);
        Assert.assertEquals(1, scanForIntentInc.get(), 1);

        BluetoothManagerServiceHooker.release();
    }
}

