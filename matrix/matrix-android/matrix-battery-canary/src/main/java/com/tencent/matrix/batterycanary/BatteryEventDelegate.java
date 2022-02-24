package com.tencent.matrix.batterycanary;

import android.annotation.SuppressLint;
import android.app.Application;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;
import android.os.SystemClock;

import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.stats.BatteryStatsFeature;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.util.MatrixLog;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.RestrictTo;
import androidx.annotation.StringDef;
import androidx.annotation.UiThread;
import androidx.annotation.VisibleForTesting;

import static com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil.ONE_MIN;

/**
 * @author Kaede
 * @since 2021/1/11
 */
public final class BatteryEventDelegate {
    public static final String TAG = "Matrix.battery.LifeCycle";
    private static final int BATTERY_POWER_GRADUATION = 5;
    private static final int BATTERY_TEMPERATURE_GRADUATION = 15;

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
    long mLastBatteryPowerPct;
    long mLastBatteryTemp;

    @Nullable
    BatteryMonitorCore mCore;


    BatteryEventDelegate(Context context) {
        if (context == null) {
            throw new IllegalStateException("Context should not be null");
        }
        mContext = context;
        mLastBatteryPowerPct = BatteryCanaryUtil.getBatteryPercentageImmediately(context);
        mLastBatteryTemp = BatteryCanaryUtil.getBatteryTemperature(context);
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
        filter.addAction(Intent.ACTION_BATTERY_CHANGED);

        filter.addAction(Intent.ACTION_SCREEN_ON);
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        filter.addAction(Intent.ACTION_POWER_CONNECTED);
        filter.addAction(Intent.ACTION_POWER_DISCONNECTED);

        filter.addAction(Intent.ACTION_BATTERY_OKAY);
        filter.addAction(Intent.ACTION_BATTERY_LOW);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            filter.addAction(PowerManager.ACTION_POWER_SAVE_MODE_CHANGED);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                filter.addAction(PowerManager.ACTION_DEVICE_IDLE_MODE_CHANGED);
            }
        }

        mContext.registerReceiver(new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                String action = intent.getAction();
                if (action != null) {
                    if (action.equals(Intent.ACTION_BATTERY_CHANGED)) {
                        // 1. Check battery power & temperature changed
                        int currPct = BatteryCanaryUtil.getBatteryPercentage(mContext);
                        if (Math.abs(currPct - mLastBatteryPowerPct) >= BATTERY_POWER_GRADUATION) {
                            mLastBatteryPowerPct = currPct;
                            if (mCore != null) {
                                BatteryStatsFeature feat = mCore.getMonitorFeature(BatteryStatsFeature.class);
                                if (feat != null) {
                                    feat.statsBatteryEvent(currPct);
                                }
                            }
                            onBatteryPowerChanged(currPct);
                        }

                        int currTemp = intent.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, -1);
                        if (currTemp != -1 && Math.abs(currTemp - mLastBatteryTemp) >= BATTERY_TEMPERATURE_GRADUATION) {
                            mLastBatteryTemp = currTemp;
                            if (mCore != null) {
                                BatteryStatsFeature feat = mCore.getMonitorFeature(BatteryStatsFeature.class);
                                if (feat != null) {
                                    feat.statsBatteryTempEvent(currTemp);
                                }
                            }
                            onBatteryTemperatureChanged(currTemp);
                        }

                    } else {
                        int devStat = -1;
                        boolean notifyStateChanged = false, notifyBatteryLowChanged = false;
                        switch (action) {
                            case Intent.ACTION_SCREEN_ON:
                                devStat = AppStats.DEV_STAT_SCREEN_ON;
                                notifyStateChanged = true;
                                break;
                            case Intent.ACTION_SCREEN_OFF:
                                devStat = AppStats.DEV_STAT_SCREEN_OFF;
                                notifyStateChanged = true;
                                break;
                            case Intent.ACTION_POWER_CONNECTED:
                                devStat = AppStats.DEV_STAT_CHARGING;
                                notifyStateChanged = true;
                                break;
                            case Intent.ACTION_POWER_DISCONNECTED:
                                devStat = AppStats.DEV_STAT_UN_CHARGING;
                                notifyStateChanged = true;
                                break;
                            case PowerManager.ACTION_DEVICE_IDLE_MODE_CHANGED:
                                devStat = BatteryCanaryUtil.isDeviceOnIdleMode(context) ? AppStats.DEV_STAT_DOZE_MODE_ON : AppStats.DEV_STAT_DOZE_MODE_OFF;
                                notifyStateChanged = true;
                                break;
                            case PowerManager.ACTION_POWER_SAVE_MODE_CHANGED:
                                devStat = BatteryCanaryUtil.isDeviceOnPowerSave(context) ? AppStats.DEV_STAT_SAVE_POWER_MODE_ON : AppStats.DEV_STAT_SAVE_POWER_MODE_OFF;
                                notifyStateChanged = true;
                                break;
                            case Intent.ACTION_BATTERY_OKAY:
                            case Intent.ACTION_BATTERY_LOW:
                                notifyStateChanged = true;
                                notifyBatteryLowChanged = true;
                                break;
                            default:
                                break;
                        }

                        // 2. Stat device status changed
                        if (devStat != -1) {
                            if (mCore != null) {
                                BatteryStatsFeature feat = mCore.getMonitorFeature(BatteryStatsFeature.class);
                                if (feat != null) {
                                    feat.statsDevStat(devStat);
                                }
                            }
                        }

                        // 3. Notify state changed
                        if (notifyStateChanged) {
                            onSateChangedEvent(intent);
                        }
                        if (notifyBatteryLowChanged) {
                            if (mCore != null) {
                                BatteryStatsFeature feat = mCore.getMonitorFeature(BatteryStatsFeature.class);
                                if (feat != null) {
                                    feat.statsBatteryEvent(action.equals(Intent.ACTION_BATTERY_LOW));
                                }
                            }
                            onBatteryChangeEvent(intent);
                        }
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
            for (Iterator<Listener> iterator = mListenerList.listIterator(); iterator.hasNext();) {
                Listener item = iterator.next();
                if (item == listener) {
                    iterator.remove();
                }
            }
        }
    }

    private void onSateChangedEvent(final Intent intent) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            dispatchSateChangedEvent(intent);
        } else {
            mUiHandler.post(new Runnable() {
                @Override
                public void run() {
                    dispatchSateChangedEvent(intent);
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

    private void onBatteryChangeEvent(final Intent intent) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            dispatchBatteryStateChangedEvent(intent);
        } else {
            mUiHandler.post(new Runnable() {
                @Override
                public void run() {
                    dispatchBatteryStateChangedEvent(intent);
                }
            });
        }
    }

    private void onBatteryPowerChanged(final int pct) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            dispatchBatteryPowerChanged(pct);
        } else {
            mUiHandler.post(new Runnable() {
                @Override
                public void run() {
                    dispatchBatteryPowerChanged(pct);
                }
            });
        }
    }

    private void onBatteryTemperatureChanged(final int temp) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            dispatchBatteryTemperatureChanged(temp);
        } else {
            mUiHandler.post(new Runnable() {
                @Override
                public void run() {
                    dispatchBatteryTemperatureChanged(temp);
                }
            });
        }
    }

    @VisibleForTesting
    void dispatchSateChangedEvent(Intent intent) {
        MatrixLog.i(TAG, "onSateChanged >> " + intent.getAction());
        synchronized (mListenerList) {
            BatteryState batteryState = currentState();
            for (Listener item : mListenerList) {
                if (item instanceof Listener.ExListener) {
                    if (((Listener.ExListener) item).onStateChanged(batteryState, intent.getAction())) {
                        removeListener(item);
                    }
                } else {
                    if (item.onStateChanged(intent.getAction())) {
                        removeListener(item);
                    }
                }
            }
        }
    }

    @VisibleForTesting
    void dispatchAppLowEnergyEvent(long duringMillis) {
        MatrixLog.i(TAG, "onAppLowEnergy >> " + (duringMillis / ONE_MIN) + "min");
        synchronized (mListenerList) {
            BatteryState batteryState = currentState();
            for (Listener item : mListenerList) {
                if (item.onAppLowEnergy(batteryState, duringMillis)) {
                    removeListener(item);
                }
            }
        }
    }

    @VisibleForTesting
    void dispatchBatteryStateChangedEvent(Intent intent) {
        String action = intent.getAction();
        if (Intent.ACTION_BATTERY_OKAY.equals(action) || Intent.ACTION_BATTERY_LOW.equals(action)) {
            MatrixLog.i(TAG, "onBatteryStateChanged >> " + action);
            synchronized (mListenerList) {
                BatteryState batteryState = currentState();
                for (Listener item : mListenerList) {
                    if (item instanceof Listener.ExListener) {
                        if (((Listener.ExListener) item).onBatteryStateChanged(batteryState, Intent.ACTION_BATTERY_LOW.equals(action))) {
                            removeListener(item);
                        }
                    }
                }
            }
        } else {
            throw new IllegalStateException("Illegal battery state: " + action);
        }
    }

    @VisibleForTesting
    void dispatchBatteryPowerChanged(int pct) {
        MatrixLog.i(TAG, "onBatteryPowerChanged >> " + pct + "%");
        synchronized (mListenerList) {
            BatteryState batteryState = currentState();
            for (Listener item : mListenerList) {
                if (item instanceof Listener.ExListener) {
                    if (((Listener.ExListener) item).onBatteryPowerChanged(batteryState, pct)) {
                        removeListener(item);
                    }
                }
            }
        }
    }

    @VisibleForTesting
    void dispatchBatteryTemperatureChanged(int temperature) {
        MatrixLog.i(TAG, "onBatteryTemperatureChanged >> " + (temperature / 10f) + "°C");
        synchronized (mListenerList) {
            BatteryState batteryState = currentState();
            for (Listener item : mListenerList) {
                if (item instanceof Listener.ExListener) {
                    if (((Listener.ExListener) item).onBatteryTemperatureChanged(batteryState, temperature)) {
                        removeListener(item);
                    }
                }
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

        public boolean isForeground() {
            return mCore == null || mCore.isForeground();
        }

        public boolean isCharging() {
            return BatteryCanaryUtil.isDeviceCharging(mContext);
        }

        public boolean isScreenOn() {
            return BatteryCanaryUtil.isDeviceScreenOn(mContext);
        }

        public boolean isSysDozeMode() {
            return BatteryCanaryUtil.isDeviceOnIdleMode(mContext);
        }

        public boolean isAppStandbyMode() {
            return BatteryCanaryUtil.isDeviceOnPowerSave(mContext);
        }

        @SuppressWarnings("unused")
        public boolean isLowBattery() {
            return BatteryCanaryUtil.isLowBattery(mContext);
        }

        @SuppressWarnings("unused")
        public int getBatteryPercentage() {
            return BatteryCanaryUtil.getBatteryPercentage(mContext);
        }

        @SuppressWarnings("unused")
        public int getBatteryCapacity() {
            return BatteryCanaryUtil.getBatteryCapacity(mContext);
        }

        public long getBackgroundTimeMillis() {
            if (isForeground()) return 0L;
            if (sBackgroundBgnMillis <= 0L) return 0L;
            return SystemClock.uptimeMillis() - sBackgroundBgnMillis;
        }

        @Override
        public String toString() {
            return "BatteryState{" +
                    "fg=" + isForeground() +
                    ", charge=" + isCharging() +
                    ", screen=" + isScreenOn() +
                    ", sysDoze=" + isSysDozeMode() +
                    ", appStandby=" + isAppStandbyMode() +
                    ", bgMillis=" + getBackgroundTimeMillis() +
                    '}';
        }
    }


    public interface Listener {
        @RequiresApi(api = Build.VERSION_CODES.M)
        @StringDef(value = {
                Intent.ACTION_SCREEN_ON,
                Intent.ACTION_SCREEN_OFF,
                Intent.ACTION_POWER_CONNECTED,
                Intent.ACTION_POWER_DISCONNECTED,
                Intent.ACTION_BATTERY_OKAY,
                Intent.ACTION_BATTERY_LOW,
                PowerManager.ACTION_DEVICE_IDLE_MODE_CHANGED,
                PowerManager.ACTION_POWER_SAVE_MODE_CHANGED,
        })
        @Retention(RetentionPolicy.SOURCE)
        @interface BatteryEventDef {
        }

        /**
         * @see ExListener#onStateChanged(BatteryState, String)
         * @return return true if your listening is done, thus we remove your listener
         */
        @UiThread
        @Deprecated
        boolean onStateChanged(@BatteryEventDef String event);

        /**
         * @return return true if your listening is done, thus we remove your listener
         */
        @UiThread
        boolean onAppLowEnergy(BatteryState batteryState, long backgroundMillis);

        interface ExListener extends Listener {

            @UiThread
            boolean onStateChanged(BatteryState batteryState, @BatteryEventDef String event);

            /**
             * On battery temperature changed.
             *
             * @param batteryState {@link BatteryState}
             * @param temperature  See {@link BatteryManager#EXTRA_TEMPERATURE}, °C * 10
             * @return return true if your listening is done, thus we remove your listener
             */
            @UiThread
            boolean onBatteryTemperatureChanged(BatteryState batteryState, int temperature);

            /**
             * On battery power changed.
             *
             * @param batteryState {@link BatteryState}
             * @param levelPct     Battery capacity level 0 - 100
             * @return return true if your listening is done, thus we remove your listener
             */
            @UiThread
            boolean onBatteryPowerChanged(BatteryState batteryState, int levelPct);

            /**
             * On battery power low or ok.
             *
             * @param batteryState {@link BatteryState}
             * @param isLowBattery {@link Intent#ACTION_BATTERY_LOW}, {@link Intent#ACTION_BATTERY_OKAY}
             * @return return true if your listening is done, thus we remove your listener
             */
            @UiThread
            boolean onBatteryStateChanged(BatteryState batteryState, boolean isLowBattery);
        }

        @SuppressWarnings("unused")
        class DefaultListenerImpl implements ExListener {

            final boolean mKeepAlive;

            public DefaultListenerImpl(boolean keepAlive) {
                mKeepAlive = keepAlive;
            }

            @Override
            public boolean onStateChanged(String event) {
                return !mKeepAlive;
            }

            @Override
            public boolean onAppLowEnergy(BatteryState batteryState, long backgroundMillis) {
                return !mKeepAlive;
            }

            @Override
            public boolean onStateChanged(BatteryState batteryState, String event) {
                return !mKeepAlive;
            }

            @Override
            public boolean onBatteryTemperatureChanged(BatteryState batteryState, int temperature) {
                return !mKeepAlive;
            }

            @Override
            public boolean onBatteryPowerChanged(BatteryState batteryState, int levelPct) {
                return !mKeepAlive;
            }

            @Override
            public boolean onBatteryStateChanged(BatteryState batteryState, boolean isLowBattery) {
                return !mKeepAlive;
            }
        }
    }
}
