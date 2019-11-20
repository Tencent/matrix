/*
 * Copyright (C) 2015 Square, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.tencent.matrix.resource.watcher;

import android.app.Activity;
import android.app.Application;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Debug;
import android.os.HandlerThread;
import android.support.v4.app.NotificationCompat;

import com.tencent.matrix.AppActiveMatrixDelegate;
import com.tencent.matrix.listeners.IAppForeground;
import com.tencent.matrix.report.FilePublisher;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.resource.CanaryWorkerService;
import com.tencent.matrix.resource.R;
import com.tencent.matrix.resource.ResourcePlugin;
import com.tencent.matrix.resource.analyzer.model.DestroyedActivityInfo;
import com.tencent.matrix.resource.analyzer.model.HeapDump;
import com.tencent.matrix.resource.config.ResourceConfig;
import com.tencent.matrix.resource.config.SharePluginInfo;
import com.tencent.matrix.resource.watcher.RetryableTaskExecutor.RetryableTask;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.lang.ref.WeakReference;
import java.util.Iterator;
import java.util.UUID;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicLong;

import static android.app.NotificationManager.IMPORTANCE_DEFAULT;
import static android.os.Build.VERSION.SDK_INT;

/**
 * Created by tangyinsheng on 2017/6/2.
 * <p>
 * This class is ported from LeakCanary.
 */

public class ActivityRefWatcher extends FilePublisher implements Watcher, IAppForeground {
    private static final String TAG = "Matrix.ActivityRefWatcher";

    private static final int  CREATED_ACTIVITY_COUNT_THRESHOLD = 2;
    private static final long FILE_CONFIG_EXPIRED_TIME         = 24 * 60 * 60 * 1000;

    private static final String ACTIVITY_REFKEY_PREFIX = "MATRIX_RESCANARY_REFKEY_";

    private final ResourcePlugin mResourcePlugin;

    private final RetryableTaskExecutor             mDetectExecutor;
    private final int                               mMaxRedetectTimes;
    private final long                              mBgScanTimes;
    private final long                              mFgScanTimes;
    private final DumpStorageManager                mDumpStorageManager;
    private final AndroidHeapDumper                 mHeapDumper;
    private final AndroidHeapDumper.HeapDumpHandler mHeapDumpHandler;
    private final ResourceConfig.DumpMode mDumpHprofMode;

    private final ConcurrentLinkedQueue<DestroyedActivityInfo> mDestroyedActivityInfos;
    private final AtomicLong                                   mCurrentCreatedActivityCount;
    private IActivityLeakCallback activityLeakCallback = null;
    private Intent mContentIntent;
    private final static int NOTIFICATION_ID = 0x110;

    public void setActivityLeakCallback(IActivityLeakCallback activityLeakCallback) {
        this.activityLeakCallback = activityLeakCallback;
    }

    public interface IActivityLeakCallback {
        boolean onLeak(String activity, String ref);
    }

    public static class ComponentFactory {

        protected RetryableTaskExecutor createDetectExecutor(ResourceConfig config, HandlerThread handlerThread) {
            return new RetryableTaskExecutor(config.getScanIntervalMillis(), handlerThread);
        }

        protected DumpStorageManager createDumpStorageManager(Context context) {
            return new DumpStorageManager(context);
        }

        protected AndroidHeapDumper createHeapDumper(Context context, DumpStorageManager dumpStorageManager) {
            return new AndroidHeapDumper(context, dumpStorageManager);
        }

        protected AndroidHeapDumper.HeapDumpHandler createHeapDumpHandler(final Context context, ResourceConfig resourceConfig) {
            return new AndroidHeapDumper.HeapDumpHandler() {
                @Override
                public void process(HeapDump result) {
                    CanaryWorkerService.shrinkHprofAndReport(context, result);
                }
            };
        }
    }

    public ActivityRefWatcher(Application app,
                              final ResourcePlugin resourcePlugin) {
        this(app, resourcePlugin, new ComponentFactory());
    }

    private ActivityRefWatcher(Application app,
                               ResourcePlugin resourcePlugin,
                               ComponentFactory componentFactory) {
        super(app, FILE_CONFIG_EXPIRED_TIME, resourcePlugin.getTag(), resourcePlugin);
        this.mResourcePlugin = resourcePlugin;
        final ResourceConfig config = resourcePlugin.getConfig();
        final Context context = app;
        HandlerThread handlerThread = MatrixHandlerThread.getDefaultHandlerThread();
        mDumpHprofMode = config.getDumpHprofMode();
        mBgScanTimes = config.getBgScanIntervalMillis();
        mFgScanTimes = config.getScanIntervalMillis();
        mContentIntent = config.getNotificationContentIntent();
        mDetectExecutor = componentFactory.createDetectExecutor(config, handlerThread);
        mMaxRedetectTimes = config.getMaxRedetectTimes();
        mDumpStorageManager = componentFactory.createDumpStorageManager(context);
        mHeapDumper = componentFactory.createHeapDumper(context, mDumpStorageManager);
        mHeapDumpHandler = componentFactory.createHeapDumpHandler(context, config);
        mDestroyedActivityInfos = new ConcurrentLinkedQueue<>();
        mCurrentCreatedActivityCount = new AtomicLong(0);
    }

