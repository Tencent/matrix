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

import android.annotation.TargetApi;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.os.Build;
import android.support.annotation.AnyThread;
import android.support.annotation.Nullable;
import android.support.annotation.RestrictTo;

import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

@RestrictTo(RestrictTo.Scope.LIBRARY)
public final class WifiManagerServiceHooker {
    private static final String TAG = "Matrix.battery.WifiHooker";

    public interface IListener {
        @AnyThread void onStartScan();
        @AnyThread void onGetScanResults();
    }

    private static List<IListener> sListeners = new ArrayList<>();
    private static boolean sTryHook;
    private static SystemServiceBinderHooker.HookCallback sHookCallback = new SystemServiceBinderHooker.HookCallback() {
        @Override
        public void onServiceMethodInvoke(Method method, Object[] args) {
            if ("startScan".equals(method.getName())) {
                dispatchStartScan();
            } else if ("getScanResults".equals(method.getName())) {
                dispatchGetScanResults();
            }
        }

        @Nullable
        @Override
        public Object onServiceMethodIntercept(Object receiver, Method method, Object[] args) {
            return null;
        }
    };
    private static SystemServiceBinderHooker sHookHelper = new SystemServiceBinderHooker(Context.WIFI_SERVICE, "android.net.wifi.IWifiManager", sHookCallback);

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

    private static void dispatchStartScan() {
        for (IListener item : sListeners) {
            item.onStartScan();
        }
    }

    private static void dispatchGetScanResults() {
        for (IListener item : sListeners) {
            item.onGetScanResults();
        }
    }
}
