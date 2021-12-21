package com.tencent.matrix.batterycanary.monitor.feature;

import android.os.Bundle;
import android.os.SystemClock;

import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.DigitEntry;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.Consumer;
import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Callable;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

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
    protected final Map<Class<? extends AbsTaskMonitorFeature>, List<Delta<AbsTaskMonitorFeature.TaskJiffiesSnapshot>>> mTaskDeltas = new HashMap<>();

    // Extra Info
    protected final Bundle mExtras = new Bundle();

    @Nullable
    protected BatteryMonitorCore mMonitor;
    @Nullable
    protected AppStats mAppStats;
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

    public void clear() {
        mBgnSnapshots.clear();
        mDeltas.clear();
        mSamplers.clear();
        mSampleResults.clear();
        mTaskDeltas.clear();
    }

    public CompositeMonitors fork() {
        CompositeMonitors that = new CompositeMonitors(mMonitor, mScope);
        that.mBgnMillis = this.mBgnMillis;
        that.mAppStats = this.mAppStats;

        that.mMetrics.addAll(mMetrics);
        that.mBgnSnapshots.putAll(mBgnSnapshots);
        that.mDeltas.putAll(mDeltas);

        that.mSampleRegs.putAll(mSampleRegs);
        that.mSamplers.putAll(mSamplers);
        that.mSampleResults.putAll(mSampleResults);

        that.mTaskDeltas.putAll(this.mTaskDeltas);
        that.mExtras.putAll(this.mExtras);
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

        Delta<JiffiesMonitorFeature.JiffiesSnapshot> appJiffies = getDelta(JiffiesMonitorFeature.JiffiesSnapshot.class);
        Delta<CpuStatFeature.CpuStateSnapshot> cpuJiffies = getDelta(CpuStatFeature.CpuStateSnapshot.class);
        if (appJiffies == null) {
            MatrixLog.w(TAG, JiffiesMonitorFeature.JiffiesSnapshot.class + " should be metrics to get CpuLoad");
            return -1;
        }
        if (cpuJiffies == null) {
            MatrixLog.w(TAG, CpuStatFeature.CpuStateSnapshot.class + "should be metrics to get CpuLoad");
            return -1;
        }

        final long appJiffiesDelta = appJiffies.dlt.totalJiffies.get();
        final long cpuJiffiesDelta = cpuJiffies.dlt.totalCpuJiffies();
        final float cpuLoad = cpuJiffiesDelta > 0 ? (float) appJiffiesDelta / cpuJiffiesDelta : 0;
        return (int) (cpuLoad * BatteryCanaryUtil.getCpuCoreNum() * 100);
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
        metric(JiffiesMonitorFeature.JiffiesSnapshot.class);
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
        mAppStats = null;
        mBgnMillis = SystemClock.uptimeMillis();
        configureBgnSnapshots();
        configureSamplers();
    }

    public void finish() {
        configureEndDeltas();
        configureSampleResults();
        mAppStats = AppStats.current(SystemClock.uptimeMillis() - mBgnMillis);
    }

    protected void configureBgnSnapshots() {
        for (Class<? extends Snapshot<?>> item : mMetrics) {
            statCurrSnapshot(item);
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

    @CallSuper
    protected Snapshot<?> statCurrSnapshot(Class<? extends Snapshot<?>> snapshotClass) {
        Snapshot<?> snapshot = null;
        if (snapshotClass == AlarmMonitorFeature.AlarmSnapshot.class) {
            AlarmMonitorFeature feature = getFeature(AlarmMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentAlarms();
                mBgnSnapshots.put(snapshotClass, snapshot);
            }
            return snapshot;
        }
        if (snapshotClass == BlueToothMonitorFeature.BlueToothSnapshot.class) {
            BlueToothMonitorFeature feature = getFeature(BlueToothMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentSnapshot();
                mBgnSnapshots.put(snapshotClass, snapshot);
            }
            return snapshot;
        }
        if (snapshotClass == DeviceStatMonitorFeature.CpuFreqSnapshot.class) {
            DeviceStatMonitorFeature feature = getFeature(DeviceStatMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentCpuFreq();
                mBgnSnapshots.put(snapshotClass, snapshot);
            }
            return snapshot;
        }
        if (snapshotClass == DeviceStatMonitorFeature.BatteryTmpSnapshot.class) {
            DeviceStatMonitorFeature feature = getFeature(DeviceStatMonitorFeature.class);
            if (feature != null && mMonitor != null) {
                snapshot = feature.currentBatteryTemperature(mMonitor.getContext());
                mBgnSnapshots.put(snapshotClass, snapshot);
            }
            return snapshot;
        }
        if (snapshotClass == JiffiesMonitorFeature.JiffiesSnapshot.class) {
            JiffiesMonitorFeature feature = getFeature(JiffiesMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentJiffiesSnapshot();
                mBgnSnapshots.put(snapshotClass, snapshot);
            }
            return snapshot;
        }
        if (snapshotClass == LocationMonitorFeature.LocationSnapshot.class) {
            LocationMonitorFeature feature = getFeature(LocationMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentSnapshot();
                mBgnSnapshots.put(snapshotClass, snapshot);
            }
            return snapshot;
        }
        if (snapshotClass == TrafficMonitorFeature.RadioStatSnapshot.class) {
            TrafficMonitorFeature feature = getFeature(TrafficMonitorFeature.class);
            if (feature != null && mMonitor != null) {
                snapshot = feature.currentRadioSnapshot(mMonitor.getContext());
                mBgnSnapshots.put(snapshotClass, snapshot);
            }
            return snapshot;
        }
        if (snapshotClass == WakeLockMonitorFeature.WakeLockSnapshot.class) {
            WakeLockMonitorFeature feature = getFeature(WakeLockMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentWakeLocks();
                mBgnSnapshots.put(snapshotClass, snapshot);
            }
            return snapshot;
        }
        if (snapshotClass == WifiMonitorFeature.WifiSnapshot.class) {
            WifiMonitorFeature feature = getFeature(WifiMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentSnapshot();
                mBgnSnapshots.put(snapshotClass, snapshot);
            }
            return snapshot;
        }
        if (snapshotClass == CpuStatFeature.CpuStateSnapshot.class) {
            CpuStatFeature feature = getFeature(CpuStatFeature.class);
            if (feature != null && feature.isSupported()) {
                snapshot = feature.currentCpuStateSnapshot();
                mBgnSnapshots.put(snapshotClass, snapshot);
            }
            return snapshot;
        }
        if (snapshotClass == AppStatMonitorFeature.AppStatSnapshot.class) {
            AppStatMonitorFeature feature = getFeature(AppStatMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentAppStatSnapshot();
                mBgnSnapshots.put(snapshotClass, snapshot);
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
                sampler = new Snapshot.Sampler(mMonitor.getHandler(), new Callable<Number>() {
                    @Override
                    public Number call() {
                        DeviceStatMonitorFeature.CpuFreqSnapshot snapshot = feature.currentCpuFreq();
                        List<DigitEntry<Integer>> list = snapshot.cpuFreqs.getList();
                        Collections.sort(list, new Comparator<DigitEntry<Integer>>() {
                            @Override
                            public int compare(DigitEntry<Integer> o1, DigitEntry<Integer> o2) {
                                return o1.get().compareTo(o2.get());
                            }
                        });
                        return list.isEmpty() ? 0 : list.get(list.size() - 1).get();
                    }
                });
                mSamplers.put(snapshotClass, sampler);
            }
            return sampler;
        }
        if (snapshotClass == DeviceStatMonitorFeature.BatteryTmpSnapshot.class) {
            final DeviceStatMonitorFeature feature = getFeature(DeviceStatMonitorFeature.class);
            if (feature != null && mMonitor != null) {
                sampler = new Snapshot.Sampler(mMonitor.getHandler(), new Callable<Number>() {
                    @Override
                    public Number call() {
                        DeviceStatMonitorFeature.BatteryTmpSnapshot snapshot = feature.currentBatteryTemperature(mMonitor.getContext());
                        return snapshot.temp.get();
                    }
                });
                mSamplers.put(snapshotClass, sampler);
            }
            return sampler;
        }
        return null;
    }

    public void configureTaskDeltas(final Class<? extends AbsTaskMonitorFeature> featClass) {
        AbsTaskMonitorFeature taskFeat = getFeature(featClass);
        if (taskFeat != null) {
            List<Delta<AbsTaskMonitorFeature.TaskJiffiesSnapshot>> deltas = taskFeat.currentJiffies();
            taskFeat.clearFinishedJiffies();
            putTaskDeltas(featClass, deltas);
        }
    }

    public void putTaskDeltas(Class<? extends AbsTaskMonitorFeature> key, List<Delta<AbsTaskMonitorFeature.TaskJiffiesSnapshot>> deltas) {
        mTaskDeltas.put(key, deltas);
    }

    public List<Delta<AbsTaskMonitorFeature.TaskJiffiesSnapshot>> getTaskDeltas(Class<? extends AbsTaskMonitorFeature> key) {
        List<Delta<AbsTaskMonitorFeature.TaskJiffiesSnapshot>> deltas = mTaskDeltas.get(key);
        if (deltas == null) {
            return Collections.emptyList();
        }
        return deltas;
    }

    public void getTaskDeltas(Class<? extends AbsTaskMonitorFeature> key, Consumer<List<Delta<AbsTaskMonitorFeature.TaskJiffiesSnapshot>>> block) {
        List<Delta<AbsTaskMonitorFeature.TaskJiffiesSnapshot>> deltas = mTaskDeltas.get(key);
        if (deltas != null) {
            block.accept(deltas);
        }
    }

    public Bundle getExtras() {
        return mExtras;
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
                ", Extras =" + mExtras + "\n" +
                '}';
    }
}