    @Override
    public void onForeground(boolean isForeground) {
        if (isForeground) {
            MatrixLog.i(TAG, "we are in foreground, modify scan time[%sms].", mFgScanTimes);
            mDetectExecutor.clearTasks();
            mDetectExecutor.setDelayMillis(mFgScanTimes);
            mDetectExecutor.executeInBackground(mScanDestroyedActivitiesTask);
        } else {
            MatrixLog.i(TAG, "we are in background, modify scan time[%sms].", mBgScanTimes);
            mDetectExecutor.setDelayMillis(mBgScanTimes);
        }
    }

    private final Application.ActivityLifecycleCallbacks mRemovedActivityMonitor = new ActivityLifeCycleCallbacksAdapter() {
        @Override
        public void onActivityDestroyed(Activity activity) {
            pushDestroyedActivityInfo(activity);
       /*     synchronized (mDestroyedActivityInfos) {
                mDestroyedActivityInfos.notifyAll();
            }*/
        }
    };

    @Override
    public void start() {
        stopDetect();
        final Application app = mResourcePlugin.getApplication();
        if (app != null) {
            app.registerActivityLifecycleCallbacks(mRemovedActivityMonitor);
            AppActiveMatrixDelegate.INSTANCE.addListener(this);
            scheduleDetectProcedure();
            MatrixLog.i(TAG, "watcher is started.");
        }
    }

    @Override
    public void stop() {
        stopDetect();
        MatrixLog.i(TAG, "watcher is stopped.");
    }

    private void stopDetect() {
        final Application app = mResourcePlugin.getApplication();
        if (app != null) {
            app.unregisterActivityLifecycleCallbacks(mRemovedActivityMonitor);
            AppActiveMatrixDelegate.INSTANCE.removeListener(this);
            unscheduleDetectProcedure();
        }
    }

    @Override
    public void destroy() {
        mDetectExecutor.quit();
        MatrixLog.i(TAG, "watcher is destroyed.");
    }

    public AndroidHeapDumper getHeapDumper() {
        return mHeapDumper;
    }

    private void pushDestroyedActivityInfo(Activity activity) {
        final String activityName = activity.getClass().getName();
        if (!mResourcePlugin.getConfig().getDetectDebugger() && isPublished(activityName)) {
            MatrixLog.i(TAG, "activity leak with name %s had published, just ignore", activityName);
            return;
        }
        final UUID uuid = UUID.randomUUID();
        final StringBuilder keyBuilder = new StringBuilder();
        keyBuilder.append(ACTIVITY_REFKEY_PREFIX).append(activityName)
            .append('_').append(Long.toHexString(uuid.getMostSignificantBits())).append(Long.toHexString(uuid.getLeastSignificantBits()));
        final String key = keyBuilder.toString();
        final DestroyedActivityInfo destroyedActivityInfo
            = new DestroyedActivityInfo(key, activity, activityName, mCurrentCreatedActivityCount.get());
        mDestroyedActivityInfos.add(destroyedActivityInfo);
    }

    private void scheduleDetectProcedure() {
        mDetectExecutor.executeInBackground(mScanDestroyedActivitiesTask);
    }

    private void unscheduleDetectProcedure() {
        mDetectExecutor.clearTasks();
        mDestroyedActivityInfos.clear();
        mCurrentCreatedActivityCount.set(0);
    }

