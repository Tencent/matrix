package com.tencent.matrix.batterycanary.monitor.feature;

import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.Context;
import android.os.Looper;
import android.text.TextUtils;

import com.tencent.matrix.batterycanary.BatteryEventDelegate;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.stats.BatteryRecord;
import com.tencent.matrix.batterycanary.stats.BatteryStatsFeature;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.TimeBreaker;
import com.tencent.matrix.lifecycle.IStateObserver;
import com.tencent.matrix.lifecycle.owners.OverlayWindowLifecycleOwner;
import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import androidx.annotation.NonNull;
import androidx.annotation.WorkerThread;

import static com.tencent.matrix.batterycanary.monitor.AppStats.APP_STAT_BACKGROUND;
import static com.tencent.matrix.batterycanary.monitor.AppStats.APP_STAT_FLOAT_WINDOW;
import static com.tencent.matrix.batterycanary.monitor.AppStats.APP_STAT_FOREGROUND;
import static com.tencent.matrix.batterycanary.monitor.AppStats.APP_STAT_FOREGROUND_SERVICE;

/**
 * @author Kaede
 * @since 2020/12/8
 */
public final class AppStatMonitorFeature extends AbsMonitorFeature {

    public interface AppStatListener {
        void onForegroundServiceLeak(boolean isMyself, int appImportance, int globalAppImportance, ComponentName componentName, long millis);
        void onAppSateLeak(boolean isMyself, int appImportance, ComponentName componentName, long millis);
    }

    private static final String TAG = "Matrix.battery.AppStatMonitorFeature";
    /**
     * Less important than {@link ActivityManager.RunningAppProcessInfo.IMPORTANCE_GONE}
     */
    @SuppressWarnings("JavadocReference")
    public static final int IMPORTANCE_LEAST = 1024;

    int mAppImportance = IMPORTANCE_LEAST;
    int mGlobalAppImportance = IMPORTANCE_LEAST;
    int mForegroundServiceImportanceLimit = ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND;

    @NonNull
    List<TimeBreaker.Stamp> mStampList = Collections.emptyList();
    @NonNull
    List<TimeBreaker.Stamp> mSceneStampList = Collections.emptyList();
    @NonNull
    Runnable coolingTask = new Runnable() {
        @Override
        public void run() {
            if (mStampList.size() >= mCore.getConfig().overHeatCount) {
                synchronized (TAG) {
                    TimeBreaker.gcList(mStampList);
                }
            }
            if (mSceneStampList.size() >= mCore.getConfig().overHeatCount) {
                synchronized (TAG) {
                    TimeBreaker.gcList(mSceneStampList);
                }
            }
        }
    };

    private final IStateObserver mFloatViewObserver = new IStateObserver() {
        @Override
        public void on() {
            MatrixLog.i(TAG, "floatView >> on");
            boolean foreground = mCore.isForeground();
            int appStat = BatteryCanaryUtil.getAppStatImmediately(mCore.getContext(), foreground);
            if (appStat != APP_STAT_FOREGROUND && appStat != APP_STAT_FOREGROUND_SERVICE) {
                MatrixLog.i(TAG, "statAppStat: " + APP_STAT_FLOAT_WINDOW);
                onStatAppStat(APP_STAT_FLOAT_WINDOW);
            } else {
                MatrixLog.i(TAG, "skip statAppStat, fg = " + foreground + ", currAppStat = " + appStat);
            }
        }

        @Override
        public void off() {
            MatrixLog.i(TAG, "floatView >> off");
            boolean foreground = mCore.isForeground();
            int appStat = BatteryCanaryUtil.getAppStatImmediately(mCore.getContext(), foreground);
            if (appStat != APP_STAT_FOREGROUND && appStat != APP_STAT_FOREGROUND_SERVICE) {
                MatrixLog.i(TAG, "statAppStat: " + APP_STAT_BACKGROUND);
                onStatAppStat(APP_STAT_BACKGROUND);
            } else {
                MatrixLog.i(TAG, "skip statAppStat, fg = " + foreground + ", currAppStat = " + appStat);
            }
        }
    };

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    public void configure(BatteryMonitorCore monitor) {
        super.configure(monitor);
        mForegroundServiceImportanceLimit = Math.max(monitor.getConfig().foregroundServiceLeakLimit, ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND);
    }

