package com.tencent.matrix.batterycanary.monitor.feature;

import android.app.Notification;
import android.app.NotificationChannel;
import android.content.res.Resources;
import android.os.Build;
import android.os.SystemClock;
import android.text.TextUtils;

import com.tencent.matrix.batterycanary.utils.NotificationManagerServiceHooker;
import com.tencent.matrix.batterycanary.utils.NotificationManagerServiceHooker.IListener;
import com.tencent.matrix.util.MatrixLog;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

public final class NotificationMonitorFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.NotificationMonitorFeature";

    public interface NotificationListener {
        void onNotify(@NonNull BadNotification notify);
    }

    @Nullable
    IListener mListener;
    @Nullable
    String mAppRunningNotifyText;
    long mLastBgStartMillis = -1;

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    public void onTurnOn() {
        super.onTurnOn();

        mAppRunningNotifyText = tryGetAppRunningNotificationText();

        if (TextUtils.isEmpty(mAppRunningNotifyText)) {
            MatrixLog.w(TAG, "can not get app_running_notification_text, abort notification monitor");

        } else {
            MatrixLog.i(TAG, "get app_running_notification_text: " + mAppRunningNotifyText);
            mListener = new IListener() {
                @Override
                public void onCreateNotificationChannel(@Nullable Object notificationChannel) {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                        if (notificationChannel instanceof NotificationChannel) {
                            MatrixLog.i(TAG, "#onCreateNotificationChannel, id = " + ((NotificationChannel) notificationChannel).getId()
                                    + ", name = " + ((NotificationChannel) notificationChannel).getName());
                        }
                    }
                }

                @Override
                public void onCreateNotification(int id, @Nullable Notification notification) {
                    if (notification != null) {
                        String title = notification.extras.getString(Notification.EXTRA_TITLE, null);
                        String text = notification.extras.getString(Notification.EXTRA_TEXT, null);
                        MatrixLog.i(TAG, "#onCreateNotification, id = " + id + ", title = " + title + ", text = " + text);

                        if (TextUtils.isEmpty(text)) {
                            MatrixLog.w(TAG, "notify with empty text!");
                            checkNotifyContent(title, "");
                        } else {
                            if (text.equals(mAppRunningNotifyText)) {
                                MatrixLog.w(TAG, "notify with appRunningText: " + text);
                                checkNotifyContent(title, text);
                            }
                        }
                    }
                }
            };

            NotificationManagerServiceHooker.addListener(mListener);
        }
    }

    private String tryGetAppRunningNotificationText() {
        Resources resources = Resources.getSystem();
        if (resources != null) {
            int appRunningNotifyTextId = resources.getIdentifier(
                    "app_running_notification_text",
                    "string",
                    "android"
            );
            if (appRunningNotifyTextId > 0) {
                return resources.getString(appRunningNotifyTextId);
            }
        }
        return null;
    }

    @Override
    public void onForeground(boolean isForeground) {
        super.onForeground(isForeground);
        mLastBgStartMillis = isForeground ? -1 : SystemClock.uptimeMillis();
    }

    @Override
    public void onTurnOff() {
        super.onTurnOff();
        NotificationManagerServiceHooker.removeListener(mListener);
    }

    @Override
    public int weight() {
        return Integer.MIN_VALUE;
    }

    @VisibleForTesting
    void checkNotifyContent(@Nullable final String title, @Nullable final String text) {
        final long bgMillis = mLastBgStartMillis > 0 ? SystemClock.uptimeMillis() - mLastBgStartMillis : 0;
        final String stack = mCore.getConfig().isAggressiveMode ? mCore.getConfig().callStackCollector.collectCurr() : "";

        mCore.getHandler().post(new Runnable() {
            @Override
            public void run() {
                BadNotification badNotification = new BadNotification();
                badNotification.title = title;
                badNotification.content = text;
                badNotification.stack = stack;
                badNotification.bgMillis = bgMillis;
                mCore.onNotify(badNotification);
            }
        });
    }


    public static final class BadNotification {
        @Nullable
        public String title;
        @Nullable
        public String content;
        @Nullable
        public String stack;
        public long bgMillis;

        @Override
        public String toString() {
            return "BadNotification{" +
                    "title='" + title + '\'' +
                    ", content='" + content + '\'' +
                    ", stack='" + stack + '\'' +
                    ", bgMillis=" + bgMillis +
                    '}';
        }
    }
}
