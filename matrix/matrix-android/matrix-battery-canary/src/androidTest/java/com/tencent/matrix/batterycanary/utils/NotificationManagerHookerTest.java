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

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.text.TextUtils;

import com.tencent.matrix.batterycanary.R;
import com.tencent.matrix.batterycanary.TestUtils;
import com.tencent.matrix.util.MatrixLog;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.lang.reflect.Method;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;
import androidx.test.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;


@RunWith(AndroidJUnit4.class)
public class NotificationManagerHookerTest {
    static final String TAG = "Matrix.test.NotificationManagerHookerTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
    }

    @After
    public void shutDown() {
    }

    @Test
    public void testGetSystemRes() {
        String text = getAppRunningNotificationText();
        Assert.assertFalse(TextUtils.isEmpty(text));
    }

    private static String getAppRunningNotificationText() {
        Resources resources = Resources.getSystem();
        Assert.assertNotNull(resources);

        // see
        // https://cs.android.com/android/platform/superproject/+/master:frameworks/base/services/core/java/com/android/server/am/ServiceRecord.java;l=892?q=app_running_notification_text
        int id = resources.getIdentifier("app_running_notification_text", "string", "android");
        Assert.assertTrue(id > 0);

        return resources.getString(id);
    }

    @Test
    public void testGettingNotificationContent() throws Exception {
        if (TestUtils.isAssembleTest()) {
            return;
        }

        final AtomicInteger createChannels = new AtomicInteger();
        final AtomicInteger notify = new AtomicInteger();
        final AtomicReference<NotificationChannel> channelRef = new AtomicReference<>();
        final AtomicReference<Notification> notificationRef = new AtomicReference<>();

        SystemServiceBinderHooker hooker = new SystemServiceBinderHooker(Context.NOTIFICATION_SERVICE, "android.app.INotificationManager", new SystemServiceBinderHooker.HookCallback() {
            @SuppressWarnings("rawtypes")
            @Override
            public void onServiceMethodInvoke(Method method, Object[] args) {
                Assert.assertNotNull(method);
                if ("createNotificationChannels".equals(method.getName())) {
                    Assert.assertNotNull(channelRef.get());
                    Assert.assertNull(notificationRef.get());

                    String packageName = null;
                    NotificationChannel notificationChannel = null;
                    if (args != null) {
                        for (Object arg : args) {
                            if (arg == null) {
                                continue;
                            }
                            if (arg instanceof String) {
                                packageName = (String) arg;
                            }
                            if (arg.getClass().getName().equals("android.content.pm.ParceledListSlice")) {
                                try {
                                    Method getListMethod = arg.getClass().getDeclaredMethod("getList");
                                    if (getListMethod != null) {
                                        Object list = getListMethod.invoke(arg);
                                        if (list instanceof Iterable) {
                                            for (Object item : (Iterable) list) {
                                                if (item instanceof NotificationChannel) {
                                                    notificationChannel = (NotificationChannel) item;
                                                }
                                            }
                                        }
                                    }
                                } catch (Exception e) {
                                    MatrixLog.w(TAG, "try parse args fail: " + e.getMessage());
                                }
                            }
                        }

                        Assert.assertFalse(TextUtils.isEmpty(packageName));
                        Assert.assertNotNull(notificationChannel);
                        Assert.assertSame(channelRef.get(), notificationChannel);
                    }

                    createChannels.incrementAndGet();

                } else if ("enqueueNotificationWithTag".equals(method.getName())) {
                    Assert.assertNotNull(channelRef.get());
                    Assert.assertNotNull(notificationRef.get());
                    Notification notification = null;
                    for (Object item : args) {
                        if (item instanceof Notification) {
                            notification = (Notification) item;
                            break;
                        }
                    }
                    Assert.assertNotNull(notification);
                    Assert.assertEquals("TEST_CHANNEL_ID", notification.getChannelId());
                    Assert.assertEquals("NOTIFICATION_TILE", notification.extras.get(Notification.EXTRA_TITLE));
                    Assert.assertEquals("NOTIFICATION_CONTENT", notification.extras.get(Notification.EXTRA_TEXT));

                    Assert.assertNotSame(notification, notificationRef.get());
                    Assert.assertNotEquals(notification.toString(), notificationRef.get().toString());
                    Assert.assertEquals(notification.getGroup(), notificationRef.get().getGroup());
                    Assert.assertEquals(notification.getChannelId(), notificationRef.get().getChannelId());
                    Assert.assertEquals(notification.extras.get(Notification.EXTRA_CHANNEL_GROUP_ID), notificationRef.get().extras.get(Notification.EXTRA_CHANNEL_GROUP_ID));
                    Assert.assertEquals(notification.extras.get(Notification.EXTRA_NOTIFICATION_ID), notificationRef.get().extras.get(Notification.EXTRA_NOTIFICATION_ID));
                    Assert.assertEquals(notification.extras.get(Notification.EXTRA_NOTIFICATION_TAG), notificationRef.get().extras.get(Notification.EXTRA_NOTIFICATION_TAG));
                    Assert.assertEquals(notification.extras.get(Notification.EXTRA_TITLE), notificationRef.get().extras.get(Notification.EXTRA_TITLE));
                    Assert.assertEquals(notification.extras.get(Notification.EXTRA_TEXT), notificationRef.get().extras.get(Notification.EXTRA_TEXT));

                    notify.incrementAndGet();
                }
            }

            @Nullable
            @Override
            public Object onServiceMethodIntercept(Object receiver, Method method, Object[] args) throws Throwable {
                return null;
            }
        });

        hooker.doHook();

        Assert.assertNull(channelRef.get());
        Assert.assertNull(notificationRef.get());
        Assert.assertEquals(0 ,createChannels.get());
        Assert.assertEquals(0 ,notify.get());

        NotificationManager notificationManager = (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE);

        String channelId = "TEST_CHANNEL_ID";
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
            NotificationManager manager = (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE);
            NotificationChannel channel = new NotificationChannel(channelId, "TEST_CHANNEL_NAME", NotificationManager.IMPORTANCE_DEFAULT);
            Assert.assertNotNull(channel);
            channelRef.set(channel);
            manager.createNotificationChannel(channel);

            Assert.assertNotNull(channelRef.get());
            Assert.assertNull(notificationRef.get());
            Assert.assertEquals(1 ,createChannels.get());
            Assert.assertEquals(0 ,notify.get());
        }

        Intent intent = new Intent(mContext, CanaryUtilsTest.SpyService.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        PendingIntent pendingIntent = PendingIntent.getActivity(mContext, 0, intent, 0);

        Notification notification = new NotificationCompat.Builder(mContext, channelId)
                .setContentTitle("NOTIFICATION_TILE")
                .setContentText("NOTIFICATION_CONTENT")
                .setContentIntent(pendingIntent)
                .setAutoCancel(true)
                .setSmallIcon(R.drawable.ic_launcher)
                .setWhen(System.currentTimeMillis())
                .build();

        Assert.assertNotNull(notification);
        notificationRef.set(notification);

        notificationManager.notify(16657, notification);

        Assert.assertNotNull(channelRef.get());
        Assert.assertNotNull(notificationRef.get());
        Assert.assertEquals(1 ,createChannels.get());
        Assert.assertEquals(1 ,notify.get());

        hooker.doUnHook();
    }


    @Test
    public void testNotificationCounting() throws Exception {
        final AtomicReference<NotificationChannel> channelRef = new AtomicReference<>();
        final AtomicReference<Notification> notificationRef = new AtomicReference<>();
        final AtomicReference<String> channelIdRef = new AtomicReference<>();
        final AtomicInteger notifyIdRef = new AtomicInteger();

        NotificationManagerServiceHooker.addListener(new NotificationManagerServiceHooker.IListener() {
            @Override
            public void onCreateNotificationChannel(@Nullable Object channel) {
                Assert.assertTrue(channel instanceof NotificationChannel);
                Assert.assertSame(channelRef.get(), channel);
                Assert.assertEquals("TEST_CHANNEL_ID", ((NotificationChannel) channel).getId());
                channelIdRef.set(((NotificationChannel) channel).getId());
            }

            @Override
            public void onCreateNotification(int id, @Nullable Notification notification) {
                Assert.assertEquals(16657, id);
                notifyIdRef.set(id);
            }
        });

        NotificationManager notificationManager = (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE);

        String channelId = "TEST_CHANNEL_ID";
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
            NotificationManager manager = (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE);
            NotificationChannel channel = new NotificationChannel(channelId, "TEST_CHANNEL_NAME", NotificationManager.IMPORTANCE_DEFAULT);
            Assert.assertNotNull(channel);
            channelRef.set(channel);
            manager.createNotificationChannel(channel);

            Assert.assertNotNull(channelRef.get());
            Assert.assertNull(notificationRef.get());
        }

        Intent intent = new Intent(mContext, CanaryUtilsTest.SpyService.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        PendingIntent pendingIntent = PendingIntent.getActivity(mContext, 0, intent, 0);

        Notification notification = new NotificationCompat.Builder(mContext, channelId)
                .setContentTitle("NOTIFICATION_TILE")
                .setContentText("NOTIFICATION_CONTENT")
                .setContentIntent(pendingIntent)
                .setAutoCancel(true)
                .setSmallIcon(R.drawable.ic_launcher)
                .setWhen(System.currentTimeMillis())
                .build();

        Assert.assertNotNull(notification);
        notificationRef.set(notification);

        int notifyId = 16657;
        notificationManager.notify(notifyId, notification);

        Assert.assertEquals(channelId, channelIdRef.get());
        Assert.assertEquals(notifyId, notifyIdRef.get());

        NotificationManagerServiceHooker.release();
    }
}