    @Override
    public void onTurnOn() {
        super.onTurnOn();
        TimeBreaker.Stamp firstStamp = new TimeBreaker.Stamp(String.valueOf(APP_STAT_FOREGROUND));
        TimeBreaker.Stamp firstSceneStamp = new TimeBreaker.Stamp(mCore.getScene());
        synchronized (TAG) {
            mStampList = new ArrayList<>();
            mStampList.add(0, firstStamp);
            mSceneStampList = new ArrayList<>();
            mSceneStampList.add(0, firstSceneStamp);
        }

        OverlayWindowLifecycleOwner.INSTANCE.observeForever(mFloatViewObserver);
    }

    @Override
    public void onTurnOff() {
        super.onTurnOff();
        OverlayWindowLifecycleOwner.INSTANCE.removeObserver(mFloatViewObserver);
        synchronized (TAG) {
            mStampList.clear();
            mSceneStampList.clear();
        }
    }

    @Override
    public void onForeground(boolean isForeground) {
        super.onForeground(isForeground);
        int appStat = BatteryCanaryUtil.getAppStatImmediately(mCore.getContext(), isForeground);
        BatteryCanaryUtil.getProxy().updateAppStat(appStat);
        onStatAppStat(appStat);

        MatrixLog.i(TAG, "updateAppImportance when app " + (isForeground ? "foreground" : "background"));
        updateAppImportance();

        // if (!isForeground) {
        //     MatrixLog.i(TAG, "checkBackgroundAppState when app background");
        //     checkBackgroundAppState(0L);
        // }
    }

    @WorkerThread
    @Override
    public void onBackgroundCheck(long duringMillis) {
        super.onBackgroundCheck(duringMillis);
        MatrixLog.i(TAG, "#onBackgroundCheck, during = " + duringMillis);

        if (mGlobalAppImportance > mForegroundServiceImportanceLimit || mAppImportance > mForegroundServiceImportanceLimit) {
            Context context = mCore.getContext();
            ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
            if (am == null) {
                return;
            }
            List<ActivityManager.RunningServiceInfo> runningServices = am.getRunningServices(Integer.MAX_VALUE);
            if (runningServices == null) {
                return;
            }

            for (ActivityManager.RunningServiceInfo item : runningServices) {
                if (!TextUtils.isEmpty(item.process) && item.process.startsWith(context.getPackageName())) {
                    if (item.foreground) {
                        MatrixLog.i(TAG, "checkForegroundService whether app importance is low, during = " + duringMillis);
                        // foreground service is running when app importance is low
                        if (mGlobalAppImportance > mForegroundServiceImportanceLimit) {
                            // global
                            MatrixLog.w(TAG, "foreground service detected with low global importance: "
                                    + mAppImportance + ", " + mGlobalAppImportance + ", " + item.service);
                            mCore.onForegroundServiceLeak(false, mAppImportance, mGlobalAppImportance, item.service, duringMillis);
                        }

                        if (mAppImportance > mForegroundServiceImportanceLimit) {
                            if (item.process.equals(context.getPackageName())) {
                                // myself
                                MatrixLog.w(TAG, "foreground service detected with low app importance: "
                                        + mAppImportance + ", " + mGlobalAppImportance + ", " + item.service);
                                mCore.onForegroundServiceLeak(true, mAppImportance, mGlobalAppImportance, item.service, duringMillis);
                            }
                        }
                    }
                }
            }
        }

        // MatrixLog.i(TAG, "checkBackgroundAppState when app background, during = " + duringMillis);
        // checkBackgroundAppState(duringMillis);
    }

    public void onStatAppStat(int appStat) {
        synchronized (TAG) {
            if (mStampList != Collections.EMPTY_LIST) {
                MatrixLog.i(BatteryEventDelegate.TAG, "onStat >> " + BatteryCanaryUtil.convertAppStat(appStat));
                mStampList.add(0, new TimeBreaker.Stamp(String.valueOf(appStat)));
                checkOverHeat();
            }
        }
    }

