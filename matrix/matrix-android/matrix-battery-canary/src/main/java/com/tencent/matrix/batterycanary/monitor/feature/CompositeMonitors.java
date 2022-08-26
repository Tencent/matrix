package com.tencent.matrix.batterycanary.monitor.feature;

import android.annotation.SuppressLint;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.TextUtils;

import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.AbsTaskMonitorFeature.TaskJiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot.ThreadJiffiesEntry;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.DigitEntry;
import com.tencent.matrix.batterycanary.stats.HealthStatsFeature;
import com.tencent.matrix.batterycanary.stats.HealthStatsFeature.HealthStatsSnapshot;
import com.tencent.matrix.batterycanary.stats.HealthStatsHelper;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.Consumer;
import com.tencent.matrix.batterycanary.utils.Function;
import com.tencent.matrix.batterycanary.utils.PowerProfile;
import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.util.Pair;

/**
 * @author Kaede
 * @since 2021/9/18
 */
public class CompositeMonitors {
    private static final String TAG = "Matrix.battery.CompositeMonitors";

    public static final String SCOPE_UNKNOWN = "unknown";
    public static final String SCOPE_CANARY = "canary";
    public static final String SCOPE_INTERNAL = "internal";
    public static final String SCOPE_OVERHEAT = "overheat";

    // Differing
    protected final List<Class<? extends Snapshot<?>>> mMetrics = new ArrayList<>();
    protected final Map<Class<? extends Snapshot<?>>, Snapshot<?>> mBgnSnapshots = new HashMap<>();
    protected final Map<Class<? extends Snapshot<?>>, Delta<?>> mDeltas = new HashMap<>();

    // Sampling
    protected final Map<Class<? extends Snapshot<?>>, Long> mSampleRegs = new HashMap<>();
    protected final Map<Class<? extends Snapshot<?>>, Snapshot.Sampler> mSamplers = new HashMap<>();
    protected final Map<Class<? extends Snapshot<?>>, Snapshot.Sampler.Result> mSampleResults = new HashMap<>();

    // Task Tracing
    protected final Map<Class<? extends AbsTaskMonitorFeature>, List<Delta<TaskJiffiesSnapshot>>> mTaskDeltas = new HashMap<>();
    protected final Map<String, List<Pair<Class<? extends AbsTaskMonitorFeature>, Delta<TaskJiffiesSnapshot>>>> mTaskDeltasCollect = new HashMap<>();

    // Extra Info
    protected final Bundle mExtras = new Bundle();

    // Call Stacks
    protected final Map<String, String> mStacks = new HashMap<>();

    @Nullable
    protected BatteryMonitorCore mMonitor;
    @Nullable
    protected AppStats mAppStats;
    @Nullable
    protected CpuFreqSampler mCpuFreqSampler;
    @Nullable
    protected BpsSampler mBpsSampler;

    protected long mBgnMillis = SystemClock.uptimeMillis();
    protected String mScope;

    public CompositeMonitors(@Nullable BatteryMonitorCore core) {
        mMonitor = core;
        mScope = SCOPE_UNKNOWN;
    }

    public CompositeMonitors(@Nullable BatteryMonitorCore core, String scope) {
        mMonitor = core;
        mScope = scope;
    }

    public String getScope() {
        return mScope;
    }

    @CallSuper
    public void clear() {
        MatrixLog.i(TAG, hashCode() + " #clear: " + mScope);
        mBgnSnapshots.clear();
        mDeltas.clear();
        mSamplers.clear();
        mSampleResults.clear();
        mTaskDeltas.clear();
        mTaskDeltasCollect.clear();
        mExtras.clear();
        mStacks.clear();
        mCpuFreqSampler = null;
    }

    public CompositeMonitors fork() {
        return fork(new CompositeMonitors(mMonitor, mScope));
    }

    @CallSuper
    protected CompositeMonitors fork(CompositeMonitors that) {
        MatrixLog.i(TAG, hashCode() + " #fork: " + mScope);
        that.clear();
        that.mBgnMillis = this.mBgnMillis;
        that.mAppStats = this.mAppStats;

        that.mMetrics.addAll(mMetrics);
        that.mBgnSnapshots.putAll(mBgnSnapshots);
        that.mDeltas.putAll(mDeltas);

        // Sampler can not be cloned.
        // that.mSampleRegs.putAll(mSampleRegs);
        // that.mSamplers.putAll(mSamplers);
        // that.mSampleResults.putAll(mSampleResults);

        that.mTaskDeltas.putAll(this.mTaskDeltas);
        that.mTaskDeltasCollect.putAll(this.mTaskDeltasCollect);
        that.mExtras.putAll(this.mExtras);
        that.mStacks.putAll(this.mStacks);
        that.mCpuFreqSampler = this.mCpuFreqSampler;
        return that;
    }

    @Nullable
    public BatteryMonitorCore getMonitor() {
        return mMonitor;
    }

    @Nullable
    public <T extends MonitorFeature> T getFeature(Class<T> clazz) {
        if (mMonitor == null) {
            return null;
        }
        for (MonitorFeature plugin : mMonitor.getConfig().features) {
            if (clazz.isAssignableFrom(plugin.getClass())) {
                //noinspection unchecked
                return (T) plugin;
            }
        }
        return null;
    }

    public <T extends MonitorFeature> void getFeature(Class<T> clazz, Consumer<T> block) {
        T feature = getFeature(clazz);
        if (feature != null) {
            block.accept(feature);
        }
    }

    @Nullable
    public AppStats getAppStats() {
        return mAppStats;
    }


    public void getAppStats(Consumer<AppStats> block) {
        AppStats appStats = getAppStats();
        if (appStats != null) {
            block.accept(appStats);
        }
    }

    public void setAppStats(@Nullable AppStats appStats) {
        mAppStats = appStats;
    }

    public int getCpuLoad() {
        if (mAppStats == null) {
            MatrixLog.w(TAG, "AppStats should not be null to get CpuLoad");
            return -1;
        }
        long appJiffiesDelta;
        Delta<JiffiesMonitorFeature.UidJiffiesSnapshot> uidJiffies = getDelta(JiffiesMonitorFeature.UidJiffiesSnapshot.class);
        if (uidJiffies != null) {
            appJiffiesDelta = uidJiffies.dlt.totalUidJiffies.get();
        } else {
            Delta<JiffiesSnapshot> pidJiffies = getDelta(JiffiesSnapshot.class);
            if (pidJiffies == null) {
                MatrixLog.w(TAG, JiffiesSnapshot.class + " should be metrics to get CpuLoad");
                return -1;
            }
            appJiffiesDelta = pidJiffies.dlt.totalJiffies.get();
        }

        long cpuUptimeDelta = mAppStats.duringMillis;
        float cpuLoad = cpuUptimeDelta > 0 ? (float) (appJiffiesDelta * 10) / cpuUptimeDelta : 0;
        return (int) (cpuLoad * 100);
    }

