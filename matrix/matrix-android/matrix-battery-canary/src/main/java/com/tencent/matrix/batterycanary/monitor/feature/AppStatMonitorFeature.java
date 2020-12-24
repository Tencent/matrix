package com.tencent.matrix.batterycanary.monitor.feature;

import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.Context;
import android.support.annotation.NonNull;
import android.support.annotation.VisibleForTesting;
import android.support.annotation.WorkerThread;
import android.text.TextUtils;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.TimeBreaker;
import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * @author Kaede
 * @since 2020/12/8
 */
public final class AppStatMonitorFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.AppStatMonitorFeature";

    public interface AppStatListener {
        void onForegroundServiceLeak(int appImportance, int globalAppImportance, ComponentName componentName, boolean isMyself);
    }

    /**
     * Less important than {@link ActivityManager.RunningAppProcessInfo.IMPORTANCE_GONE}
     */
    @SuppressWarnings("JavadocReference")
    public static final int IMPORTANCE_LEAST = 1024;

    int mAppImportance = IMPORTANCE_LEAST;
    int mGlobalAppImportance = IMPORTANCE_LEAST;
    int mForegroundServiceImportanceLimit = ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND;

    @NonNull List<AppStatStamp> mStampList = Collections.emptyList();
    @NonNull List<TimeBreaker.Stamp> mSceneStampList = Collections.emptyList();
    @NonNull Runnable coolingTask = new Runnable() {
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

    @Override
    public void configure(BatteryMonitorCore monitor) {
        super.configure(monitor);
        mForegroundServiceImportanceLimit = Math.max(monitor.getConfig().foregroundServiceLeakLimit, ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND);
    }

    @Override
    public void onTurnOn() {
        super.onTurnOn();
        AppStatStamp firstStamp = new AppStatStamp(1);
        TimeBreaker.Stamp firstSceneStamp = new TimeBreaker.Stamp(mCore.getScene());
        synchronized (TAG) {
            mStampList = new ArrayList<>();
            mStampList.add(0, firstStamp);
            mSceneStampList = new ArrayList<>();
            mSceneStampList.add(0, firstSceneStamp);
        }
    }

    @Override
    public void onTurnOff() {
        super.onTurnOff();
        synchronized (TAG) {
            mStampList.clear();
            mSceneStampList.clear();
        }
    }

    @Override
    public void onForeground(boolean isForeground) {
        super.onForeground(isForeground);
        int appStat = BatteryCanaryUtil.getAppStat(mCore.getContext(), isForeground);
        synchronized (TAG) {
            if (mStampList != Collections.EMPTY_LIST) {
                mStampList.add(0, new AppStatStamp(appStat));
                checkOverHeat();
            }
        }

        MatrixLog.i(TAG, "updateAppImportance when app " + (isForeground ? "foreground" : "background"));
        updateAppImportance();
    }

    @WorkerThread
    @Override
    public void onBackgroundCheck(long duringMillis) {
        super.onBackgroundCheck(duringMillis);
        if (mGlobalAppImportance > mForegroundServiceImportanceLimit || mAppImportance > mForegroundServiceImportanceLimit) {
            Context context = mCore.getContext();
            ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
            if (am == null) {
                return;
            }

            for (ActivityManager.RunningServiceInfo item : am.getRunningServices(Integer.MAX_VALUE)) {
                if (!TextUtils.isEmpty(item.process) && item.process.startsWith(context.getPackageName())) {
                    if (item.foreground) {
                        // foreground service is running when app importance is low
                        if (mGlobalAppImportance > mForegroundServiceImportanceLimit) {
                            // global
                            MatrixLog.w(TAG, "foreground service detected with low global importance: "
                                    + mAppImportance + ", " + mGlobalAppImportance + ", " + item.service);
                            mCore.onForegroundServiceLeak(mAppImportance, mGlobalAppImportance, item.service, false);
                        }

                        if (mAppImportance > mForegroundServiceImportanceLimit) {
                            if (item.process.equals(context.getPackageName())) {
                                // myself
                                MatrixLog.w(TAG, "foreground service detected with low app importance: "
                                        + mAppImportance + ", " + mGlobalAppImportance + ", " + item.service);
                                mCore.onForegroundServiceLeak(mAppImportance, mGlobalAppImportance, item.service, true);
                            }
                        }
                    }
                }
            }
        }
    }

    public void onStatScene(@NonNull String scene) {
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

        mCore.getHandler().post(new Runnable() {
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

                for (ActivityManager.RunningAppProcessInfo item : am.getRunningAppProcesses()) {
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
        });
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
            int appStat = BatteryCanaryUtil.getAppStat(mCore.getContext(), mCore.isForeground());
            AppStatStamp lastStamp = new AppStatStamp(appStat);
            synchronized (TAG) {
                if (mStampList != Collections.EMPTY_LIST) {
                    mStampList.add(0, lastStamp);
                }
            }
            return configureSnapshot(mStampList, windowMillis);
        } catch (Throwable e) {
            MatrixLog.w(TAG, "configureSnapshot fail: " + e.getMessage());
            AppStatSnapshot snapshot = new AppStatSnapshot();
            snapshot.setValid(false);
            return snapshot;
        }
    }

    @VisibleForTesting
    static AppStatSnapshot configureSnapshot(List<AppStatStamp> stampList, long windowMillis) {
        TimeBreaker.TimePortions timePortions = TimeBreaker.configurePortions(stampList, windowMillis);
        AppStatSnapshot snapshot = new AppStatSnapshot();
        snapshot.setValid(timePortions.isValid());
        snapshot.uptime = Snapshot.Entry.DigitEntry.of(timePortions.totalUptime);
        snapshot.fgRatio = Snapshot.Entry.DigitEntry.of((long) timePortions.getRatio("1"));
        snapshot.bgRatio = Snapshot.Entry.DigitEntry.of((long) timePortions.getRatio("2"));
        snapshot.fgSrvRatio = Snapshot.Entry.DigitEntry.of((long) timePortions.getRatio("3"));
        return snapshot;
    }

    @VisibleForTesting
    static final class AppStatStamp extends TimeBreaker.Stamp {
        public AppStatStamp(int appStat) {
            super(String.valueOf(appStat));
        }
    }

    public static final class AppStatSnapshot extends Snapshot<AppStatSnapshot> {
        public Entry.DigitEntry<Long> uptime = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> fgRatio = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> bgRatio = Entry.DigitEntry.of(0L);
        public Entry.DigitEntry<Long> fgSrvRatio = Entry.DigitEntry.of(0L);

        AppStatSnapshot() {}

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

    public TimeBreaker.TimePortions currentSceneSnapshot() {
        return currentSceneSnapshot(0L);
    }

    public TimeBreaker.TimePortions currentSceneSnapshot(long windowMillis) {
        try {
            TimeBreaker.Stamp lastSceneStamp = new TimeBreaker.Stamp(mCore.getScene());
            synchronized (TAG) {
                if (mSceneStampList != Collections.EMPTY_LIST) {
                    mSceneStampList.add(0, lastSceneStamp);
                }
            }
            return configureSceneSnapshot(mSceneStampList, windowMillis);
        } catch (Throwable e) {
            MatrixLog.w(TAG, "currentSceneSnapshot fail: " + e.getMessage());
            return TimeBreaker.TimePortions.ofInvalid();
        }
    }

    @VisibleForTesting
    static TimeBreaker.TimePortions configureSceneSnapshot(List<TimeBreaker.Stamp> stampList, long windowMillis) {
        return TimeBreaker.configurePortions(stampList, windowMillis);
    }
}
