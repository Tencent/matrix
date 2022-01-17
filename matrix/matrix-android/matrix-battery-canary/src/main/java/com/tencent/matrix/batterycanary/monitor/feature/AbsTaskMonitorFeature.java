package com.tencent.matrix.batterycanary.monitor.feature;


import android.annotation.SuppressLint;
import android.os.Looper;
import android.os.Process;
import android.os.SystemClock;
import android.util.Pair;
import android.util.SparseArray;

import androidx.annotation.AnyThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.WorkerThread;

import com.tencent.matrix.batterycanary.BuildConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.DigitEntry;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.ProcStatUtil;
import com.tencent.matrix.batterycanary.utils.TimeBreaker;
import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.ConcurrentHashMap;

import static com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig.DEF_STAMP_OVERHEAT;

@SuppressWarnings("NotNullFieldNotInitialized")
public abstract class AbsTaskMonitorFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.AbsTaskMonitorFeature";
    private static final long INITIAL_JIFFIES = 0L;
    private static final long JIFFIES_PORTIONING_DELTA = 10L;

    public static final String IDLE_TASK = "thread_pool@idle";

    @NonNull
    final protected List<Delta<TaskJiffiesSnapshot>> mDeltaList = new ArrayList<>();
    @NonNull
    final protected Map<Integer, TaskJiffiesSnapshot> mTaskJiffiesTrace = new ConcurrentHashMap<>();
    @NonNull
    final protected Map<String, Pair<? extends List<Integer>, Long>> mTaskConcurrentTrace = new ConcurrentHashMap<>();
    @NonNull
    final protected SparseArray<List<TimeBreaker.Stamp>> mTaskStampList = new SparseArray<>();
    @NonNull
    protected TimeBreaker.Stamp mFirstTaskStamp;

    @Nullable
    protected AppStatMonitorFeature mAppStatFeat;
    @Nullable
    protected DeviceStatMonitorFeature mDevStatFeat;

    protected int mOverHeatCount = DEF_STAMP_OVERHEAT;
    protected int mConcurrentLimit = 50;

    @NonNull
    protected Runnable coolingTask = new Runnable() {
        @SuppressLint("RestrictedApi")
        @Override
        public void run() {
            onCoolingDown();
        }
    };

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    public void configure(BatteryMonitorCore monitor) {
        super.configure(monitor);
        mAppStatFeat = monitor.getMonitorFeature(AppStatMonitorFeature.class);
        mDevStatFeat = monitor.getMonitorFeature(DeviceStatMonitorFeature.class);
        mFirstTaskStamp = new TimeBreaker.Stamp(IDLE_TASK, INITIAL_JIFFIES);
        mOverHeatCount = Math.max(monitor.getConfig().overHeatCount, mOverHeatCount);
    }

    @Override
    public void onTurnOn() {
        super.onTurnOn();
    }

    @Override
    public void onTurnOff() {
        super.onTurnOff();
        mTaskJiffiesTrace.clear();
        synchronized (mTaskConcurrentTrace) {
            mTaskConcurrentTrace.clear();
        }
        synchronized (mDeltaList) {
            mDeltaList.clear();
        }
        synchronized (mTaskStampList) {
            mTaskStampList.clear();
        }
    }

    public List<Delta<TaskJiffiesSnapshot>> currentJiffies() {
        // Report unfinished task:
        // Maybe cause duplicated reporting
        // for (TaskJiffiesSnapshot bgn : mTaskJiffiesTrace.values()) {
        //     TaskJiffiesSnapshot end = TaskJiffiesSnapshot.of(bgn.name, bgn.tid);
        //     if (end != null) {
        //         updateDeltas(bgn, end);
        //     }
        // }

        ArrayList<Delta<TaskJiffiesSnapshot>> list;
        synchronized (mDeltaList) {
            list = new ArrayList<>(mDeltaList);
        }

        // Sorting by jiffies dec
        Collections.sort(list, new Comparator<Delta<TaskJiffiesSnapshot>>() {
            @Override
            public int compare(Delta<TaskJiffiesSnapshot> o1, Delta<TaskJiffiesSnapshot> o2) {
                if (o1.dlt == null || o2.dlt == null) {
                    String message = "delta should not be null: " + o1 + " vs " + o2;
                    if (BuildConfig.DEBUG) {
                        throw new RuntimeException(message);
                    }
                    MatrixLog.w(TAG, message);
                    return 0;
                }
                TaskJiffiesSnapshot left = o1.dlt;
                TaskJiffiesSnapshot right = o2.dlt;
                if (left.appStat != 1 || right.appStat != 1) {
                    if (left.appStat == 1) {
                        return 1;
                    }
                    if (right.appStat == 1) {
                        return -1;
                    }
                }
                long minus = left.jiffies.get() - right.jiffies.get();
                if (minus == 0) return 0;
                if (minus > 0) return -1;
                return 1;
            }
        });
        return list;
    }

    public void clearFinishedJiffies() {
        synchronized (mDeltaList) {
            // Iterator<Delta<TaskJiffiesSnapshot>> iterator = mDeltaList.iterator();
            // while (iterator.hasNext()) {
            //     Delta<TaskJiffiesSnapshot> item = iterator.next();
            //     if (item.dlt.isFinished) {
            //         iterator.remove();
            //     }
            // }
            mDeltaList.clear();
        }
    }

    @Nullable
    public ArrayList<TimeBreaker.Stamp> getTaskStamps(int tid) {
        synchronized (mTaskStampList) {
            List<TimeBreaker.Stamp> stamps = mTaskStampList.get(tid);
            if (stamps != null) {
                return new ArrayList<>(stamps);
            }
            return null;
        }
    }

    @SuppressLint("RestrictedApi")
    public TimeBreaker.TimePortions getTaskPortions(int tid, long jiffiesDelta, final long jiffiesEnd) {
        synchronized (mTaskStampList) {
            if (jiffiesDelta < 0L || mTaskStampList.get(tid) == null) {
                return TimeBreaker.TimePortions.ofInvalid();
            }

            List<TimeBreaker.Stamp> stampList = mTaskStampList.get(tid);
            return TimeBreaker.configurePortions(stampList, jiffiesDelta, JIFFIES_PORTIONING_DELTA, new TimeBreaker.Stamp.Stamper() {
                @Override
                public TimeBreaker.Stamp stamp(String name) {
                    return new TimeBreaker.Stamp(name, jiffiesEnd);
                }
            });
        }
    }

    @WorkerThread
    protected void onTaskStarted(final String key, final int hashcode) {
        if (Looper.myLooper() == Looper.getMainLooper()) return;

        // Trace task jiffies
        final TaskJiffiesSnapshot bgn = createSnapshot(key, Process.myTid());
        if (bgn != null) {
            mTaskJiffiesTrace.put(hashcode, bgn);
            // Update task stamp list
            onStatTask(Process.myTid(), key, bgn.jiffies.get());
        }

        // Trace task concurrent count +
        // onTaskConcurrentInc(key, hashcode);
    }

    @WorkerThread
    protected void onTaskFinished(final String key, final int hashcode) {
        final TaskJiffiesSnapshot bgn = mTaskJiffiesTrace.remove(hashcode);
        // Trace task concurrent count -
        // onTaskConcurrentDec(hashcode);

        if (Looper.myLooper() != Looper.getMainLooper() && bgn != null) {
            TaskJiffiesSnapshot end = createSnapshot(key, Process.myTid());
            if (end != null) {
                end.isFinished = true;
                updateDeltas(bgn, end);
            }
            // Update task stamp list
            onStatTask(Process.myTid(), IDLE_TASK,
                    end == null ? bgn.jiffies.get() : end.jiffies.get());
        }
    }

    @AnyThread
    protected void onTaskRemoved(final int hashcode) {
        mTaskJiffiesTrace.remove(hashcode);
        // Trace task concurrent count -
        // onTaskConcurrentDec(hashcode);
    }

    protected void onTaskConcurrentInc(final String key, final int hashcode) {
        mCore.getHandler().post(new Runnable() {
            @Override
            public void run() {
                Pair<? extends List<Integer>, Long> workingTasks;
                synchronized (mTaskConcurrentTrace) {
                    workingTasks = mTaskConcurrentTrace.get(key);
                    if (workingTasks == null) {
                        workingTasks = new Pair<>(new LinkedList<Integer>(), SystemClock.uptimeMillis());
                    }
                    //noinspection ConstantConditions
                    workingTasks.first.add(hashcode);
                    mTaskConcurrentTrace.put(key, workingTasks);
                }

                if (workingTasks.first.size() > mConcurrentLimit) {
                    MatrixLog.w(TAG,
                            "reach task concurrent limit, count = " + workingTasks.first.size()
                                    + ", key = " + key);
                    //noinspection ConstantConditions
                    long duringMillis = SystemClock.uptimeMillis() - workingTasks.second;
                    MatrixLog.w(TAG, "onConcurrentOverHeat, during = " + duringMillis);
                    onConcurrentOverHeat(key, workingTasks.first.size(), duringMillis);
                }
            }
        });
    }

    protected void onTaskConcurrentDec(final int hashcode) {
        mCore.getHandler().post(new Runnable() {
            @SuppressWarnings("ConstantConditions")
            @Override
            public void run() {
                synchronized (mTaskConcurrentTrace) {
                    boolean found = false;
                    for (Iterator<Map.Entry<String, Pair<? extends List<Integer>, Long>>> entryIterator = mTaskConcurrentTrace.entrySet().iterator(); entryIterator.hasNext(); ) {
                        Map.Entry<String, Pair<? extends List<Integer>, Long>> entry = entryIterator.next();
                        for (Iterator<Integer> iterator = entry.getValue().first.iterator(); iterator.hasNext(); ) {
                            Integer item = iterator.next();
                            if (item == hashcode) {
                                iterator.remove();
                                found = true;
                                break;
                            }
                        }
                        if (entry.getValue().first.isEmpty()) {
                            entryIterator.remove();
                        }
                        if (found) {
                            break;
                        }
                    }
                }
            }
        });
    }

    protected void onStatTask(int tid, @NonNull String taskName, long currJiffies) {
        synchronized (mTaskStampList) {
            List<TimeBreaker.Stamp> stampList = mTaskStampList.get(tid);
            if (stampList == null) {
                stampList = new ArrayList<>();
                stampList.add(0, mFirstTaskStamp);
                mTaskStampList.put(tid, stampList);
            }
            stampList.add(0, new TimeBreaker.Stamp(taskName, currJiffies));
        }

        checkOverHeat();
    }

    protected void updateDeltas(TaskJiffiesSnapshot bgn, TaskJiffiesSnapshot end) {
        if (end.tid != bgn.tid) {
            String message = "task tid mismatch: " + bgn + " vs " + end;
            if (BuildConfig.DEBUG) {
                throw new RuntimeException(message);
            }
            MatrixLog.w(TAG, message);
            return;
        }
        if (!end.name.equals(bgn.name)) {
            String message = "task name mismatch: " + bgn + " vs " + end;
            if (BuildConfig.DEBUG) {
                throw new RuntimeException(message);
            }
            MatrixLog.w(TAG, message);
            return;
        }

        // Compute task jiffies consumed
        Delta<TaskJiffiesSnapshot> delta = end.diff(bgn);
        if (!shouldTraceTask(delta)) {
            return;
        }

        MatrixLog.i(TAG, "onTaskReport: %s, jiffies = %s, millis = %s", delta.dlt.name, delta.dlt.jiffies.get(), delta.during);

        // Compute task context info
        if (mAppStatFeat != null) {
            AppStatMonitorFeature.AppStatSnapshot appStats = mAppStatFeat.currentAppStatSnapshot(delta.during);
            if (!appStats.isValid()) {
                delta.end.setValid(false);
                delta.dlt.setValid(false);
            }
            String scene = delta.dlt.scene;
            long sceneRatio = 100;
            TimeBreaker.TimePortions portions = mAppStatFeat.currentSceneSnapshot(delta.during);
            TimeBreaker.TimePortions.Portion top1 = portions.top1();
            if (top1 != null) {
                scene = top1.key;
                sceneRatio = top1.ratio;
            }
            delta.dlt.bgRatio = appStats.bgRatio.get();
            delta.dlt.scene = scene;
            delta.dlt.sceneRatio = sceneRatio;
        }
        if (mDevStatFeat != null) {
            DeviceStatMonitorFeature.DevStatSnapshot devStat = mDevStatFeat.currentDevStatSnapshot(delta.during);
            if (!devStat.isValid()) {
                delta.end.setValid(false);
                delta.dlt.setValid(false);
            }
            delta.dlt.chargeRatio = devStat.chargingRatio.get();
        }

        // Update records
        updateDeltas(delta);

        // Cooling down if need
        if (mDeltaList.size() >= mOverHeatCount) {
            MatrixLog.w(TAG, "task list overheat, size = " + mDeltaList.size());
            checkOverHeat();
        }
    }

    protected boolean shouldTraceTask(Delta<TaskJiffiesSnapshot> delta) {
        return BuildConfig.DEBUG || (delta.during > 1000L
                && delta.dlt.jiffies.get() / Math.max(1, delta.during / BatteryCanaryUtil.ONE_MIN)
                > 100L);
    }

    protected void updateDeltas(Delta<TaskJiffiesSnapshot> delta) {
        synchronized (mDeltaList) {
            // remove pre records of current task
            Iterator<Delta<TaskJiffiesSnapshot>> iterator = mDeltaList.iterator();
            while (iterator.hasNext()) {
                Delta<TaskJiffiesSnapshot> item = iterator.next();
                if (item.dlt.name.equals(delta.dlt.name) && item.dlt.tid == delta.dlt.tid) {
                    if (!item.dlt.isFinished) {
                        iterator.remove();
                    }
                }
            }
            mDeltaList.add(delta);
        }
    }

    protected void checkOverHeat() {
        mCore.getHandler().removeCallbacks(coolingTask);
        mCore.getHandler().postDelayed(coolingTask, 1000L);
    }

    protected void onCoolingDown() {
        // task stamp list overheat
        synchronized (mTaskStampList) {
            for (int i = 0; i < mTaskStampList.size(); i++) {
                List<TimeBreaker.Stamp> stampList = mTaskStampList.valueAt(i);
                if (stampList != null && stampList.size() > mOverHeatCount) {
                    TimeBreaker.gcList(stampList);
                }
            }
        }

        // task jiffies list overheat
        if (mDeltaList.size() > mOverHeatCount) {
            MatrixLog.w(TAG, "cooling task jiffies list, before = " + mDeltaList.size());
            List<Delta<TaskJiffiesSnapshot>> deltas = currentJiffies();
            clearFinishedJiffies();
            MatrixLog.w(TAG, "cooling task jiffies list, after = " + mDeltaList.size());

            // report
            MatrixLog.w(TAG, "report task jiffies list overheat");
            onTraceOverHeat(deltas);
        }
    }

    protected void onTraceOverHeat(List<Delta<TaskJiffiesSnapshot>> deltas) {
    }

    protected void onConcurrentOverHeat(String key, int concurrentCount, long duringMillis) {
    }

    protected void onParseTaskJiffiesFail(String key, int pid, int tid) {
    }

    @Nullable
    protected TaskJiffiesSnapshot createSnapshot(String name, int tid) {
        TaskJiffiesSnapshot snapshot = new TaskJiffiesSnapshot();
        snapshot.tid = tid;
        snapshot.name = name;

        // FIXME: perf opt needed via devStat & appStat caching
        snapshot.appStat = BatteryCanaryUtil.getAppStat(mCore.getContext(), mCore.isForeground());
        snapshot.devStat = BatteryCanaryUtil.getDeviceStat(mCore.getContext());

        try {
            Callable<String> supplier = mCore.getConfig().onSceneSupplier;
            snapshot.scene = supplier == null ? "" : supplier.call();
        } catch (Exception ignored) {
            snapshot.scene = "";
        }

        if (mCore.getConfig().isUseThreadClock) {
            snapshot.jiffies = DigitEntry.of(SystemClock.currentThreadTimeMillis() / BatteryCanaryUtil.JIFFY_MILLIS);
        } else {
            int pid = Process.myPid();
            ProcStatUtil.ProcStat stat = ProcStatUtil.of(pid, tid);
            if (stat == null) {
                MatrixLog.w(TAG, "parse task procStat fail, name = " + name + ", tid = " + tid);
                onParseTaskJiffiesFail(name, pid, tid);
                return null;
            }
            snapshot.jiffies = DigitEntry.of(stat.getJiffies());
            return snapshot;
        }

        return snapshot;
    }

    public static class TaskJiffiesSnapshot extends Snapshot<TaskJiffiesSnapshot> {
        public int tid;
        public String name;
        public long timeMillis = System.currentTimeMillis();
        public DigitEntry<Long> jiffies;
        public int appStat;
        public int devStat;
        public String scene;
        public boolean isFinished = false;
        public long bgRatio = 100;
        public long chargeRatio = 100;
        public long sceneRatio = 100;

        public TaskJiffiesSnapshot() {
        }

        @Override
        public Delta<TaskJiffiesSnapshot> diff(TaskJiffiesSnapshot bgn) {
            return new Delta<TaskJiffiesSnapshot>(bgn, this) {
                @Override
                protected TaskJiffiesSnapshot computeDelta() {
                    TaskJiffiesSnapshot delta = new TaskJiffiesSnapshot();
                    delta.tid = end.tid;
                    delta.name = end.name;
                    delta.timeMillis = end.timeMillis - bgn.timeMillis;
                    delta.jiffies = Differ.DigitDiffer.globalDiff(bgn.jiffies, end.jiffies);
                    delta.isFinished = end.isFinished;

                    if (bgn.appStat == 1 || end.appStat == 1) {
                        delta.appStat = 1; // 前台
                    } else if (bgn.appStat == 3 && end.appStat == 3) {
                        delta.appStat = 3;
                    } else {
                        delta.appStat = 2;
                    }

                    if (bgn.devStat == 1 || end.devStat == 1) {
                        delta.devStat = 1; // 充电中
                    } else if (bgn.devStat == 3 && end.devStat == 3) {
                        delta.devStat = 3;
                    } else if (bgn.devStat == 4 && end.devStat == 4) {
                        delta.devStat = 3;
                    } else {
                        delta.devStat = 2;
                    }

                    delta.scene = end.scene;
                    return delta;
                }
            };
        }

        @Override
        public String toString() {
            return "TaskJiffiesSnapshot{"
                    + "appStat=" + appStat
                    + ", devStat=" + devStat
                    + ", tid=" + tid
                    + ", name='" + name + '\''
                    + ", jiffies=" + jiffies
                    + '}';
        }
    }
}