    public int getNorCpuLoad() {
        int cpuLoad = getCpuLoad();
        if (cpuLoad == -1) {
            MatrixLog.w(TAG, "cpu is invalid");
            return -1;
        }
        MonitorFeature.Snapshot.Sampler.Result result = getSamplingResult(DeviceStatMonitorFeature.CpuFreqSnapshot.class);
        if (result == null) {
            MatrixLog.w(TAG, "cpufreq is null");
            return -1;
        }
        List<int[]> cpuFreqSteps = BatteryCanaryUtil.getCpuFreqSteps();
        if (cpuFreqSteps.size() != BatteryCanaryUtil.getCpuCoreNum()) {
            MatrixLog.w(TAG, "cpuCore is invalid: " + cpuFreqSteps.size() + " vs " + BatteryCanaryUtil.getCpuCoreNum());
        }
        long sumMax = 0;
        for (int[] steps : cpuFreqSteps) {
            int max = 0;
            for (int item : steps) {
                if (item > max) {
                    max = item;
                }
            }
            sumMax += max;
        }
        if (sumMax <= 0) {
            MatrixLog.w(TAG, "cpufreq sum is invalid: " + sumMax);
            return -1;
        }
        if (result.sampleAvg >= sumMax) {
            // avgFreq should not greater than maxFreq
            MatrixLog.w(TAG, "NorCpuLoad err: sampling = " + result);
            for (int[] item : cpuFreqSteps) {
                MatrixLog.w(TAG, "NorCpuLoad err: freqs = " + Arrays.toString(item));
            }
        }
        return (int) (cpuLoad * result.sampleAvg / sumMax);
    }

    /**
     * Work in progress
     */
    public int getDevCpuLoad() {
        if (mAppStats == null) {
            MatrixLog.w(TAG, "AppStats should not be null to get CpuLoad");
            return -1;
        }
        Delta<CpuStatFeature.CpuStateSnapshot> cpuJiffies = getDelta(CpuStatFeature.CpuStateSnapshot.class);
        if (cpuJiffies == null) {
            MatrixLog.w(TAG, "Configure CpuLoad by uptime");
            return -1;
        }

        long cpuJiffiesDelta = cpuJiffies.dlt.totalCpuJiffies();
        long devJiffiesDelta = mAppStats.duringMillis;
        float cpuLoad = devJiffiesDelta > 0 ? (float) (cpuJiffiesDelta * 10) / devJiffiesDelta : 0;
        return (int) (cpuLoad * 100);
    }

    public long computeAvgJiffies(long jiffies) {
        if (mAppStats == null) {
            MatrixLog.w(TAG, "AppStats should not be null to computeAvgJiffies");
            return -1;
        }
        return computeAvgJiffies(jiffies, mAppStats.duringMillis);
    }

    public static long computeAvgJiffies(long jiffies, long millis) {
        if (millis <= 0) {
            throw new IllegalArgumentException("Illegal millis: " + millis);
        }
        return (long) (jiffies / (millis / 60000f));
    }

    public <T extends Snapshot<T>> boolean isOverHeat(Class<T> snapshotClass) {
        AppStats appStats = getAppStats();
        Delta<?> delta = getDelta(snapshotClass);
        if (appStats == null || delta == null) {
            return false;
        }
        if (snapshotClass == JiffiesSnapshot.class) {
            //noinspection unchecked
            Delta<JiffiesSnapshot> jiffiesDelta = (Delta<JiffiesSnapshot>) delta;
            long minute = appStats.getMinute();
            long avgJiffies = jiffiesDelta.dlt.totalJiffies.get() / minute;
            return minute >= 5 && avgJiffies >= 1000;
        }
        // Override by child
        return false;
    }

    @Nullable
    public <T extends Snapshot<T>> Delta<T> getDelta(Class<T> snapshotClass) {
        //noinspection unchecked
        return (Delta<T>) mDeltas.get(snapshotClass);
    }

    public <T extends Snapshot<T>> void getDelta(Class<T> snapshotClass, Consumer<Delta<T>> block) {
        Delta<T> delta = getDelta(snapshotClass);
        if (delta != null) {
            block.accept(delta);
        }
    }

    @Nullable
    public Delta<?> getDeltaRaw(Class<? extends Snapshot<?>> snapshotClass) {
        return mDeltas.get(snapshotClass);
    }

    public void getDeltaRaw(Class<? extends Snapshot<?>> snapshotClass, Consumer<Delta<?>> block) {
        Delta<?> delta = getDeltaRaw(snapshotClass);
        if (delta != null) {
            block.accept(delta);
        }
    }

    public void putDelta(Class<? extends Snapshot<?>> snapshotClass, Delta<? extends Snapshot<?>> delta) {
        mDeltas.put(snapshotClass, delta);
    }

    public Snapshot.Sampler.Result getSamplingResult(Class<? extends Snapshot<?>> snapshotClass) {
        return mSampleResults.get(snapshotClass);
    }

    public void getSamplingResult(Class<? extends Snapshot<?>> snapshotClass, Consumer<Snapshot.Sampler.Result> block) {
        Snapshot.Sampler.Result result = getSamplingResult(snapshotClass);
        if (result != null) {
            block.accept(result);
        }
    }

    @CallSuper
    public CompositeMonitors metricAll() {
        metric(JiffiesSnapshot.class);
        metric(AlarmMonitorFeature.AlarmSnapshot.class);
        metric(WakeLockMonitorFeature.WakeLockSnapshot.class);
        metric(CpuStatFeature.CpuStateSnapshot.class);

        metric(AppStatMonitorFeature.AppStatSnapshot.class);
        metric(DeviceStatMonitorFeature.CpuFreqSnapshot.class);
        metric(DeviceStatMonitorFeature.BatteryTmpSnapshot.class);

        metric(TrafficMonitorFeature.RadioStatSnapshot.class);
        metric(BlueToothMonitorFeature.BlueToothSnapshot.class);
        metric(WifiMonitorFeature.WifiSnapshot.class);
        metric(LocationMonitorFeature.LocationSnapshot.class);
        return this;
    }

    public CompositeMonitors metricCpuLoad() {
        if (!mMetrics.contains(JiffiesSnapshot.class)) {
            metric(JiffiesSnapshot.class);
        }
        if (!mMetrics.contains(CpuStatFeature.CpuStateSnapshot.class)) {
            metric(CpuStatFeature.CpuStateSnapshot.class);
        }
        return this;
    }

    public CompositeMonitors metric(Class<? extends Snapshot<?>> snapshotClass) {
        if (!mMetrics.contains(snapshotClass)) {
            mMetrics.add(snapshotClass);
        }
        return this;
    }

    public CompositeMonitors sample(Class<? extends Snapshot<?>> snapshotClass) {
        return sample(snapshotClass, BatteryCanaryUtil.ONE_MIN);
    }

    public CompositeMonitors sample(Class<? extends Snapshot<?>> snapshotClass, long interval) {
        mSampleRegs.put(snapshotClass, interval);
        return this;
    }

    public void start() {
        MatrixLog.i(TAG, hashCode() + " #start: " + mScope);
        mAppStats = null;
        mBgnMillis = SystemClock.uptimeMillis();
        configureBgnSnapshots();
        configureSamplers();
    }

