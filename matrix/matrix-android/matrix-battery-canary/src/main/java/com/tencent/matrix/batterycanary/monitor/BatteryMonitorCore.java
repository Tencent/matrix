package com.tencent.matrix.batterycanary.monitor;

import android.content.Context;
import android.os.Handler;
import android.os.Message;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;

import com.tencent.matrix.AppActiveMatrixDelegate;
import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.monitor.feature.AlarmMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.LooperTaskMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature.WakeLockTrace.WakeLockRecord;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import java.util.List;

public class BatteryMonitorCore implements Handler.Callback, LooperTaskMonitorFeature.LooperTaskListener,
        WakeLockMonitorFeature.WakeLockListener, AlarmMonitorFeature.AlarmListener {
    private static final String TAG = "Matrix.battery.watchdog";

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
                mHandler.postDelayed(this, mLooperMillis);
            }
        }
    }

    private static final int MSG_ID_JIFFIES_START = 0x1;
    private static final int MSG_ID_JIFFIES_END = 0x2;
    private static final int MSG_ARG_FOREGROUND = 0x3;

    @NonNull private Handler mHandler;
    @Nullable private ForegroundLoopCheckTask mLooperTask;
    private final BatteryMonitorConfig mConfig;

    private volatile boolean mTurnOn = false;
    private boolean mAppForeground = AppActiveMatrixDelegate.INSTANCE.isAppForeground();
    private boolean mForegroundModeEnabled;
    private static long mMonitorDelayMillis;
    private static long mLooperMillis;

    public BatteryMonitorCore(BatteryMonitorConfig config) {
        mConfig = config;
        if (config.callback instanceof BatteryMonitorCallback.BatteryPrinter) ((BatteryMonitorCallback.BatteryPrinter) config.callback).attach(this);

        mHandler = new Handler(MatrixHandlerThread.getDefaultHandlerThread().getLooper(), this);
        enableForegroundLoopCheck(config.isForegroundModeEnabled);
        mMonitorDelayMillis = config.greyTime;
        mLooperMillis = config.foregroundLoopCheckTime;

        for (MonitorFeature plugin : config.features) {
            plugin.configure(this);
        }
    }

    @VisibleForTesting
    public void enableForegroundLoopCheck(boolean bool) {
        mForegroundModeEnabled = bool;
        if (mForegroundModeEnabled) {
            mLooperTask = new ForegroundLoopCheckTask();
        }
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

        if (!isForeground) {
            // back:
            // 1. remove all checks
            mHandler.removeCallbacksAndMessages(null);
            // 2. start background jiffies check
            Message message = Message.obtain(mHandler);
            message.what = MSG_ID_JIFFIES_START;
            mHandler.sendMessageDelayed(message, mMonitorDelayMillis);
        } else if (!mHandler.hasMessages(MSG_ID_JIFFIES_START)) {
            // fore:
            // 1. finish background jiffies check
            Message message = Message.obtain(mHandler);
            message.what = MSG_ID_JIFFIES_END;
            mHandler.sendMessageAtFrontOfQueue(message);
            // 2. start foreground jiffies loop check
            if (mForegroundModeEnabled && mLooperTask != null) {
                mHandler.removeCallbacks(mLooperTask);
                mLooperTask.lastWhat = MSG_ID_JIFFIES_START;
                mHandler.post(mLooperTask);
            }
        }

        for (MonitorFeature plugin : mConfig.features) {
            plugin.onForeground(isForeground);
        }
    }

    public Context getContext() {
        // FIXME: context api configs
        return Matrix.with().getApplication();
    }

    public boolean isForeground() {
        return mAppForeground;
    }

    public int getCurrentBatteryTemperature(Context context) {
        try {
            return BatteryCanaryUtil.getBatteryTemperature(context);
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "#currentBatteryTemperature error");
            return 0;
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
    public void onWakeLockTimeout(int warningCount, WakeLockRecord record) {
        MatrixLog.d(TAG, "#onWakeLockTimeout, tag = " + record.tag + ", pkg = " + record.packageName + ", count = " + warningCount);
        getConfig().callback.onWakeLockTimeout(warningCount, record);
    }

    @Override
    public void onAlarmDuplicated(int duplicatedCount, AlarmMonitorFeature.AlarmRecord record) {
        MatrixLog.d(TAG, "#onAlarmDuplicated");
        getConfig().callback.onAlarmDuplicated(duplicatedCount, record);
    }
}