    @SuppressWarnings("unused")
    public void onStatScene(@NonNull String scene) {
        BatteryStatsFeature statsFeature = mCore.getMonitorFeature(BatteryStatsFeature.class);
        if (statsFeature != null) {
            BatteryRecord.SceneStatRecord statRecord = new BatteryRecord.SceneStatRecord();
            statRecord.scene = scene;
            statsFeature.writeRecord(statRecord);
        }

        synchronized (TAG) {
            if (mSceneStampList != Collections.EMPTY_LIST) {
                mSceneStampList.add(0, new TimeBreaker.Stamp(scene));
                checkOverHeat();
            }
        }

        MatrixLog.i(TAG, "updateAppImportance when launch: " + scene);
        updateAppImportance();
    }

    private void checkOverHeat() {
        mCore.getHandler().removeCallbacks(coolingTask);
        mCore.getHandler().postDelayed(coolingTask, 1000L);
    }

    private void updateAppImportance() {
        if (mAppImportance <= mForegroundServiceImportanceLimit && mGlobalAppImportance <= mForegroundServiceImportanceLimit) {
            return;
        }

        Runnable runnable = new Runnable() {
            @SuppressWarnings("SpellCheckingInspection")
            @Override
            public void run() {
                Context context = mCore.getContext();
                String mainProc = context.getPackageName();
                if (mainProc.contains(":")) {
                    mainProc = mainProc.substring(0, mainProc.indexOf(":"));
                }

                ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
                if (am == null) {
                    return;
                }
                List<ActivityManager.RunningAppProcessInfo> processes = am.getRunningAppProcesses();
                if (processes == null) {
                    return;
                }

                for (ActivityManager.RunningAppProcessInfo item : processes) {
                    if (item.processName.startsWith(mainProc)) {
                        if (mGlobalAppImportance > item.importance) {
                            MatrixLog.i(TAG, "update global importance: " + mGlobalAppImportance + " > " + item.importance
                                    + ", reason = " + item.importanceReasonComponent);
                            mGlobalAppImportance = item.importance;
                        }
                        if (item.processName.equals(context.getPackageName())) {
                            if (mAppImportance > item.importance) {
                                MatrixLog.i(TAG, "update app importance: " + mAppImportance + " > " + item.importance
                                        + ", reason = " + item.importanceReasonComponent);
                                mAppImportance = item.importance;
                            }
                        }
                    }
                }
            }
        };

        if (Looper.myLooper() == Looper.getMainLooper()) {
            mCore.getHandler().post(runnable);
        } else {
            runnable.run();
        }
    }

    private void checkBackgroundAppState(final long duringMillis) {
        Runnable runnable = new Runnable() {
            @SuppressWarnings("SpellCheckingInspection")
            @Override
            public void run() {
                Context context = mCore.getContext();
                String mainProc = context.getPackageName();
                if (mainProc.contains(":")) {
                    mainProc = mainProc.substring(0, mainProc.indexOf(":"));
                }

                ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
                if (am == null) {
                    return;
                }
                List<ActivityManager.RunningAppProcessInfo> processes = am.getRunningAppProcesses();
                if (processes == null) {
                    return;
                }

                MatrixLog.i(TAG, "Dump backgroud app sate:");
                for (ActivityManager.RunningAppProcessInfo item : processes) {
                    if (item.processName.startsWith(mainProc)) {
                        if (item.importance <= mForegroundServiceImportanceLimit) {
                            // FIXME: maybe implicit
                            MatrixLog.w(TAG, " + " + item.processName + ", proc = " + item.importance + ", reason = " + item.importanceReasonComponent);
                            mCore.onAppSateLeak(item.processName.equals(context.getPackageName()), item.importance, item.importanceReasonComponent, duringMillis);

                        } else {
                            MatrixLog.i(TAG, " - " + item.processName + ", proc = " + item.importance + ", reason = " + item.importanceReasonComponent);
                        }
                    }
                }
            }
        };

        if (Looper.myLooper() == Looper.getMainLooper()) {
            mCore.getHandler().post(runnable);
        } else {
            runnable.run();
        }
    }

    @Override
    public int weight() {
        return Integer.MAX_VALUE;
    }

    public AppStatSnapshot currentAppStatSnapshot() {
        return currentAppStatSnapshot(0L);
    }

