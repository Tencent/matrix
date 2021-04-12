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
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;
import android.os.Looper;
import androidx.annotation.Nullable;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.core.content.ContextCompat;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;


@RunWith(AndroidJUnit4.class)
public class LocationManagerHookerTest {
    static final String TAG = "Matrix.test.LocationManagerHookerTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
    }

    @After
    public void shutDown() {
    }

    @Test
    public void testRequestGpsLocationUpdate() throws Exception {
        final long MIN_DISTANCE_CHANGE_FOR_UPDATES = 10; // 10 meters
        final long MIN_TIME_BW_UPDATES = 1000 * 60;      // 1 minute
        final AtomicInteger scanInc = new AtomicInteger();

        SystemServiceBinderHooker hooker = new SystemServiceBinderHooker(Context.LOCATION_SERVICE, "android.location.ILocationManager", new SystemServiceBinderHooker.HookCallback() {
            @Override
            public void onServiceMethodInvoke(Method method, Object[] args) {
                Assert.assertNotNull(method);
                if ("requestLocationUpdates".equals(method.getName())) {
                    for (Object item : args) {
                        if (item != null && "android.location.LocationRequest".equals(item.getClass().getName())) {
                            long minTimeMillis;
                            float minDistance;
                            try {
                                Field mFastestInterval = item.getClass().getDeclaredField("mFastestInterval");
                                mFastestInterval.setAccessible(true);
                                minTimeMillis = mFastestInterval.getLong(item);
                                Field mSmallestDisplacement = item.getClass().getDeclaredField("mSmallestDisplacement");
                                mSmallestDisplacement.setAccessible(true);
                                minDistance = mSmallestDisplacement.getFloat(item);
                                scanInc.incrementAndGet();
                            } catch (NoSuchFieldException | IllegalAccessException e) {
                                Assert.fail(e.getMessage());
                            }
                        }
                    }
                }
            }

            @Nullable
            @Override
            public Object onServiceMethodIntercept(Object receiver, Method method, Object[] args) throws Throwable {
                return null;
            }
        });

        hooker.doHook();
        Assert.assertEquals(0, scanInc.get());

        LocationManager locationManager = (LocationManager) mContext.getSystemService(Context.LOCATION_SERVICE);
        if (locationManager.isLocationEnabled()) {
            if (locationManager.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
                if (ContextCompat.checkSelfPermission(mContext, "android.permission.ACCESS_FINE_LOCATION") == 0) {
                    locationManager.requestLocationUpdates(LocationManager.GPS_PROVIDER, MIN_TIME_BW_UPDATES, MIN_DISTANCE_CHANGE_FOR_UPDATES, new LocationListener() {
                        @Override
                        public void onLocationChanged(Location location) {

                        }

                        @Override
                        public void onStatusChanged(String provider, int status, Bundle extras) {

                        }

                        @Override
                        public void onProviderEnabled(String provider) {

                        }

                        @Override
                        public void onProviderDisabled(String provider) {

                        }
                    }, Looper.getMainLooper());
                    Assert.assertEquals(1, scanInc.get());
                }
            }
        }

        hooker.doUnHook();
    }

    @Test
    public void testRequestNetworkLocationUpdate() throws Exception {
        final long MIN_DISTANCE_CHANGE_FOR_UPDATES = 10; // 10 meters
        final long MIN_TIME_BW_UPDATES = 1000 * 60;      // 1 minute
        final AtomicInteger scanInc = new AtomicInteger();

        SystemServiceBinderHooker hooker = new SystemServiceBinderHooker(Context.LOCATION_SERVICE, "android.location.ILocationManager", new SystemServiceBinderHooker.HookCallback() {
            @Override
            public void onServiceMethodInvoke(Method method, Object[] args) {
                Assert.assertNotNull(method);
                if ("requestLocationUpdates".equals(method.getName())) {
                    scanInc.incrementAndGet();
                }
            }

            @Nullable
            @Override
            public Object onServiceMethodIntercept(Object receiver, Method method, Object[] args) throws Throwable {
                return null;
            }
        });

        hooker.doHook();
        Assert.assertEquals(0, scanInc.get());

        LocationManager locationManager = (LocationManager) mContext.getSystemService(Context.LOCATION_SERVICE);
        if (locationManager.isLocationEnabled()) {
            if (locationManager.isProviderEnabled(LocationManager.NETWORK_PROVIDER)) {
                if (ContextCompat.checkSelfPermission(mContext, "android.permission.ACCESS_FINE_LOCATION") == 1) {
                    locationManager.requestLocationUpdates(LocationManager.NETWORK_PROVIDER, MIN_TIME_BW_UPDATES, MIN_DISTANCE_CHANGE_FOR_UPDATES, new LocationListener() {
                        @Override
                        public void onLocationChanged(Location location) {

                        }

                        @Override
                        public void onStatusChanged(String provider, int status, Bundle extras) {

                        }

                        @Override
                        public void onProviderEnabled(String provider) {

                        }

                        @Override
                        public void onProviderDisabled(String provider) {

                        }
                    }, Looper.getMainLooper());
                    Assert.assertEquals(1, scanInc.get());
                }
            }
        }

        hooker.doUnHook();
    }

    @Test
    public void testLocationCounting() {
        final AtomicLong minTimeRef = new AtomicLong();
        final AtomicReference<Float> minDistanceRef = new AtomicReference<>();

        LocationManagerServiceHooker.addListener(new LocationManagerServiceHooker.IListener() {
            @Override
            public void onRequestLocationUpdates(long minTimeMillis, float minDistance) {
                minTimeRef.set(minTimeMillis);
                minDistanceRef.set(minDistance);
            }
        });

        for (int i = 0; i < 100; i++) {
            LocationManager locationManager = (LocationManager) mContext.getSystemService(Context.LOCATION_SERVICE);
            if (locationManager.isLocationEnabled()) {
                if (locationManager.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
                    if (ContextCompat.checkSelfPermission(mContext, "android.permission.ACCESS_FINE_LOCATION") == 1) {
                        locationManager.requestLocationUpdates(LocationManager.GPS_PROVIDER, i, i + 1, new LocationListener() {
                            @Override
                            public void onLocationChanged(Location location) {

                            }

                            @Override
                            public void onStatusChanged(String provider, int status, Bundle extras) {

                            }

                            @Override
                            public void onProviderEnabled(String provider) {

                            }

                            @Override
                            public void onProviderDisabled(String provider) {

                            }
                        }, Looper.getMainLooper());

                        Assert.assertEquals(i, minTimeRef.get());
                        Assert.assertEquals(i + 1, minDistanceRef.get(), 0.1F);
                    }
                }
            }
        }
        LocationManagerServiceHooker.release();
    }
}