    public void finish() {
        MatrixLog.i(TAG, hashCode() + " #finish: " + mScope);
        configureEndDeltas();
        collectStacks();
        configureSampleResults();
        mAppStats = AppStats.current(SystemClock.uptimeMillis() - mBgnMillis);

        // For further procedures
        polishEstimatedPower();
    }

    protected void configureBgnSnapshots() {
        for (Class<? extends Snapshot<?>> item : mMetrics) {
            Snapshot<?> currSnapshot = statCurrSnapshot(item);
            if (currSnapshot != null) {
                mBgnSnapshots.put(item, currSnapshot);
            }
        }
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    protected void configureEndDeltas() {
        for (Map.Entry<Class<? extends Snapshot<?>>, Snapshot<?>> item : mBgnSnapshots.entrySet()) {
            Snapshot lastSnapshot = item.getValue();
            if (lastSnapshot != null) {
                Class<? extends Snapshot<?>> snapshotClass = item.getKey();
                Snapshot currSnapshot = statCurrSnapshot(snapshotClass);
                if (currSnapshot != null && currSnapshot.getClass() == lastSnapshot.getClass()) {
                    putDelta(snapshotClass, currSnapshot.diff(lastSnapshot));
                }
            }
        }
    }

    protected void collectStacks() {
        if (mMonitor == null) {
            return;
        }
        // Figure out thread' stack if need
        if (SCOPE_CANARY.equals(getScope())) {
            // 待机功耗监控
            AppStats appStats = getAppStats();
            if (appStats != null && !appStats.isForeground()) {
                getDelta(JiffiesSnapshot.class, new Consumer<Delta<JiffiesSnapshot>>() {
                    @Override
                    public void accept(Delta<JiffiesSnapshot> delta) {
                        long minute = Math.max(1, delta.during / BatteryCanaryUtil.ONE_MIN);
                        if (minute < 5) {
                            return;
                        }
                        for (ThreadJiffiesEntry threadEntry : delta.dlt.threadEntries.getList()) {
                            long topThreadAvgJiffies = threadEntry.get() / minute;
                            if (topThreadAvgJiffies < 3000L) {
                                break;
                            }
                            String stack = mMonitor.getConfig().callStackCollector.collect(threadEntry.tid);
                            if (!TextUtils.isEmpty(stack)) {
                                mStacks.put(String.valueOf(threadEntry.tid), stack);
                            }
                        }
                    }
                });
            }
        }
    }

    @CallSuper
    protected Snapshot<?> statCurrSnapshot(Class<? extends Snapshot<?>> snapshotClass) {
        Snapshot<?> snapshot = null;
        if (snapshotClass == AlarmMonitorFeature.AlarmSnapshot.class) {
            AlarmMonitorFeature feature = getFeature(AlarmMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentAlarms();
            }
            return snapshot;
        }
        if (snapshotClass == BlueToothMonitorFeature.BlueToothSnapshot.class) {
            BlueToothMonitorFeature feature = getFeature(BlueToothMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentSnapshot();
            }
            return snapshot;
        }
        if (snapshotClass == DeviceStatMonitorFeature.CpuFreqSnapshot.class) {
            DeviceStatMonitorFeature feature = getFeature(DeviceStatMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentCpuFreq();
            }
            return snapshot;
        }
        if (snapshotClass == DeviceStatMonitorFeature.BatteryTmpSnapshot.class) {
            DeviceStatMonitorFeature feature = getFeature(DeviceStatMonitorFeature.class);
            if (feature != null && mMonitor != null) {
                snapshot = feature.currentBatteryTemperature(mMonitor.getContext());
            }
            return snapshot;
        }
        if (snapshotClass == JiffiesSnapshot.class) {
            JiffiesMonitorFeature feature = getFeature(JiffiesMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentJiffiesSnapshot();
            }
            return snapshot;
        }
        if (snapshotClass == JiffiesMonitorFeature.UidJiffiesSnapshot.class) {
            JiffiesMonitorFeature feat = getFeature(JiffiesMonitorFeature.class);
            if (feat != null) {
                return feat.currentUidJiffiesSnapshot();
            }
        }
        if (snapshotClass == LocationMonitorFeature.LocationSnapshot.class) {
            LocationMonitorFeature feature = getFeature(LocationMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentSnapshot();
            }
            return snapshot;
        }
        if (snapshotClass == TrafficMonitorFeature.RadioStatSnapshot.class) {
            TrafficMonitorFeature feature = getFeature(TrafficMonitorFeature.class);
            if (feature != null && mMonitor != null) {
                snapshot = feature.currentRadioSnapshot(mMonitor.getContext());
            }
            return snapshot;
        }
        if (snapshotClass == WakeLockMonitorFeature.WakeLockSnapshot.class) {
            WakeLockMonitorFeature feature = getFeature(WakeLockMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentWakeLocks();
            }
            return snapshot;
        }
        if (snapshotClass == WifiMonitorFeature.WifiSnapshot.class) {
            WifiMonitorFeature feature = getFeature(WifiMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentSnapshot();
            }
            return snapshot;
        }
        if (snapshotClass == CpuStatFeature.CpuStateSnapshot.class) {
            CpuStatFeature feature = getFeature(CpuStatFeature.class);
            if (feature != null && feature.isSupported()) {
                snapshot = feature.currentCpuStateSnapshot();
            }
            return snapshot;
        }
        if (snapshotClass == CpuStatFeature.UidCpuStateSnapshot.class) {
            CpuStatFeature feature = getFeature(CpuStatFeature.class);
            if (feature != null && feature.isSupported()) {
                snapshot = feature.currentUidCpuStateSnapshot();
            }
            return snapshot;
        }
        if (snapshotClass == AppStatMonitorFeature.AppStatSnapshot.class) {
            AppStatMonitorFeature feature = getFeature(AppStatMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentAppStatSnapshot();
            }
            return snapshot;
        }
        if (snapshotClass == HealthStatsSnapshot.class) {
            HealthStatsFeature feature = getFeature(HealthStatsFeature.class);
            if (feature != null) {
                snapshot = feature.currHealthStatsSnapshot();
            }
            return snapshot;
        }
        return null;
    }

    protected void configureSamplers() {
        for (Map.Entry<Class<? extends Snapshot<?>>, Long> item : mSampleRegs.entrySet()) {
            Snapshot.Sampler sampler = statSampler(item.getKey());
            if (sampler != null) {
                sampler.setInterval(item.getValue());
                sampler.start();
            }
        }
    }

    protected void configureSampleResults() {
        for (Map.Entry<Class<? extends Snapshot<?>>, Snapshot.Sampler> item : mSamplers.entrySet()) {
            MatrixLog.i(TAG, hashCode() + " " + item.getValue().getTag() + " #pause: " + mScope);
            item.getValue().pause();
            Snapshot.Sampler.Result result = item.getValue().getResult();
            if (result != null) {
                mSampleResults.put(item.getKey(), result);
            }
        }
    }

    @CallSuper
    protected Snapshot.Sampler statSampler(Class<? extends Snapshot<?>> snapshotClass) {
        Snapshot.Sampler sampler = null;
        if (snapshotClass == DeviceStatMonitorFeature.CpuFreqSnapshot.class) {
            final DeviceStatMonitorFeature feature = getFeature(DeviceStatMonitorFeature.class);
            if (feature != null && mMonitor != null) {
                final CpuStatFeature cpuStatsFeat = getFeature(CpuStatFeature.class);
                if (cpuStatsFeat != null) {
                    if (cpuStatsFeat.isSupported()) {
                        mCpuFreqSampler = new CpuFreqSampler(BatteryCanaryUtil.getCpuFreqSteps());
                    }
                }
                sampler = new Snapshot.Sampler("cpufreq", mMonitor.getHandler(), new Function<Snapshot.Sampler, Number>() {
                    @Override
                    public Number apply(Snapshot.Sampler sampler) {
                        int[] cpuFreqs = BatteryCanaryUtil.getCpuCurrentFreq();
                        if (cpuStatsFeat != null && cpuStatsFeat.isSupported()) {
                            if (mCpuFreqSampler != null && mCpuFreqSampler.isCompat(cpuStatsFeat.getPowerProfile())) {
                                mCpuFreqSampler.count(cpuFreqs);
                            }
                        }
                        DeviceStatMonitorFeature.CpuFreqSnapshot snapshot = feature.currentCpuFreq(cpuFreqs);
                        List<DigitEntry<Integer>> list = snapshot.cpuFreqs.getList();
                        MatrixLog.i(TAG, CompositeMonitors.this.hashCode() + " #onSampling: " + mScope);
                        MatrixLog.i(TAG, "onSampling " + sampler.mCount + " " + sampler.mTag + ", val = " + list);
                        if (list.isEmpty()) {
                            return Snapshot.Sampler.INVALID;
                        }
                        // Better to use sum of all cpufreqs, rather than just use the max value?
                        // Collections.sort(list, new Comparator<DigitEntry<Integer>>() {
                        //     @Override
                        //     public int compare(DigitEntry<Integer> o1, DigitEntry<Integer> o2) {
                        //         return o1.get().compareTo(o2.get());
                        //     }
                        // });
                        // return list.isEmpty() ? 0 : list.get(list.size() - 1).get();
                        long sum = 0;
                        for (DigitEntry<Integer> item : list) {
                            sum += item.get();
                        }
                        return sum;
                    }
                });
                mSamplers.put(snapshotClass, sampler);
            }
            return sampler;
        }
        if (snapshotClass == DeviceStatMonitorFeature.BatteryTmpSnapshot.class) {
            final DeviceStatMonitorFeature feature = getFeature(DeviceStatMonitorFeature.class);
            if (feature != null && mMonitor != null) {
                sampler = new Snapshot.Sampler("batt-temp", mMonitor.getHandler(), new Function<Snapshot.Sampler, Number>() {
                    @Override
                    public Number apply(Snapshot.Sampler sampler) {
                        DeviceStatMonitorFeature.BatteryTmpSnapshot snapshot = feature.currentBatteryTemperature(mMonitor.getContext());
                        Integer value = snapshot.temp.get();
                        MatrixLog.i(TAG, "onSampling " + sampler.mCount + " " + sampler.mTag + ", val = " + value);
                        if (value == -1) {
                            return Snapshot.Sampler.INVALID;
                        }
                        return value;
                    }
                });
                mSamplers.put(snapshotClass, sampler);
            }
            return sampler;
        }
        if (snapshotClass == DeviceStatMonitorFeature.ThermalStatSnapshot.class) {
            final DeviceStatMonitorFeature feature = getFeature(DeviceStatMonitorFeature.class);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && feature != null && mMonitor != null) {
                sampler = new Snapshot.Sampler("thermal-stat", mMonitor.getHandler(), new Function<Snapshot.Sampler, Number>() {
                    @Override
                    public Number apply(Snapshot.Sampler sampler) {
                        int value = BatteryCanaryUtil.getThermalStat(mMonitor.getContext());
                        MatrixLog.i(TAG, "onSampling " + sampler.mCount + " " + sampler.mTag + ", val = " + value);
                        if (value == -1) {
                            return Snapshot.Sampler.INVALID;
                        }
                        return value;
                    }
                });
                mSamplers.put(snapshotClass, sampler);
            }
            return sampler;
        }
        if (snapshotClass == DeviceStatMonitorFeature.ThermalHeadroomSnapshot.class) {
            final DeviceStatMonitorFeature feature = getFeature(DeviceStatMonitorFeature.class);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && feature != null && mMonitor != null) {
                final Long interval = mSampleRegs.get(snapshotClass);
                if (interval != null && interval >= 1000L) {
                    sampler = new Snapshot.Sampler("thermal-headroom", mMonitor.getHandler(), new Function<Snapshot.Sampler, Number>() {
                        @Override
                        public Number apply(Snapshot.Sampler sampler) {
                            float value = BatteryCanaryUtil.getThermalHeadroom(mMonitor.getContext(), (int) (interval / 1000L));
                            MatrixLog.i(TAG, "onSampling " + sampler.mCount + " " + sampler.mTag + ", val = " + value);
                            if (value == -1f) {
                                return Snapshot.Sampler.INVALID;
                            }
                            return value;
                        }
                    });
                    mSamplers.put(snapshotClass, sampler);
                }
            }
            return sampler;
        }
        if (snapshotClass == DeviceStatMonitorFeature.ChargeWattageSnapshot.class) {
            final DeviceStatMonitorFeature feature = getFeature(DeviceStatMonitorFeature.class);
            if (feature != null && mMonitor != null) {
                sampler = new Snapshot.Sampler("batt-watt", mMonitor.getHandler(), new Function<Snapshot.Sampler, Number>() {
                    @Override
                    public Number apply(Snapshot.Sampler sampler) {
                        int value = BatteryCanaryUtil.getChargingWatt(mMonitor.getContext());
                        MatrixLog.i(TAG, "onSampling " + sampler.mCount + " " + sampler.mTag + ", val = " + value);
                        if (value == -1) {
                            return Snapshot.Sampler.INVALID;
                        }
                        return value;
                    }
                });
                mSamplers.put(snapshotClass, sampler);
            }
            return sampler;
        }
        if (snapshotClass == CpuStatFeature.CpuStateSnapshot.class) {
            final CpuStatFeature feature = getFeature(CpuStatFeature.class);
            if (feature != null && feature.isSupported() && mMonitor != null) {
                sampler = new Snapshot.Sampler("cpu-stat", mMonitor.getHandler(), new Function<Snapshot.Sampler, Number>() {
                    @Override
                    public Number apply(Snapshot.Sampler sampler) {
                        CpuStatFeature.CpuStateSnapshot snapshot = feature.currentCpuStateSnapshot();
                        for (int i = 0; i < snapshot.cpuCoreStates.size(); i++) {
                            Snapshot.Entry.ListEntry<DigitEntry<Long>> item = snapshot.cpuCoreStates.get(i);
                            MatrixLog.i(TAG, "onSampling " + sampler.mCount + " " + sampler.mTag + " cpuCore" + i  + ", val = " + item.getList());
                        }
                        for (int i = 0; i < snapshot.procCpuCoreStates.size(); i++) {
                            Snapshot.Entry.ListEntry<DigitEntry<Long>> item = snapshot.procCpuCoreStates.get(i);
                            MatrixLog.i(TAG, "onSampling " + sampler.mCount + " " + sampler.mTag + " procCpuCluster" + i + ", val = " + item.getList());
                        }
                        return 0;
                    }
                });
                mSamplers.put(snapshotClass, sampler);
            }
            return sampler;
        }
        if (snapshotClass == JiffiesMonitorFeature.UidJiffiesSnapshot.class) {
            final JiffiesMonitorFeature feature = getFeature(JiffiesMonitorFeature.class);
            if (feature != null && mMonitor != null) {
                sampler = new Snapshot.Sampler("uid-jiffies", mMonitor.getHandler(), new Function<Snapshot.Sampler, Number>() {
                    JiffiesMonitorFeature.UidJiffiesSnapshot mLastSnapshot;
                    @Override
                    public Number apply(Snapshot.Sampler sampler) {
                        JiffiesMonitorFeature.UidJiffiesSnapshot curr = feature.currentUidJiffiesSnapshot();
                        if (mLastSnapshot != null) {
                            Delta<JiffiesMonitorFeature.UidJiffiesSnapshot> delta = curr.diff(mLastSnapshot);
                            long minute = Math.max(1, delta.during / BatteryCanaryUtil.ONE_MIN);
                            long avgUidJiffies = computeAvgJiffies(delta.dlt.totalUidJiffies.get(), delta.during);
                            MatrixLog.i(TAG, "onSampling " + sampler.mCount + " " + sampler.mTag + " avgUidJiffies, val = " + avgUidJiffies + ", minute = " + minute);
                            for (Delta<JiffiesSnapshot> item : delta.dlt.pidDeltaJiffiesList) {
                                long avgPidJiffies = computeAvgJiffies(item.dlt.totalJiffies.get(), delta.during);
                                MatrixLog.i(TAG, "onSampling " + sampler.mCount + " " + sampler.mTag + " avgPidJiffies, val = " + avgPidJiffies + ", minute = " + minute + ", name = " + item.dlt.name);
                            }
                            mLastSnapshot = curr;
                            return avgUidJiffies;
                        } else {
                            mLastSnapshot = curr;
                        }
                        return 0;
                    }
                });
                mSamplers.put(snapshotClass, sampler);
            }
            return sampler;
        }
        if (snapshotClass == TrafficMonitorFeature.RadioStatSnapshot.class) {
            final TrafficMonitorFeature feature = getFeature(TrafficMonitorFeature.class);
            if (feature != null && mMonitor != null) {
                sampler = new MonitorFeature.Snapshot.Sampler("traffic", mMonitor.getHandler(), new Function<Snapshot.Sampler, Number>() {
                    @Override
                    public Number apply(Snapshot.Sampler sampler) {
                        TrafficMonitorFeature.RadioStatSnapshot snapshot = feature.currentRadioSnapshot(mMonitor.getContext());
                        if (snapshot != null) {
                            MatrixLog.i(TAG, "onSampling " + sampler.mCount + " " + sampler.mTag + " wifiRx, val = " + snapshot.wifiRxBytes);
                            MatrixLog.i(TAG, "onSampling " + sampler.mCount + " " + sampler.mTag + " wifiTx, val = " + snapshot.wifiTxBytes);
                            MatrixLog.i(TAG, "onSampling " + sampler.mCount + " " + sampler.mTag + " mobileRx, val = " + snapshot.mobileRxBytes);
                            MatrixLog.i(TAG, "onSampling " + sampler.mCount + " " + sampler.mTag + " mobileTx, val = " + snapshot.mobileTxBytes);
                        }
                        return 0;
                    }
                });
                mSamplers.put(snapshotClass, sampler);
            }
            return sampler;
        }
        if (snapshotClass == TrafficMonitorFeature.RadioBpsSnapshot.class) {
            final TrafficMonitorFeature feature = getFeature(TrafficMonitorFeature.class);
            if (feature != null && mMonitor != null) {
                mBpsSampler = new BpsSampler();
                sampler = new MonitorFeature.Snapshot.Sampler("trafficBps", mMonitor.getHandler(), new Function<Snapshot.Sampler, Number>() {
                    @Override
                    public Number apply(Snapshot.Sampler sampler) {
                        TrafficMonitorFeature.RadioBpsSnapshot snapshot = feature.currentRadioBpsSnapshot(mMonitor.getContext());
                        if (snapshot != null) {
                            mBpsSampler.count(snapshot);
                        }
                        return 0;
                    }
                });
                mSamplers.put(snapshotClass, sampler);
            }
            return sampler;
        }
        if (snapshotClass == DeviceStatMonitorFeature.BatteryCurrentSnapshot.class) {
            final DeviceStatMonitorFeature feature = getFeature(DeviceStatMonitorFeature.class);
            if (feature != null && mMonitor != null) {
                sampler = new MonitorFeature.Snapshot.Sampler("batt-curr", mMonitor.getHandler(), new Function<Snapshot.Sampler, Number>() {
                    @Override
                    public Number apply(Snapshot.Sampler sampler) {
                        long value = BatteryCanaryUtil.getBatteryCurrencyImmediately(mMonitor.getContext());
                        MatrixLog.i(TAG, "onSampling " + sampler.mCount + " " + sampler.mTag + ", val = " + value);
                        if (value == -1L) {
                            return Snapshot.Sampler.INVALID;
                        }
                        return value;
                    }
                });
                mSamplers.put(snapshotClass, sampler);
            }
            return sampler;
        }
        return null;
    }

    protected void configureTaskDeltas(final Class<? extends AbsTaskMonitorFeature> featClass) {
        if (mAppStats != null) {
            AbsTaskMonitorFeature taskFeat = getFeature(featClass);
            if (taskFeat != null) {
                List<Delta<TaskJiffiesSnapshot>> deltas = taskFeat.currentJiffies(mAppStats.duringMillis);
                // No longer clear here
                // Clear at BG Scope or OverHeat
                // taskFeat.clearFinishedJiffies();
                putTaskDeltas(featClass, deltas);
            }
        }
    }

    protected void collectTaskDeltas() {
        if (!mTaskDeltas.isEmpty()) {
            for (Map.Entry<Class<? extends AbsTaskMonitorFeature>, List<Delta<TaskJiffiesSnapshot>>> entry : mTaskDeltas.entrySet()) {
                Class<? extends AbsTaskMonitorFeature> key = entry.getKey();
                for (Delta<TaskJiffiesSnapshot> taskDelta : entry.getValue()) {
                    // FIXME: better windowMillis cfg of Task and AppStats
                    if (taskDelta.bgn.time >= mBgnMillis) {
                        List<Pair<Class<? extends AbsTaskMonitorFeature>, Delta<TaskJiffiesSnapshot>>> pairList = mTaskDeltasCollect.get(taskDelta.dlt.name);
                        if (pairList == null) {
                            pairList = new ArrayList<>();
                            mTaskDeltasCollect.put(taskDelta.dlt.name, pairList);
                        }
                        pairList.add(new Pair<Class<? extends AbsTaskMonitorFeature>, Delta<TaskJiffiesSnapshot>>(key, taskDelta));
                    }
                }
            }
        }
    }

    public void putTaskDeltas(Class<? extends AbsTaskMonitorFeature> key, List<Delta<TaskJiffiesSnapshot>> deltas) {
        mTaskDeltas.put(key, deltas);
    }

    public List<Delta<TaskJiffiesSnapshot>> getTaskDeltas(Class<? extends AbsTaskMonitorFeature> key) {
        List<Delta<TaskJiffiesSnapshot>> deltas = mTaskDeltas.get(key);
        if (deltas == null) {
            return Collections.emptyList();
        }
        return deltas;
    }

    public void getTaskDeltas(Class<? extends AbsTaskMonitorFeature> key, Consumer<List<Delta<TaskJiffiesSnapshot>>> block) {
        List<Delta<TaskJiffiesSnapshot>> deltas = mTaskDeltas.get(key);
        if (deltas != null) {
            block.accept(deltas);
        }
    }

    public Map<String, List<Pair<Class<? extends AbsTaskMonitorFeature>, Delta<TaskJiffiesSnapshot>>>> getCollectedTaskDeltas() {
        if (mTaskDeltasCollect.size() <= 1) {
            return mTaskDeltasCollect;
        }
        // Sorting by jiffies sum
        return BatteryCanaryUtil.sortMapByValue(mTaskDeltasCollect, new Comparator<Map.Entry<String, List<Pair<Class<? extends AbsTaskMonitorFeature>, Delta<TaskJiffiesSnapshot>>>>>() {
            @SuppressWarnings("ConstantConditions")
            @Override
            public int compare(Map.Entry<String, List<Pair<Class<? extends AbsTaskMonitorFeature>, Delta<TaskJiffiesSnapshot>>>> o1, Map.Entry<String, List<Pair<Class<? extends AbsTaskMonitorFeature>, Delta<TaskJiffiesSnapshot>>>> o2) {
                long sumLeft = 0, sumRight = 0;
                for (Pair<Class<? extends AbsTaskMonitorFeature>, Delta<TaskJiffiesSnapshot>> item : o1.getValue()) {
                    sumLeft += item.second.dlt.jiffies.get();
                }
                for (Pair<Class<? extends AbsTaskMonitorFeature>, Delta<TaskJiffiesSnapshot>> item : o2.getValue()) {
                    sumRight += item.second.dlt.jiffies.get();
                }
                long minus = sumLeft - sumRight;
                if (minus == 0) return 0;
                if (minus > 0) return -1;
                return 1;
            }
        });
    }

    public void getCollectedTaskDeltas(Consumer<Map<String, List<Pair<Class<? extends AbsTaskMonitorFeature>, Delta<TaskJiffiesSnapshot>>>>> block) {
        block.accept(getCollectedTaskDeltas());
    }

    public void getAllPidDeltaList(Consumer<List<Delta<JiffiesSnapshot>>> block) {
        List<Delta<JiffiesSnapshot>> deltaList = getAllPidDeltaList();
        if (deltaList != null) {
            block.accept(deltaList);
        }
    }

    public List<Delta<JiffiesSnapshot>> getAllPidDeltaList() {
        Delta<JiffiesMonitorFeature.UidJiffiesSnapshot> delta = getDelta(JiffiesMonitorFeature.UidJiffiesSnapshot.class);
        if (delta == null) {
            Delta<JiffiesSnapshot> pidDelta = getDelta(JiffiesSnapshot.class);
            if (pidDelta != null) {
                return Collections.singletonList(pidDelta);
            }
            return Collections.emptyList();
        }
        return delta.dlt.pidDeltaJiffiesList;
    }

    public Map<String, String> getStacks() {
        return mStacks;
    }

    public Bundle getExtras() {
        return mExtras;
    }

    @Nullable
    public CpuFreqSampler getCpuFreqSampler() {
        return mCpuFreqSampler;
    }

    @Nullable
    public BpsSampler getBpsSampler() {
        return mBpsSampler;
    }

    @Override
    @NonNull
    public String toString() {
        return "CompositeMonitors{" + "\n" +
                "Metrics=" + mMetrics + "\n" +
                ", BgnSnapshots=" + mBgnSnapshots + "\n" +
                ", Deltas=" + mDeltas + "\n" +
                ", SampleRegs=" + mSampleRegs + "\n" +
                ", Samplers=" + mSamplers + "\n" +
                ", SampleResults=" + mSampleResults + "\n" +
                ", TaskDeltas=" + mTaskDeltas + "\n" +
                ", AppStats=" + mAppStats + "\n" +
                ", Stacks=" + mStacks + "\n" +
                ", Extras =" + mExtras + "\n" +
                '}';
    }

    public static class CpuFreqSampler {
        public int[] cpuCurrentFreq;
        public final List<int[]> cpuFreqSteps;
        public final List<int[]> cpuFreqCounters;

        public CpuFreqSampler(List<int[]> cpuFreqSteps) {
            this.cpuFreqSteps = cpuFreqSteps;
            this.cpuFreqCounters = new ArrayList<>(cpuFreqSteps.size());
            for (int[] item : cpuFreqSteps) {
                this.cpuFreqCounters.add(new int[item.length]);
            }
        }

        public boolean isCompat(PowerProfile powerProfile) {
            if (cpuFreqSteps.size() == powerProfile.getCpuCoreNum()) {
                for (int i = 0; i < cpuFreqSteps.size(); i++) {
                    int clusterByCpuNum = powerProfile.getClusterByCpuNum(i);
                    int steps = powerProfile.getNumSpeedStepsInCpuCluster(clusterByCpuNum);
                    if (cpuFreqSteps.get(i).length != steps) {
                        return false;
                    }
                }
                return true;
            }
            return false;
        }

        public void count(int[] cpuCurrentFreq) {
            this.cpuCurrentFreq = cpuCurrentFreq;
            for (int i = 0; i < cpuCurrentFreq.length; i++) {
                int speed = cpuCurrentFreq[i];
                int[] steps = cpuFreqSteps.get(i);
                if (speed < steps[0]) {
                    cpuFreqCounters.get(i)[0]++;
                    continue;
                }
                boolean found = false;
                for (int j = 0; j < steps.length; j++) {
                    if (speed <= steps[j]) {
                        cpuFreqCounters.get(i)[j]++;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (speed > steps[steps.length - 1]) {
                        cpuFreqCounters.get(i)[steps.length - 1]++;
                    }
                }
            }
        }
    }

    public static class BpsSampler {
        public int count;
        public long wifiRxBps;
        public long wifiTxBps;
        public long mobileRxBps;
        public long mobileTxBps;

        public void count(TrafficMonitorFeature.RadioBpsSnapshot snapshot) {
            count++;
            wifiRxBps += snapshot.wifiRxBps.get();
            wifiTxBps += snapshot.wifiTxBps.get();
            mobileRxBps += snapshot.mobileRxBps.get();
            mobileTxBps += snapshot.mobileTxBps.get();
        }

        public double getAverage(long input) {
            if (count != 0) {
                return input * 1f / count;
            }
            return 0;
        }
    }


    protected void polishEstimatedPower() {
        getDelta(HealthStatsFeature.HealthStatsSnapshot.class, new Consumer<Delta<HealthStatsSnapshot>>() {
            @Override
            public void accept(Delta<HealthStatsSnapshot> healthStatsDelta) {
                tuningPowers(healthStatsDelta.dlt);

                {
                    // Reset cpuPower if exists
                    double power = 0;
                    Object powers = healthStatsDelta.dlt.extras.get("JiffyUid");
                    if (powers instanceof Map<?, ?>) {
                        // Take power-cpu-uidDiff or 0 as default
                        Object val = ((Map<?, ?>) powers).get("power-cpu-uidDiff");
                        if (val instanceof Double) {
                            power = (double) val;
                        }
                    }
                    healthStatsDelta.dlt.cpuPower = DigitEntry.of(power);
                }
                {
                    // Reset mobilePower if exists
                    double power = 0;
                    Object val = healthStatsDelta.dlt.extras.get("power-mobile-statByte");
                    if (val instanceof Double) {
                        power = (double) val;
                    }
                    healthStatsDelta.dlt.mobilePower = DigitEntry.of(power);
                }
                {
                    // Reset wifiPower if exists
                    double power = 0;
                    Object val = healthStatsDelta.dlt.extras.get("power-wifi-statByte");
                    if (val instanceof Double) {
                        power = (double) val;
                    }
                    healthStatsDelta.dlt.wifiPower = DigitEntry.of(power);
                }
            }
        });
    }

    protected void tuningPowers(final HealthStatsFeature.HealthStatsSnapshot snapshot) {
        if (!snapshot.isDelta) {
            throw new IllegalStateException("Only support delta snapshot");
        }
        final Tuner tuner = new Tuner();
        getFeature(CpuStatFeature.class, new Consumer<CpuStatFeature>() {
            @Override
            public void accept(CpuStatFeature feat) {
                if (feat.isSupported()) {
                    final PowerProfile powerProfile = feat.getPowerProfile();

                    // Tune CPU
                    getDelta(CpuStatFeature.CpuStateSnapshot.class, new Consumer<Delta<CpuStatFeature.CpuStateSnapshot>>() {
                        @Override
                        public void accept(final Delta<CpuStatFeature.CpuStateSnapshot> cpuStatDelta) {
                            // CpuTimeMs
                            getDelta(HealthStatsFeature.HealthStatsSnapshot.class, new Consumer<Delta<HealthStatsFeature.HealthStatsSnapshot>>() {
                                @Override
                                public void accept(final Delta<HealthStatsFeature.HealthStatsSnapshot> healthStats) {
                                    healthStats.dlt.extras.put("TimeUid", tuner.tuningCpuPowers(powerProfile, CompositeMonitors.this, new Tuner.CpuTime() {
                                        @Override
                                        public long getBgnMs(String procSuffix) {
                                            return getCpuTimeMs(procSuffix, healthStats.bgn);
                                        }

                                        @Override
                                        public long getEndMs(String procSuffix) {
                                            return getCpuTimeMs(procSuffix, healthStats.end);
                                        }

                                        @Override
                                        public long getDltMs(String procSuffix) {
                                            return getCpuTimeMs(procSuffix, healthStats.dlt);
                                        }

                                        private long getCpuTimeMs(String procSuffix, HealthStatsFeature.HealthStatsSnapshot healthStatsSnapshot) {
                                            if (procSuffix == null) {
                                                return healthStatsSnapshot.cpuUsrTimeMs.get()
                                                        + healthStatsSnapshot.cpuSysTimeMs.get();
                                            } else {
                                                if (mMonitor != null) {
                                                    String procName = mMonitor.getContext().getPackageName();
                                                    if ("main".equals(procSuffix)) {
                                                        procName = mMonitor.getContext().getPackageName() + ":" + procSuffix;
                                                    }
                                                    DigitEntry<Long> usrTime = healthStatsSnapshot.procStatsCpuUsrTimeMs.get(procName);
                                                    DigitEntry<Long> sysTime = healthStatsSnapshot.procStatsCpuSysTimeMs.get(procName);
                                                    return (usrTime == null ? 0 : usrTime.get()) + (sysTime == null ? 0 : sysTime.get());
                                                }
                                            }
                                            return 0L;
                                        }
                                    }));
                                }
                            });

                            // CpuTimeJiffies
                            getDelta(JiffiesMonitorFeature.UidJiffiesSnapshot.class, new Consumer<Delta<JiffiesMonitorFeature.UidJiffiesSnapshot>>() {
                                @Override
                                public void accept(final Delta<JiffiesMonitorFeature.UidJiffiesSnapshot> delta) {
                                    snapshot.extras.put("JiffyUid", tuner.tuningCpuPowers(powerProfile, CompositeMonitors.this, new Tuner.CpuTime() {
                                        @Override
                                        public long getBgnMs(String procSuffix) {
                                            if (procSuffix == null) {
                                                // All
                                                return delta.bgn.totalUidJiffies.get() * 10L;
                                            } else {
                                                for (JiffiesSnapshot item : delta.bgn.pidCurrJiffiesList) {
                                                    if (item.name.equals(procSuffix)) {
                                                        return item.totalJiffies.get() * 10L;
                                                    }
                                                }
                                            }
                                            return 0L;
                                        }
                                        @Override
                                        public long getEndMs(String procSuffix) {
                                            if (procSuffix == null) {
                                                // All
                                                return delta.end.totalUidJiffies.get() * 10L;
                                            } else {
                                                for (JiffiesSnapshot item : delta.end.pidCurrJiffiesList) {
                                                    if (item.name.equals(procSuffix)) {
                                                        return item.totalJiffies.get() * 10L;
                                                    }
                                                }
                                            }
                                            return 0L;
                                        }
                                        @Override
                                        public long getDltMs(String procSuffix) {
                                            if (procSuffix == null) {
                                                // All
                                                return delta.dlt.totalUidJiffies.get() * 10L;
                                            } else {
                                                for (Delta<JiffiesSnapshot> item : delta.dlt.pidDeltaJiffiesList) {
                                                    if (item.dlt.name.equals(procSuffix)) {
                                                        return item.dlt.totalJiffies.get() * 10L;
                                                    }
                                                }
                                            }
                                            return 0L;
                                        }
                                    }));
                                }
                            });
                        }
                    });

                    // Tune Network
                    getDelta(TrafficMonitorFeature.RadioStatSnapshot.class, new Consumer<Delta<TrafficMonitorFeature.RadioStatSnapshot>>() {
                        @Override
                        public void accept(Delta<TrafficMonitorFeature.RadioStatSnapshot> delta) {
                            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP && mMonitor != null) {
                                BpsSampler bpsSampler = getBpsSampler();
                                if (bpsSampler != null) {
                                    // mobile
                                    double power = HealthStatsHelper.calcMobilePowerByNetworkStatBytes(
                                            powerProfile,
                                            delta.dlt,
                                            bpsSampler.getAverage(bpsSampler.mobileRxBps),
                                            bpsSampler.getAverage(bpsSampler.mobileTxBps));
                                    snapshot.extras.put("power-mobile-statByte", power);

                                    // wifi
                                    power = HealthStatsHelper.calcWifiPowerByNetworkStatBytes(
                                            powerProfile,
                                            delta.dlt,
                                            bpsSampler.getAverage(bpsSampler.wifiRxBps),
                                            bpsSampler.getAverage(bpsSampler.wifiTxBps));
                                    snapshot.extras.put("power-wifi-statByte", power);
                                    power = HealthStatsHelper.calcWifiPowerByNetworkStatPackets(
                                            powerProfile,
                                            delta.dlt,
                                            bpsSampler.getAverage(bpsSampler.wifiRxBps),
                                            bpsSampler.getAverage(bpsSampler.wifiTxBps));
                                    snapshot.extras.put("power-wifi-statPacket", power);
                                }
                            }
                        }
                    });
                }
            }
        });
    }


    @SuppressLint("RestrictedApi")
    static class Tuner {
        interface CpuTime {
            long getBgnMs(String procSuffix);
            long getEndMs(String procSuffix);
            long getDltMs(String procSuffix);
        }

        public Map<String, Object> tuningCpuPowers(final PowerProfile powerProfile, final CompositeMonitors monitors, final CpuTime cpuTime) {
            final Map<String, Object> dict = new HashMap<>();
            monitors.getDelta(CpuStatFeature.UidCpuStateSnapshot.class, new Consumer<Delta<CpuStatFeature.UidCpuStateSnapshot>>() {
                @SuppressLint("VisibleForTests")
                @Override
                public void accept(Delta<CpuStatFeature.UidCpuStateSnapshot> uidCpuStatDelta) {
                    // Calc by DIff
                    {
                        // UID Diff
                        double cpuPower = 0;
                        boolean scaled = false;
                        for (Delta<CpuStatFeature.CpuStateSnapshot> cpuStateDelta : uidCpuStatDelta.dlt.pidDeltaCpuSateList) {
                            CpuStatFeature.CpuStateSnapshot cpuStatsSnapshot = cpuStateDelta.dlt;
                            long cpuTimeMs = cpuTime.getDltMs(cpuStatsSnapshot.name);
                            cpuPower += HealthStatsHelper.estimateCpuActivePower(powerProfile, cpuTimeMs)
                                    + HealthStatsHelper.estimateCpuClustersPowerByUidStats(powerProfile, cpuStatsSnapshot, cpuTimeMs, scaled)
                                    + HealthStatsHelper.estimateCpuCoresPowerByUidStats(powerProfile, cpuStatsSnapshot, cpuTimeMs, scaled);
                        }
                        dict.put("power-cpu-uidDiff", cpuPower);
                    }
                    {
                        // UID Diff Scaled
                        double cpuPower = 0;
                        boolean scaled = true;
                        for (Delta<CpuStatFeature.CpuStateSnapshot> cpuStateDelta : uidCpuStatDelta.dlt.pidDeltaCpuSateList) {
                            CpuStatFeature.CpuStateSnapshot cpuStatsSnapshot = cpuStateDelta.dlt;
                            long cpuTimeMs = cpuTime.getDltMs(cpuStatsSnapshot.name);
                            cpuPower += HealthStatsHelper.estimateCpuActivePower(powerProfile, cpuTimeMs)
                                    + HealthStatsHelper.estimateCpuClustersPowerByUidStats(powerProfile, cpuStatsSnapshot, cpuTimeMs, scaled)
                                    + HealthStatsHelper.estimateCpuCoresPowerByUidStats(powerProfile, cpuStatsSnapshot, cpuTimeMs, scaled);
                        }
                        dict.put("power-cpu-uidDiffScale", cpuPower);
                    }

                    monitors.getDelta(CpuStatFeature.CpuStateSnapshot.class, new Consumer<Delta<CpuStatFeature.CpuStateSnapshot>>() {
                        @Override
                        public void accept(Delta<CpuStatFeature.CpuStateSnapshot> pidCpuStatDelta) {
                            {
                                // DEV Diff
                                CpuStatFeature.CpuStateSnapshot cpuStatsSnapshot = pidCpuStatDelta.dlt;
                                long cpuTimeMs = cpuTime.getDltMs(null);
                                double cpuPower = HealthStatsHelper.estimateCpuActivePower(powerProfile, cpuTimeMs)
                                        + HealthStatsHelper.estimateCpuClustersPowerByDevStats(powerProfile, cpuStatsSnapshot, cpuTimeMs)
                                        + HealthStatsHelper.estimateCpuCoresPowerByDevStats(powerProfile, cpuStatsSnapshot, cpuTimeMs);
                                dict.put("power-cpu-devDiff", cpuPower);
                            }
                            {
                                // CpuFreq Diff
                                CompositeMonitors.CpuFreqSampler cpuFreqSampler = monitors.getCpuFreqSampler();
                                if (cpuFreqSampler != null) {
                                    if (cpuFreqSampler.isCompat(powerProfile)) {
                                        long cpuTimeMs = cpuTime.getDltMs(null);
                                        double cpuPower = HealthStatsHelper.estimateCpuActivePower(powerProfile, cpuTimeMs)
                                                + estimateCpuPowerByCpuFreqStats(powerProfile, cpuFreqSampler, cpuTimeMs);
                                        dict.put("power-cpu-cpuFreq", cpuPower);
                                    }
                                }
                            }
                        }
                    });
                }
            });

            return dict;
        }

        private static double estimateCpuPowerByCpuFreqStats(PowerProfile powerProfile, CompositeMonitors.CpuFreqSampler sampler, long cpuTimeMs) {
            double powerMah = 0;
            if (cpuTimeMs > 0) {
                long totalSum = 0;
                for (int i = 0; i < sampler.cpuFreqCounters.size(); i++) {
                    for (int j = 0; j < sampler.cpuFreqCounters.get(i).length; j++) {
                        totalSum += sampler.cpuFreqCounters.get(i)[j];
                    }
                }
                if (totalSum > 0) {
                    for (int i = 0; i < sampler.cpuFreqCounters.size(); i++) {
                        int clusterNum = powerProfile.getClusterByCpuNum(i);
                        long coreSum = 0;
                        for (int j = 0; j < sampler.cpuFreqCounters.get(i).length; j++) {
                            int step = sampler.cpuFreqCounters.get(i)[j];
                            if (step > 0) {
                                long figuredCoreTimeMs = (long) ((step * 1.0f / totalSum) * cpuTimeMs);
                                double powerMa = powerProfile.getAveragePowerForCpuCore(clusterNum, j);
                                powerMah += new HealthStatsHelper.UsageBasedPowerEstimator(powerMa).calculatePower(figuredCoreTimeMs);
                            }
                            coreSum += step;
                        }
                        if (coreSum > 0) {
                            long figuredClusterTimeMs = (long) ((coreSum * 1.0f / totalSum) * cpuTimeMs);
                            double powerMa = powerProfile.getAveragePowerForCpuCluster(clusterNum);
                            powerMah += new HealthStatsHelper.UsageBasedPowerEstimator(powerMa).calculatePower(figuredClusterTimeMs);
                        }
                    }
                }
            }
            return powerMah;
        }
    }
}
