package com.tencent.matrix.batterycanary;

import android.annotation.SuppressLint;
import android.app.Application;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.RestrictTo;
import android.support.annotation.UiThread;
import android.support.annotation.VisibleForTesting;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;

import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

/**
 * @author Kaede
 * @since 2021/1/11
 */
public final class BatteryEventDelegate {
    static final String TAG = "Matrix.battery.EventDelegate";

    @SuppressLint("StaticFieldLeak")
    static volatile BatteryEventDelegate sInstance;
    static long sBackgroundBgnMillis = 0L;

    @VisibleForTesting
    static void release() {
        sInstance = null;
    }

    public static boolean isInit() {
        if (sInstance != null) {
            return true;
        } else {
            synchronized (TAG) {
                return sInstance != null;
            }
        }
    }

    public static void init(Application application) {
        if (sInstance == null) {
            synchronized (TAG) {
                if (sInstance == null) {
                    sInstance = new BatteryEventDelegate(application);
                }
            }
        }
    }

    public static BatteryEventDelegate getInstance() {
        if (sInstance == null) {
            synchronized (TAG) {
                if (sInstance == null) {
                   throw new IllegalStateException("Call #init() first!");
                }
            }
        }
        return sInstance;
    }

    final Context mContext;
    final List<Listener> mListenerList = new LinkedList<>();
    final Handler mUiHandler = new Handler(Looper.getMainLooper());
    final BackgroundTask mAppLowEnergyTask = new BackgroundTask();
    boolean sIsForeground = true;
    @Nullable
    BatteryMonitorCore mCore;


    BatteryEventDelegate(Context context) {
        if (context == null) {
            throw new IllegalStateException("Context should not be null");
        }
        mContext = context;
    }

    public BatteryEventDelegate attach(BatteryMonitorCore core) {
        if (core != null) {
            mCore = core;
        }
        return this;
    }

    @RestrictTo(RestrictTo.Scope.LIBRARY)
    public void onForeground(boolean isForeground) {
        sIsForeground = isForeground;
        if (isForeground) {
            sBackgroundBgnMillis = 0L;
            mUiHandler.removeCallbacks(mAppLowEnergyTask);
        } else {
            sBackgroundBgnMillis = SystemClock.uptimeMillis();
            mUiHandler.postDelayed(mAppLowEnergyTask, mAppLowEnergyTask.reset());
        }
    }

    @RestrictTo(RestrictTo.Scope.LIBRARY)
    public void startListening() {
        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_SCREEN_ON);
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        filter.addAction(Intent.ACTION_POWER_CONNECTED);
        filter.addAction(Intent.ACTION_POWER_DISCONNECTED);

        mContext.registerReceiver(new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                String action = intent.getAction();
                if (action != null) {
                    switch (action) {
                        case Intent.ACTION_SCREEN_ON:
                        case Intent.ACTION_SCREEN_OFF:
                        case Intent.ACTION_POWER_CONNECTED:
                        case Intent.ACTION_POWER_DISCONNECTED:
                            onSateChangedEvent();
                            break;
                    }
                }

            }
        }, filter);
    }

    public BatteryState currentState() {
        return new BatteryState(mContext).attach(mCore);
    }

    public void addListener(@NonNull Listener listener) {
        synchronized (mListenerList) {
            if (!mListenerList.contains(listener)) {
                mListenerList.add(listener);
            }
        }
    }

    public void removeListener(@NonNull Listener listener) {
        synchronized (mListenerList) {
            for (Iterator<Listener> iterator = mListenerList.listIterator(); iterator.hasNext(); ) {
                Listener item = iterator.next();
                if (item == listener) {
                    iterator.remove();
                }
            }
        }
    }

    private void onSateChangedEvent() {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            dispatchSateChangedEvent();
        } else {
            mUiHandler.post(new Runnable() {
                @Override
                public void run() {
                    dispatchSateChangedEvent();
                }
            });
        }
    }

    private void onAppLowEnergyEvent(final long duringMillis) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            dispatchAppLowEnergyEvent(duringMillis);
        } else {
            mUiHandler.post(new Runnable() {
                @Override
                public void run() {
                    dispatchAppLowEnergyEvent(duringMillis);
                }
            });
        }
    }

    @VisibleForTesting
    void dispatchSateChangedEvent() {
        synchronized (mListenerList) {
            for (Listener item : mListenerList) {
                item.onStateChanged(currentState());
            }
        }
    }

    @VisibleForTesting
    void dispatchAppLowEnergyEvent(long duringMillis) {
        synchronized (mListenerList) {
            for (Listener item : mListenerList) {
                item.onAppLowEnergy(currentState(), duringMillis);
            }
        }
    }

    public final class BackgroundTask implements Runnable {
        private long duringMillis;

        long reset() {
            duringMillis = 0L;
            setNext(5 * 60 * 1000L);
            return duringMillis;
        }

        long setNext(long millis) {
            duringMillis += millis;
            return millis;
        }

        @Override
        public void run() {
            if (!sIsForeground) {
                if (!BatteryCanaryUtil.isDeviceCharging(mContext)) {
                    // On app low energy event
                    onAppLowEnergyEvent(duringMillis);
                }

                // next loop
                if (duringMillis <= 5 * 60 * 1000L) {
                    mUiHandler.postDelayed(this, setNext(5 * 60 * 1000L));
                } else {
                    if (duringMillis <= 10 * 60 * 1000L) {
                        mUiHandler.postDelayed(this, setNext(20 * 60 * 1000L));
                    }
                }
            }
        }
    }


    public static final class BatteryState {
        @Nullable
        BatteryMonitorCore mCore;
        final Context mContext;

        public BatteryState(Context context) {
            mContext = context;
        }

        public BatteryState attach(BatteryMonitorCore core) {
            if (core != null) {
                mCore = core;
            }
            return this;
        }

        public boolean isOnBackground() {
            return mCore != null && !mCore.isForeground();
        }

        public boolean isCharging() {
            return BatteryCanaryUtil.isDeviceCharging(mContext);
        }

        public boolean isScreenOff() {
            return !BatteryCanaryUtil.isDeviceScreenOn(mContext);
        }

        public boolean isPowerSaveMode() {
            return BatteryCanaryUtil.isDeviceOnPowerSave(mContext);
        }

        public long getBackgroundTimeMillis() {
            if (!isOnBackground()) return 0L;
            if (sBackgroundBgnMillis <= 0L) return 0L;
            return SystemClock.uptimeMillis() - sBackgroundBgnMillis;
        }
    }

    public interface Listener {
        @UiThread void onStateChanged(BatteryState batteryState);
        @UiThread void onAppLowEnergy(BatteryState batteryState, long backgroundMillis);
    }
}