    private final RetryableTask mScanDestroyedActivitiesTask = new RetryableTask() {

        @Override
        public Status execute() {
            // If destroyed activity list is empty, just wait to save power.
            if (mDestroyedActivityInfos.isEmpty()) {
                MatrixLog.i(TAG, "DestroyedActivityInfo isEmpty!");
                return Status.RETRY;
            }

            // Fake leaks will be generated when debugger is attached.
            if (Debug.isDebuggerConnected() && !mResourcePlugin.getConfig().getDetectDebugger()) {
                MatrixLog.w(TAG, "debugger is connected, to avoid fake result, detection was delayed.");
                return Status.RETRY;
            }

            final WeakReference<Object> sentinelRef = new WeakReference<>(new Object());
            triggerGc();
            if (sentinelRef.get() != null) {
                // System ignored our gc request, we will retry later.
                MatrixLog.d(TAG, "system ignore our gc request, wait for next detection.");
                return Status.RETRY;
            }

            final Iterator<DestroyedActivityInfo> infoIt = mDestroyedActivityInfos.iterator();

            while (infoIt.hasNext()) {
                final DestroyedActivityInfo destroyedActivityInfo = infoIt.next();
                if (!mResourcePlugin.getConfig().getDetectDebugger() && isPublished(destroyedActivityInfo.mActivityName) && mDumpHprofMode != ResourceConfig.DumpMode.SILENCE_DUMP) {
                    MatrixLog.v(TAG, "activity with key [%s] was already published.", destroyedActivityInfo.mActivityName);
                    infoIt.remove();
                    continue;
                }
                if (destroyedActivityInfo.mActivityRef.get() == null) {
                    // The activity was recycled by a gc triggered outside.
                    MatrixLog.v(TAG, "activity with key [%s] was already recycled.", destroyedActivityInfo.mKey);
                    infoIt.remove();
                    continue;
                }

                ++destroyedActivityInfo.mDetectedCount;

                long createdActivityCountFromDestroy = mCurrentCreatedActivityCount.get() - destroyedActivityInfo.mLastCreatedActivityCount;
                if (destroyedActivityInfo.mDetectedCount < mMaxRedetectTimes
                    || (createdActivityCountFromDestroy < CREATED_ACTIVITY_COUNT_THRESHOLD && !mResourcePlugin.getConfig().getDetectDebugger())) {
                    // Although the sentinel tell us the activity should have been recycled,
                    // system may still ignore it, so try again until we reach max retry times.
                    MatrixLog.i(TAG, "activity with key [%s] should be recycled but actually still \n"
                            + "exists in %s times detection with %s created activities during destroy, wait for next detection to confirm.",
                        destroyedActivityInfo.mKey, destroyedActivityInfo.mDetectedCount, createdActivityCountFromDestroy);
                    continue;
                }

                MatrixLog.i(TAG, "activity with key [%s] was suspected to be a leaked instance. mode[%s]", destroyedActivityInfo.mKey, mDumpHprofMode);

                if (mDumpHprofMode == ResourceConfig.DumpMode.SILENCE_DUMP) {
                    if (mResourcePlugin != null && !isPublished(destroyedActivityInfo.mActivityName)) {
                        final JSONObject resultJson = new JSONObject();
                        try {
                            resultJson.put(SharePluginInfo.ISSUE_ACTIVITY_NAME, destroyedActivityInfo.mActivityName);
                        } catch (JSONException e) {
                            MatrixLog.printErrStackTrace(TAG, e, "unexpected exception.");
                        }
                        mResourcePlugin.onDetectIssue(new Issue(resultJson));
                    }
                    if (null != activityLeakCallback) {
                        activityLeakCallback.onLeak(destroyedActivityInfo.mActivityName, destroyedActivityInfo.mKey);
                    }
                } else if (mDumpHprofMode == ResourceConfig.DumpMode.AUTO_DUMP) {
                    final File hprofFile = mHeapDumper.dumpHeap();
                    if (hprofFile != null) {
                        markPublished(destroyedActivityInfo.mActivityName);
                        final HeapDump heapDump = new HeapDump(hprofFile, destroyedActivityInfo.mKey, destroyedActivityInfo.mActivityName);
                        mHeapDumpHandler.process(heapDump);
                        infoIt.remove();
                    } else {
                        MatrixLog.i(TAG, "heap dump for further analyzing activity with key [%s] was failed, just ignore.",
                                destroyedActivityInfo.mKey);
                        infoIt.remove();
                    }
                } else if (mDumpHprofMode == ResourceConfig.DumpMode.MANUAL_DUMP) {
                    markPublished(destroyedActivityInfo.mActivityName);
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
                    MatrixLog.i(TAG, "show notification for notify activity leak. %s", destroyedActivityInfo.mActivityName);
                } else {
                    // Lightweight mode, just report leaked activity name.
                    MatrixLog.i(TAG, "lightweight mode, just report leaked activity name.");
                    markPublished(destroyedActivityInfo.mActivityName);
                    if (mResourcePlugin != null) {
                        final JSONObject resultJson = new JSONObject();
                        try {
                            resultJson.put(SharePluginInfo.ISSUE_ACTIVITY_NAME, destroyedActivityInfo.mActivityName);
                        } catch (JSONException e) {
                            MatrixLog.printErrStackTrace(TAG, e, "unexpected exception.");
                        }
                        mResourcePlugin.onDetectIssue(new Issue(resultJson));
                    }
                }
            }

            return Status.RETRY;
        }
    };

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

    private void triggerGc() {
        MatrixLog.v(TAG, "triggering gc...");
        Runtime.getRuntime().gc();
        Runtime.getRuntime().runFinalization();
        MatrixLog.v(TAG, "gc was triggered.");
    }
}
