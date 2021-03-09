package com.tencent.matrix.resource.processor;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.support.v4.app.NotificationCompat;

import com.tencent.matrix.resource.R;
import com.tencent.matrix.resource.analyzer.model.DestroyedActivityInfo;
import com.tencent.matrix.resource.config.SharePluginInfo;
import com.tencent.matrix.resource.watcher.ActivityRefWatcher;
import com.tencent.matrix.util.MatrixLog;

import java.util.HashMap;
import java.util.Map;

import static android.app.NotificationManager.IMPORTANCE_DEFAULT;
import static android.os.Build.VERSION.SDK_INT;

/**
 * Created by Yves on 2021/3/4
 */
public class ManualDumpProcessor extends BaseLeakProcessor{

    private static final String TAG = "Matrix.LeakProcessor.ManualDump";

    private static final int NOTIFICATION_ID = 0x110;

    private final Intent mContentIntent;

//    private final Map<String, Integer> mLeakedActivities = new HashMap<>();

    public ManualDumpProcessor(ActivityRefWatcher watcher, Intent contentIntent) {
        super(watcher);
        mContentIntent = contentIntent;
    }

    @Override
    public boolean process(DestroyedActivityInfo destroyedActivityInfo) {
        final Context context = getWatcher().getContext();

        NotificationManager notificationManager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        String dumpingHeapContent = context.getString(R.string.resource_canary_leak_tip);
        String dumpingHeapTitle = destroyedActivityInfo.mActivityName;

        mContentIntent.putExtra(SharePluginInfo.ISSUE_ACTIVITY_NAME, destroyedActivityInfo.mActivityName);
        mContentIntent.putExtra(SharePluginInfo.ISSUE_REF_KEY, destroyedActivityInfo.mKey);
        PendingIntent pIntent = PendingIntent.getActivity(context, 0, mContentIntent,
                PendingIntent.FLAG_UPDATE_CURRENT);
        NotificationCompat.Builder builder = new NotificationCompat.Builder(context)
                .setContentTitle(dumpingHeapTitle)
                .setContentIntent(pIntent)
                .setContentText(dumpingHeapContent);
        Notification notification = buildNotification(context, builder);
        notificationManager.notify(NOTIFICATION_ID, notification);

        getWatcher().markPublished(destroyedActivityInfo.mActivityName);
        MatrixLog.i(TAG, "show notification for activity leak. %s", destroyedActivityInfo.mActivityName);
        return true;
    }

    private Notification buildNotification(Context context, NotificationCompat.Builder builder) {
        builder.setSmallIcon(R.drawable.ic_launcher)
                .setWhen(System.currentTimeMillis());
        if (SDK_INT >= Build.VERSION_CODES.O) {
            String channelName = context.getString(R.string.app_name);
            NotificationManager notificationManager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
            NotificationChannel notificationChannel = notificationManager.getNotificationChannel(channelName);
            if (notificationChannel == null) {
                notificationChannel = new NotificationChannel(channelName, channelName, IMPORTANCE_DEFAULT);
                notificationManager.createNotificationChannel(notificationChannel);
            }
            builder.setChannelId(channelName);
        }

        return builder.build();
    }
}
