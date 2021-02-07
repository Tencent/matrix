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
import android.annotation.TargetApi;
import android.bluetooth.le.ScanSettings;
import android.os.Build;
import android.os.IBinder;
import android.os.IInterface;
import android.support.annotation.AnyThread;
import android.support.annotation.BinderThread;
import android.support.annotation.Nullable;
import android.support.annotation.RestrictTo;

import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;

@RestrictTo(RestrictTo.Scope.LIBRARY)
public final class BluetoothManagerServiceHooker {
    private static final String TAG = "Matrix.battery.BluetoothHooker";

    public interface IListener {
        @AnyThread
        void onStartDiscovery();

        @AnyThread
        void onRegisterScanner();

        /**
         * Callback from H handler by AMS
         */
        @TargetApi(Build.VERSION_CODES.LOLLIPOP)
        @BinderThread
        void onStartScan(int scanId, @Nullable ScanSettings scanSettings);

        /**
         * Callback from H handler by AMS
         */
        @AnyThread
        @TargetApi(Build.VERSION_CODES.LOLLIPOP)
        void onStartScanForIntent(@Nullable ScanSettings scanSettings);
    }

    private static List<IListener> sListeners = new ArrayList<>();
    private static boolean sTryHook;
    private static SystemServiceBinderHooker.HookCallback sHookCallback = new SystemServiceBinderHooker.HookCallback() {
        @Override
        public void onServiceMethodInvoke(Method method, Object[] args) {}

        @Nullable
        @Override
        public Object onServiceMethodIntercept(Object receiver, Method method, Object[] args) throws Throwable {
            if ("registerAdapter".equals(method.getName())) {
                Object blueTooth = method.invoke(receiver, args);
                Object proxy = proxyBluetooth(blueTooth);
                return proxy == null ? blueTooth : proxy;
            } else if ("getBluetoothGatt".equals(method.getName())) {
                Object blueToothGatt = method.invoke(receiver, args);
                Object proxy = proxyBluetoothGatt(blueToothGatt);
                return proxy == null ? blueToothGatt : proxy;
            }
            return null;
        }
    };
    private static SystemServiceBinderHooker sHookHelper = new SystemServiceBinderHooker("bluetooth_manager", "android.bluetooth.IBluetoothManager", sHookCallback);

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public synchronized static void addListener(IListener listener) {
        if (listener == null) {
            return;
        }

        if (sListeners.contains(listener)) {
            return;
        }

        sListeners.add(listener);
        checkHook();
    }


    public synchronized static void removeListener(IListener listener) {
        if (listener == null) {
            return;
        }

        sListeners.remove(listener);
        checkUnHook();
    }

    public synchronized static void release() {
        sListeners.clear();
        checkUnHook();
    }

    private static void checkHook() {
        if (sTryHook) {
            return;
        }

        if (sListeners.isEmpty()) {
            return;
        }

        boolean hookRet = sHookHelper.doHook();
        MatrixLog.i(TAG, "checkHook hookRet:%b", hookRet);
        sTryHook = true;
    }

    private static void checkUnHook() {
        if (!sTryHook) {
            return;
        }

        if (!sListeners.isEmpty()) {
            return;
        }

        boolean unHookRet = sHookHelper.doUnHook();
        MatrixLog.i(TAG, "checkUnHook unHookRet:%b", unHookRet);
        sTryHook = false;
    }

    private static Object proxyBluetooth(final Object delegate) {
        try {
            @SuppressLint("PrivateApi")
            final Class<?> clazz = Class.forName("android.bluetooth.IBluetooth");
            final Class<?>[] interfaces = new Class<?>[]{IBinder.class, IInterface.class, clazz};
            final ClassLoader loader = delegate.getClass().getClassLoader();
            final InvocationHandler handler = new InvocationHandler() {
                @Override
                public Object invoke(Object proxy, Method method, Object[] args) {
                    if ("startDiscovery".equals(method.getName())) {
                        dispatchStartDiscovery();
                    }

                    try {
                        return method.invoke(proxy, args);
                    } catch (Throwable e) {
                        MatrixLog.printErrStackTrace(TAG, e, "invokeBluetooth fail");
                        return null;
                    }
                }
            };
            return Proxy.newProxyInstance(loader, interfaces, handler);
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "proxyBluetooth fail");
        }
        return null;
    }

    private static Object proxyBluetoothGatt(final Object delegate) {
        try {
            @SuppressLint("PrivateApi")
            final Class<?> clazz = Class.forName("android.bluetooth.IBluetoothGatt");
            final Class<?>[] interfaces = new Class<?>[]{IBinder.class, IInterface.class, clazz};
            final ClassLoader loader = delegate.getClass().getClassLoader();
            final InvocationHandler handler = new InvocationHandler() {
                @Override
                public Object invoke(Object proxy, Method method, Object[] args) {
                    if ("registerScanner".equals(method.getName())) {
                        dispatchRegisterScanner();

                    } else if ("startScan".equals(method.getName())) {
                        int scanId = -1;
                        ScanSettings scanSettings = null;
                        if (args.length > 0) {
                            if (args[0] instanceof Integer) {
                                scanId = (int) args[0];
                            }
                            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                                for (Object item : args) {
                                    if (item instanceof ScanSettings) {
                                        scanSettings = (ScanSettings) item;
                                    }
                                }
                            }
                        }
                        dispatchStartScan(scanId, scanSettings);
                    } else if ("startScanForIntent".equals(method.getName())) {
                        ScanSettings scanSettings = null;
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP && args != null) {
                            for (Object item : args) {
                                if (item instanceof ScanSettings) {
                                    scanSettings = (ScanSettings) item;
                                }
                            }
                        }
                        dispatchStartScanForIntent(scanSettings);
                    }

                    try {
                        return method.invoke(proxy, args);
                    } catch (Throwable e) {
                        MatrixLog.printErrStackTrace(TAG, e, "invokeBluetoothGatt fail");
                        return null;
                    }
                }
            };
            return Proxy.newProxyInstance(loader, interfaces, handler);
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "proxyBluetoothGatt fail");
        }
        return null;
    }

    private static void dispatchStartDiscovery() {
        for (IListener item : sListeners) {
            item.onStartDiscovery();
        }
    }

    private static void dispatchRegisterScanner() {
        for (IListener item : sListeners) {
            item.onRegisterScanner();
        }
    }

    private static void dispatchStartScan(int scanId, ScanSettings scanSettings) {
        for (IListener item : sListeners) {
            item.onStartScan(scanId, scanSettings);
        }
    }

    private static void dispatchStartScanForIntent(ScanSettings scanSettings) {
        for (IListener item : sListeners) {
            item.onStartScanForIntent(scanSettings);
        }
    }
}
