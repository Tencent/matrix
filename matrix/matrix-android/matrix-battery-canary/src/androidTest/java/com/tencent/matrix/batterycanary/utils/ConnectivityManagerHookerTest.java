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
import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.SystemClock;

import com.tencent.matrix.batterycanary.TestUtils;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.util.Pair;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import static com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil.ONE_HOR;


@RunWith(AndroidJUnit4.class)
public class ConnectivityManagerHookerTest {
    static final String TAG = "Matrix.test.ConnectivityManagerHookerTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    @After
    public void shutDown() {
    }

    @Test
    public void testScanning() throws Exception {
        if (TestUtils.isAssembleTest()) return;

        final AtomicInteger scanInc = new AtomicInteger();
        final AtomicInteger getScanInc = new AtomicInteger();
        SystemServiceBinderHooker hooker = new SystemServiceBinderHooker(Context.CONNECTIVITY_SERVICE, "android.net.IConnectivityManager", new SystemServiceBinderHooker.HookCallback() {
            @Override
            public void onServiceMethodInvoke(Method method, Object[] args) {
                Assert.assertNotNull(method);
            }

            @Nullable
            @Override
            public Object onServiceMethodIntercept(Object receiver, Method method, Object[] args) throws Throwable {
                return null;
            }
        });

        hooker.doHook();

        ConnectivityManager manager = (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        Assert.assertNotNull(manager);

        List<List<Pair<Integer, String>>> pairs = new ArrayList<>();
        for (Network item : manager.getAllNetworks()) {
            NetworkInfo networkInfo = manager.getNetworkInfo(item);
            if (networkInfo != null && (networkInfo.isConnected() || networkInfo.isConnectedOrConnecting())) {
                pairs.add(Arrays.asList(
                        new Pair<>(networkInfo.getType(), String.valueOf(networkInfo.getTypeName())),
                        new Pair<>(networkInfo.getSubtype(), String.valueOf(networkInfo.getSubtypeName()))
                ));
                int txBwKBps = 0, rxBwKBps = 0;
                NetworkCapabilities capabilities = manager.getNetworkCapabilities(item);
                if (capabilities != null) {
                    txBwKBps = capabilities.getLinkUpstreamBandwidthKbps() / 8;
                    rxBwKBps = capabilities.getLinkDownstreamBandwidthKbps() / 8;
                }
            }
        }

        Assert.assertFalse(pairs.isEmpty());

        ConnectivityManager.NetworkCallback callback = new ConnectivityManager.NetworkCallback() {
            long mLastMs = 0;

            @Override
            public void onAvailable(@NonNull Network network) {
                super.onAvailable(network);
            }

            @Override
            public void onLosing(@NonNull Network network, int maxMsToLive) {
                super.onLosing(network, maxMsToLive);
            }

            @Override
            public void onLost(@NonNull Network network) {
                super.onLost(network);
            }

            @Override
            public void onUnavailable() {
                super.onUnavailable();
            }

            @SuppressLint({"WrongConstant", "RestrictedApi"})
            @Override
            public void onCapabilitiesChanged(@NonNull Network network, @NonNull NetworkCapabilities networkCapabilities) {
                super.onCapabilitiesChanged(network, networkCapabilities);
                try {
                    int strength = 0;
                    int[] capabilities = null;
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                        strength = networkCapabilities.getSignalStrength();
                    }
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                        capabilities = networkCapabilities.getCapabilities();
                    }

                    try {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                        }
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
                        }
                    } catch (Throwable ignored) {
                    }

                    // Dual-link check: both wifi & cellar transported networks
                    {
                        if (networkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR))
                            if (networkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) || networkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI_AWARE) || networkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET)) {
                            }
                    }
                } catch (Throwable e) {
                }
            }

            @Override
            public void onLinkPropertiesChanged(@NonNull Network network, @NonNull LinkProperties linkProperties) {
                super.onLinkPropertiesChanged(network, linkProperties);
            }

            @Override
            public void onBlockedStatusChanged(@NonNull Network network, boolean blocked) {
                super.onBlockedStatusChanged(network, blocked);
            }
        };

        manager.registerDefaultNetworkCallback(callback);
        hooker.doUnHook();
    }
}

