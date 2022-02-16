package com.tencent.matrix.resource.processor;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Parcel;
import android.os.Parcelable;

import com.tencent.matrix.resource.MemoryUtil;
import com.tencent.matrix.resource.R;
import com.tencent.matrix.resource.analyzer.model.ActivityLeakResult;
import com.tencent.matrix.resource.analyzer.model.DestroyedActivityInfo;
import com.tencent.matrix.resource.config.ResourceConfig;
import com.tencent.matrix.resource.config.SharePluginInfo;
import com.tencent.matrix.resource.watcher.ActivityRefWatcher;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

import static android.os.Build.VERSION.SDK_INT;

import androidx.annotation.Nullable;

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

        dumpAndAnalyzeAsync(destroyedActivityInfo.mActivityName, destroyedActivityInfo.mKey, new ManualDumpCallback() {
            @Override
            public void onDumpComplete(@Nullable ManualDumpData data) {
                if (data != null) {
                    if (!isMuted) {
                        MatrixLog.i(TAG, "shown notification!!!3");
                        sendResultNotification(destroyedActivityInfo, data);
                    } else {
                        MatrixLog.i(TAG, "mute mode, notification will not be shown.");
                    }
                }
            }
        });

        return true;
    }

    private void sendResultNotification(DestroyedActivityInfo activityInfo, ManualDumpData data) {
        final Context context = getWatcher().getContext();

        Intent targetIntent = new Intent();
        targetIntent.setClassName(getWatcher().getContext(), mTargetActivity);
        targetIntent.putExtra(SharePluginInfo.ISSUE_ACTIVITY_NAME, activityInfo.mActivityName);
        targetIntent.putExtra(SharePluginInfo.ISSUE_REF_KEY, activityInfo.mKey);
        targetIntent.putExtra(SharePluginInfo.ISSUE_LEAK_PROCESS, MatrixUtil.getProcessName(context));
        targetIntent.putExtra(SharePluginInfo.ISSUE_DUMP_DATA, data);

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

    private void dumpAndAnalyzeAsync(final String activity, final String refString, final ManualDumpCallback callback) {
        MatrixHandlerThread.getDefaultHandler().postAtFrontOfQueue(new Runnable() {
            @Override
            public void run() {
                callback.onDumpComplete(dumpAndAnalyse(activity, refString));
            }
        });
    }

    private interface ManualDumpCallback {
        void onDumpComplete(@Nullable ManualDumpData data);
    }

    /**
     * run in leaked process
     *
     * @param activity
     * @param key
     * @return
     */
    private ManualDumpData dumpAndAnalyse(String activity, String key) {

        getWatcher().triggerGc();

        File file = getDumpStorageManager().newHprofFile();
        final ActivityLeakResult result = MemoryUtil.dumpAndAnalyze(file.getAbsolutePath(), key);
        if (result.mLeakFound) {
            final String leakChain = result.toString();
            publishIssue(
                    SharePluginInfo.IssueType.LEAK_FOUND,
                    ResourceConfig.DumpMode.FORK_ANALYSE,
                    activity, key, leakChain, String.valueOf(result.mAnalysisDurationMs)
            );
            return new ManualDumpData(file.getAbsolutePath(), leakChain);
        } else if (result.mFailure != null) {
            publishIssue(
                    SharePluginInfo.IssueType.ERR_EXCEPTION,
                    ResourceConfig.DumpMode.FORK_ANALYSE,
                    activity, key, result.mFailure.toString(), "0"
            );
            return null;
        } else {
            return new ManualDumpData(file.getAbsolutePath(), null);
        }
    }

    public static class ManualDumpData implements Parcelable {
        public final String hprofPath;
        public final String refChain;

        public ManualDumpData(String hprofPath, String refChain) {
            this.hprofPath = hprofPath;
            this.refChain = refChain;
        }

        protected ManualDumpData(Parcel in) {
            hprofPath = in.readString();
            refChain = in.readString();
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeString(hprofPath);
            dest.writeString(refChain);
        }

        @Override
        public int describeContents() {
            return 0;
        }

        public static final Creator<ManualDumpData> CREATOR = new Creator<ManualDumpData>() {
            @Override
            public ManualDumpData createFromParcel(Parcel in) {
                return new ManualDumpData(in);
            }

            @Override
            public ManualDumpData[] newArray(int size) {
                return new ManualDumpData[size];
            }
        };
    }
}
