package com.tencent.matrix.batterycanary.monitor;

import android.annotation.SuppressLint;
import android.content.ComponentName;
import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryEventDelegate;
import com.tencent.matrix.batterycanary.monitor.feature.AbsTaskMonitorFeature.TaskJiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.AlarmMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.AppStatMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot.ThreadJiffiesEntry;
import com.tencent.matrix.batterycanary.monitor.feature.LooperTaskMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.ListEntry;
import com.tencent.matrix.batterycanary.monitor.feature.NotificationMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.NotificationMonitorFeature.BadNotification;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature.WakeLockTrace.WakeLockRecord;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.lifecycle.owners.ProcessUILifecycleOwner;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import java.util.List;
import java.util.concurrent.Callable;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

public class BatteryMonitorCore implements
        LooperTaskMonitorFeature.LooperTaskListener,
        WakeLockMonitorFeature.WakeLockListener,
        AlarmMonitorFeature.AlarmListener,
        JiffiesMonitorFeature.JiffiesListener,
        AppStatMonitorFeature.AppStatListener,
        NotificationMonitorFeature.NotificationListener,
        Handler.Callback {

    private static final String TAG = "Matrix.battery.BatteryMonitorCore";

    public interface Callback<T extends MonitorFeature.Snapshot<T>> {
        void onGetJiffies(T snapshot);
    }

    public interface JiffiesListener {
        void onTraceBegin();
        void onTraceEnd(boolean isForeground); // TODO: configurable status support
    }

    private class ForegroundLoopCheckTask implements Runnable {
        int lastWhat = MSG_ID_JIFFIES_START;
        @Override
        public void run() {
            if (mForegroundModeEnabled) {
                Message message = Message.obtain(mHandler);
                message.what = lastWhat;
                message.arg1 = MSG_ARG_FOREGROUND;
                mHandler.sendMessageAtFrontOfQueue(message);
                lastWhat = (lastWhat == MSG_ID_JIFFIES_END ? MSG_ID_JIFFIES_START : MSG_ID_JIFFIES_END);
                mHandler.postDelayed(this, mFgLooperMillis);
            }
        }
    }

    private class BackgroundLoopCheckTask implements Runnable {
        int round = 0;
        @Override
        public void run() {
            round++;
            MatrixLog.i(TAG, "#onBackgroundLoopCheck, round = " + round);
            if (!isForeground()) {
                synchronized (BatteryMonitorCore.class) {
                    for (MonitorFeature plugin : mConfig.features) {
                        plugin.onBackgroundCheck(mBgLooperMillis * round);
                    }
                }
            }
            if (!isForeground()) {
                mHandler.postDelayed(this, mBgLooperMillis);
            }
        }
    }

    private static final int MSG_ID_JIFFIES_START = 0x1;
    private static final int MSG_ID_JIFFIES_END = 0x2;
    private static final int MSG_ARG_FOREGROUND = 0x3;

    private final BatteryMonitorConfig mConfig;
    @NonNull private final Handler mHandler;
    @NonNull private final Handler mCanaryHandler;
    @Nullable private ForegroundLoopCheckTask mFgLooperTask;
    @Nullable private BackgroundLoopCheckTask mBgLooperTask;

    @NonNull
    Callable<String> mSupplier = new Callable<String>() {
        @Override
        public String call() {
            return "unknown";
        }
    };

    private volatile boolean mTurnOn = false;
    private boolean mAppForeground = ProcessUILifecycleOwner.INSTANCE.isProcessForeground();
    private boolean mForegroundModeEnabled;
    private boolean mBackgroundModeEnabled;
    private final long mMonitorDelayMillis;
    private final long mFgLooperMillis;
    private final long mBgLooperMillis;

    @SuppressLint("VisibleForTests")
    public BatteryMonitorCore(BatteryMonitorConfig config) {
        mConfig = config;
        if (config.callback instanceof BatteryMonitorCallback.BatteryPrinter) ((BatteryMonitorCallback.BatteryPrinter) config.callback).attach(this);
        if (config.onSceneSupplier != null) {
            mSupplier = config.onSceneSupplier;
        }

        if (config.canaryThread != null) {
            HandlerThread thread = config.canaryThread;
            mHandler = new Handler(thread.getLooper(), this);       // For BatteryMonitorCore only
            mCanaryHandler = new Handler(thread.getLooper(), this); // For BatteryCanary
        } else {
            HandlerThread thread = MatrixHandlerThread.getDefaultHandlerThread();
            mHandler = new Handler(thread.getLooper(), this);       // For BatteryMonitorCore only
            mCanaryHandler = mHandler;                                      // For BatteryCanary as legacy logic
        }

        enableForegroundLoopCheck(config.isForegroundModeEnabled);
        enableBackgroundLoopCheck(config.isBackgroundModeEnabled);
        mMonitorDelayMillis = config.greyTime;
        mFgLooperMillis = config.foregroundLoopCheckTime;
        mBgLooperMillis = config.backgroundLoopCheckTime;

        for (MonitorFeature plugin : config.features) {
            plugin.configure(this);
        }
    }

    @VisibleForTesting
    public void enableForegroundLoopCheck(boolean bool) {
        mForegroundModeEnabled = bool;
        if (mForegroundModeEnabled) {
            mFgLooperTask = new ForegroundLoopCheckTask();
        }
    }

    @VisibleForTesting
    public void enableBackgroundLoopCheck(boolean bool) {
        mBackgroundModeEnabled = bool;
    }

    @Override
    public boolean handleMessage(Message msg) {
        if (msg.what == MSG_ID_JIFFIES_START) {
            notifyTraceBegin();
            return true;
        }
        if (msg.what == MSG_ID_JIFFIES_END) {
            notifyTraceEnd(msg.arg1 == MSG_ARG_FOREGROUND);
            return true;
        }
        return false;
    }


    public <T extends MonitorFeature> T getMonitorFeature(Class<T> clazz) {
        for (MonitorFeature plugin : mConfig.features) {
            if (clazz.isAssignableFrom(plugin.getClass())) {
                //noinspection unchecked
                return (T) plugin;
            }
        }
        return null;
    }

    public BatteryMonitorConfig getConfig() {
        return mConfig;
    }

    public boolean isTurnOn() {
        synchronized (BatteryMonitorCore.class) {
            return mTurnOn;
        }
    }

    public void start() {
        synchronized (BatteryMonitorCore.class) {
            if (!mTurnOn) {
                for (MonitorFeature plugin : mConfig.features) {
                    plugin.onTurnOn();
                }
                mTurnOn = true;
            }
            if (BatteryEventDelegate.isInit()) {
                BatteryEventDelegate.getInstance().attach(this).startListening();
            }
        }
    }

    public void stop() {
        synchronized (BatteryMonitorCore.class) {
            if (mTurnOn) {
                mHandler.removeCallbacksAndMessages(null);
                for (MonitorFeature plugin : mConfig.features) {
                    plugin.onTurnOff();
                }
                mTurnOn = false;
            }
        }
    }

    public void onForeground(boolean isForeground) {
        if (!Matrix.isInstalled()) {
            MatrixLog.e(TAG, "Matrix was not installed yet, just ignore the event");
            return;
        }
        mAppForeground = isForeground;

        if (BatteryEventDelegate.isInit()) {
            BatteryEventDelegate.getInstance().onForeground(isForeground);
        }

        if (!isForeground) {
            // back:
            // 1. remove all checks
            mHandler.removeCallbacksAndMessages(null);

            // 2. start background jiffies check
            Message message = Message.obtain(mHandler);
            message.what = MSG_ID_JIFFIES_START;
            mHandler.sendMessageDelayed(message, mMonitorDelayMillis);

            // 3. start background loop check task
            if (mBackgroundModeEnabled) {
                if (mBgLooperTask != null) {
                    mHandler.removeCallbacks(mBgLooperTask);
                    mBgLooperTask = null;
                }
                mBgLooperTask = new BackgroundLoopCheckTask();
                mHandler.postDelayed(mBgLooperTask, mBgLooperMillis);
            }

        } else if (!mHandler.hasMessages(MSG_ID_JIFFIES_START)) {
            // fore:
            // 1. remove background loop task
            if (mBgLooperTask != null) {
                mHandler.removeCallbacks(mBgLooperTask);
                mBgLooperTask = null;
            }

            // 2. finish background jiffies check
            Message message = Message.obtain(mHandler);
            message.what = MSG_ID_JIFFIES_END;
            mHandler.sendMessageAtFrontOfQueue(message);

            // 3. start foreground jiffies loop check
            if (mForegroundModeEnabled && mFgLooperTask != null) {
                mHandler.removeCallbacks(mFgLooperTask);
                mFgLooperTask.lastWhat = MSG_ID_JIFFIES_START;
                mHandler.post(mFgLooperTask);
            }
        }

        for (MonitorFeature plugin : mConfig.features) {
            plugin.onForeground(isForeground);
        }
    }

    @NonNull
    public Handler getHandler() {
        return mCanaryHandler;
    }

    public Context getContext() {
        // FIXME: context api configs
        return Matrix.with().getApplication();
    }

    public String getScene() {
        try {
            return mSupplier.call();
        } catch (Exception e) {
            return "unknown";
        }
    }

    public boolean isForeground() {
        return mAppForeground;
    }

    public int getCurrentBatteryTemperature(Context context) {
        try {
            int tmp = BatteryCanaryUtil.getBatteryTemperature(context);
            MatrixLog.i(TAG, "onGetTemperature, battery = " + tmp);
            return tmp;
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "#currentBatteryTemperature error");
            return -1;
        }
    }

    private void notifyTraceBegin() {
        MatrixLog.d(TAG, "#onTraceBegin");
        getConfig().callback.onTraceBegin();
    }

    private void notifyTraceEnd(boolean isForeground) {
        MatrixLog.d(TAG, "#onTraceEnd");
        getConfig().callback.onTraceEnd(isForeground);
    }

    @Override
    public void onTaskTrace(Thread thread, List<LooperTaskMonitorFeature.TaskTraceInfo> sortList) {
        MatrixLog.d(TAG, "#onTaskTrace, thread = " + thread.getName());
        getConfig().callback.onTaskTrace(thread, sortList);
    }

    @Override
    public void onLooperTaskOverHeat(@NonNull List<Delta<TaskJiffiesSnapshot>> deltas) {
        getConfig().callback.onLooperTaskOverHeat(deltas);
    }

    @Override
    public void onLooperConcurrentOverHeat(String key, int concurrentCount, long duringMillis) {
        getConfig().callback.onLooperConcurrentOverHeat(key, concurrentCount, duringMillis);
    }

    @Override
    public void onWakeLockTimeout(int warningCount, WakeLockRecord record) {
        getConfig().callback.onWakeLockTimeout(warningCount, record);
    }

    @Override
    public void onWakeLockTimeout(WakeLockRecord record, long backgroundMillis) {
        getConfig().callback.onWakeLockTimeout(record, backgroundMillis);
    }

    @Override
    public void onAlarmDuplicated(int duplicatedCount, AlarmMonitorFeature.AlarmRecord record) {
        getConfig().callback.onAlarmDuplicated(duplicatedCount, record);
    }

    @Override
    public void onParseError(int pid, int tid) {
        getConfig().callback.onParseError(pid, tid);
    }

    @Override
    public void onWatchingThreads(ListEntry<? extends ThreadJiffiesEntry> threadJiffiesList) {
        getConfig().callback.onWatchingThreads(threadJiffiesList);
    }

    @Override
    public void onForegroundServiceLeak(boolean isMyself, int appImportance, int globalAppImportance, ComponentName componentName, long millis) {
        getConfig().callback.onForegroundServiceLeak(isMyself, appImportance, globalAppImportance, componentName, millis);
    }

    @Override
    public void onAppSateLeak(boolean isMyself, int appImportance, ComponentName componentName, long millis) {
        getConfig().callback.onAppSateLeak(isMyself, appImportance, componentName, millis);
    }

    @Override
    public void onNotify(BadNotification notification) {
        getConfig().callback.onNotify(notification);
    }
}
