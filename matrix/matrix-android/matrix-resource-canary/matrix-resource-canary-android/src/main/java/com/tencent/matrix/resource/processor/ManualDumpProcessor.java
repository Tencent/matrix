package com.tencent.matrix.resource.processor;

import static android.os.Build.VERSION.SDK_INT;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.util.Pair;

import com.tencent.matrix.resource.MemoryUtil;
import com.tencent.matrix.resource.R;
import com.tencent.matrix.resource.analyzer.model.ActivityLeakResult;
import com.tencent.matrix.resource.analyzer.model.DestroyedActivityInfo;
import com.tencent.matrix.resource.config.ResourceConfig;
import com.tencent.matrix.resource.config.SharePluginInfo;
import com.tencent.matrix.resource.dumper.HprofFileManager;
import com.tencent.matrix.resource.watcher.ActivityRefWatcher;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

/**
 * X process leaked -> send notification -> main process activity -> dump and analyse in X process -> show result in main process activity
 * Created by Yves on 2021/3/4
 */
public class ManualDumpProcessor extends BaseLeakProcessor {

    private static final String TAG = "Matrix.LeakProcessor.ManualDump";

    private static final int NOTIFICATION_ID = 0x110;

    private final String mTargetActivity;

    private final NotificationManager mNotificationManager;

    private final List<DestroyedActivityInfo> mLeakedActivities = new ArrayList<>();

    private boolean isMuted;

    public ManualDumpProcessor(ActivityRefWatcher watcher, String targetActivity) {
        super(watcher);
        mTargetActivity = targetActivity;
        mNotificationManager = (NotificationManager) watcher.getContext().getSystemService(Context.NOTIFICATION_SERVICE);
    }

    @Override
    public boolean process(final DestroyedActivityInfo destroyedActivityInfo) {
        getWatcher().triggerGc();

        if (destroyedActivityInfo.mActivityRef.get() == null) {
            MatrixLog.v(TAG, "activity with key [%s] was already recycled.", destroyedActivityInfo.mKey);
            return true;
        }

        mLeakedActivities.add(destroyedActivityInfo);

        MatrixLog.i(TAG, "show notification for activity leak. %s", destroyedActivityInfo.mActivityName);

        if (isMuted) {
            MatrixLog.i(TAG, "is muted, won't show notification util process reboot");
            return true;
        }

        Pair<String, String> data = dumpAndAnalyse(destroyedActivityInfo.mActivityName, destroyedActivityInfo.mKey);
        if (data != null) {
            if (!isMuted) {
                MatrixLog.i(TAG, "shown notification!!!3");
                sendResultNotification(destroyedActivityInfo, data.first, data.second);
            } else {
                MatrixLog.i(TAG, "mute mode, notification will not be shown.");
            }
        }


        return true;
    }

    private void sendResultNotification(DestroyedActivityInfo activityInfo, String hprofPath, String refChain) {
        final Context context = getWatcher().getContext();

        Intent targetIntent = new Intent();
        targetIntent.setClassName(getWatcher().getContext(), mTargetActivity);
        targetIntent.putExtra(SharePluginInfo.ISSUE_ACTIVITY_NAME, activityInfo.mActivityName);
        targetIntent.putExtra(SharePluginInfo.ISSUE_REF_KEY, activityInfo.mKey);
        targetIntent.putExtra(SharePluginInfo.ISSUE_LEAK_PROCESS, MatrixUtil.getProcessName(context));
        targetIntent.putExtra(SharePluginInfo.ISSUE_HPROF_PATH, hprofPath);
        targetIntent.putExtra(SharePluginInfo.ISSUE_LEAK_DETAIL, refChain);

        PendingIntent pIntent = PendingIntent.getActivity(context, 0, targetIntent, PendingIntent.FLAG_UPDATE_CURRENT);

        String dumpingHeapTitle = context.getString(R.string.resource_canary_leak_tip);
        ResourceConfig config = getWatcher().getResourcePlugin().getConfig();
        String dumpingHeapContent =
                String.format(Locale.getDefault(), "[%s] has leaked for [%s]min!!!",
                        activityInfo.mActivityName,
                        TimeUnit.MILLISECONDS.toMinutes(
                                config.getScanIntervalMillis() * config.getMaxRedetectTimes()));

        Notification.Builder builder;
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
            builder = new Notification.Builder(context, getNotificationChannelIdCompat(context));
        } else {
            builder = new Notification.Builder(context);
        }

        builder.setContentTitle(dumpingHeapTitle)
                .setPriority(Notification.PRIORITY_DEFAULT)
                .setStyle(new Notification.BigTextStyle().bigText(dumpingHeapContent))
                .setAutoCancel(true)
                .setContentIntent(pIntent)
                .setSmallIcon(R.drawable.ic_launcher)
                .setWhen(System.currentTimeMillis());

        Notification notification = builder.build();

        mNotificationManager.notify(NOTIFICATION_ID + activityInfo.mKey.hashCode(), notification);
    }

    private String getNotificationChannelIdCompat(Context context) {
        if (SDK_INT >= Build.VERSION_CODES.O) {
            String channelName = "com.tencent.matrix.resource.processor.ManualDumpProcessor";
            NotificationManager notificationManager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
            NotificationChannel notificationChannel = notificationManager.getNotificationChannel(channelName);
            if (notificationChannel == null) {
                MatrixLog.v(TAG, "create channel");
                notificationChannel = new NotificationChannel(channelName, channelName, NotificationManager.IMPORTANCE_HIGH);
                notificationManager.createNotificationChannel(notificationChannel);
            }
            return channelName;
        }
        return null;
    }

    public void setMuted(boolean mute) {
        isMuted = mute;
    }

    /**
     * run in leaked process
     *
     * @param activity
     * @param key
     * @return
     */
    private Pair<String, String> dumpAndAnalyse(String activity, String key) {

        getWatcher().triggerGc();

        File file = null;
        try {
            file = HprofFileManager.INSTANCE.prepareHprofFile("MDP", false);
        } catch (FileNotFoundException e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
        }

        if (file == null) {
            MatrixLog.e(TAG, "prepare hprof file failed, see log above");
            return null;
        }

        final ActivityLeakResult result = MemoryUtil.dumpAndAnalyze(file.getAbsolutePath(), key, 600);
        if (result.mLeakFound) {
            final String leakChain = result.toString();
            publishIssue(
                    SharePluginInfo.IssueType.LEAK_FOUND,
                    ResourceConfig.DumpMode.MANUAL_DUMP,
                    activity, key, leakChain, String.valueOf(result.mAnalysisDurationMs),
                    0,
                    file.getAbsolutePath()
            );
            return new Pair<>(file.getAbsolutePath(), leakChain);
        } else if (result.mFailure != null) {
            publishIssue(
                    SharePluginInfo.IssueType.ERR_EXCEPTION,
                    ResourceConfig.DumpMode.MANUAL_DUMP,
                    activity, key, result.mFailure.toString(), "0"
            );
            return null;
        } else {
            return new Pair<>(file.getAbsolutePath(), null);
        }
    }
}