    public AppStatSnapshot currentAppStatSnapshot(long windowMillis) {
        try {
            TimeBreaker.TimePortions timePortions = TimeBreaker.configurePortions(mStampList, windowMillis, 10L, new TimeBreaker.Stamp.Stamper() {
                @Override
                public TimeBreaker.Stamp stamp(String key) {
                    int appStat = BatteryCanaryUtil.getAppStat(mCore.getContext(), mCore.isForeground());
                    return new TimeBreaker.Stamp(String.valueOf(appStat));
                }
            });
            AppStatSnapshot snapshot = new AppStatSnapshot();
            snapshot.setValid(timePortions.isValid());
            snapshot.uptime = Snapshot.Entry.DigitEntry.of(timePortions.totalUptime);
            snapshot.fgRatio = Snapshot.Entry.DigitEntry.of((long) timePortions.getRatio(String.valueOf(APP_STAT_FOREGROUND)));
            snapshot.bgRatio = Snapshot.Entry.DigitEntry.of((long) timePortions.getRatio(String.valueOf(APP_STAT_BACKGROUND)));
            snapshot.fgSrvRatio = Snapshot.Entry.DigitEntry.of((long) timePortions.getRatio(String.valueOf(APP_STAT_FOREGROUND_SERVICE)));
            snapshot.floatRatio = Snapshot.Entry.DigitEntry.of((long) timePortions.getRatio(String.valueOf(APP_STAT_FLOAT_WINDOW)));
            return snapshot;

        } catch (Throwable e) {
            MatrixLog.w(TAG, "configureSnapshot fail: " + e.getMessage());
            AppStatSnapshot snapshot = new AppStatSnapshot();
            snapshot.setValid(false);
            return snapshot;
        }
    }

    public TimeBreaker.TimePortions currentSceneSnapshot() {
        return currentSceneSnapshot(0L);
    }

    public TimeBreaker.TimePortions currentSceneSnapshot(long windowMillis) {
        try {
            return TimeBreaker.configurePortions(mSceneStampList, windowMillis, 10L, new TimeBreaker.Stamp.Stamper() {
                @Override
                public TimeBreaker.Stamp stamp(String key) {
                    return new TimeBreaker.Stamp(mCore.getScene());
                }
            });
        } catch (Throwable e) {
            MatrixLog.w(TAG, "currentSceneSnapshot fail: " + e.getMessage());
            return TimeBreaker.TimePortions.ofInvalid();
        }
    }

    @NonNull
    public List<TimeBreaker.Stamp> getAppStatStampList() {
        if (mStampList.isEmpty()) return Collections.emptyList();
        return new ArrayList<>(mStampList);
    }

    @NonNull
    public List<TimeBreaker.Stamp> getSceneStampList() {
        if (mSceneStampList.isEmpty()) return Collections.emptyList();
        return new ArrayList<>(mSceneStampList);
    }


    // @VisibleForTesting
    // static final class AppStatStamp extends TimeBreaker.Stamp {
    //     public AppStatStamp(int appStat) {
    //         super(String.valueOf(appStat));
    //     }
    // }

    public static final class AppStatSnapshot extends Snapshot<AppStatSnapshot> {
        public Entry.DigitEntry<Long> uptime = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> fgRatio = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> bgRatio = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> fgSrvRatio = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> floatRatio = Entry.DigitEntry.of(0L);

        AppStatSnapshot() {
        }

        @Override
        public Delta<AppStatSnapshot> diff(AppStatSnapshot bgn) {
            return new Delta<AppStatSnapshot>(bgn, this) {
                @Override
                protected AppStatSnapshot computeDelta() {
                    AppStatSnapshot delta = new AppStatSnapshot();
                    delta.uptime = Differ.DigitDiffer.globalDiff(bgn.uptime, end.uptime);
                    delta.fgRatio = Differ.DigitDiffer.globalDiff(bgn.fgRatio, end.fgRatio);
                    delta.bgRatio = Differ.DigitDiffer.globalDiff(bgn.bgRatio, end.bgRatio);
                    delta.fgSrvRatio = Differ.DigitDiffer.globalDiff(bgn.fgSrvRatio, end.fgSrvRatio);
                    return delta;
                }
            };
        }
    }
}
