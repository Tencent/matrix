package com.tencent.matrix.batterycanary.monitor.feature;

import android.os.SystemClock;

import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.batterycanary.utils.Consumer;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;

/**
 * @author Kaede
 * @since 2021/9/18
 */
public class CompositeMonitors {
    protected final List<Class<? extends Snapshot<?>>> mMetrics = new ArrayList<>();
    protected final Map<Class<? extends Snapshot<?>>, Snapshot<?>> mBgnSnapshots = new HashMap<>();
    protected final Map<Class<? extends Snapshot<?>>, Delta<?>> mDeltas = new HashMap<>();

    @Nullable
    protected BatteryMonitorCore mMonitor;
    @Nullable
    protected AppStats mAppStats;
    private long mBgnMillis = SystemClock.uptimeMillis();

    public CompositeMonitors(@Nullable BatteryMonitorCore core) {
        mMonitor = core;
    }

    public void clear() {
        mBgnSnapshots.clear();
        mDeltas.clear();
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

    public void putDelta(Class<? extends Snapshot<?>> snapshotClass, Delta<? extends Snapshot<?>> delta) {
        mDeltas.put(snapshotClass, delta);
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

    public void configureAllSnapshot() {
        mAppStats = null;
        mBgnMillis = SystemClock.uptimeMillis();
        for (Class<? extends Snapshot<?>> item : mMetrics) {
            statCurrSnapshot(item);
        }
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    public void configureDeltas() {
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
        mAppStats = AppStats.current(SystemClock.uptimeMillis() - mBgnMillis);
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

    @Override
    public String toString() {
        return "CompositeMonitors{" +
                "Metrics=" + mBgnSnapshots +
                ", BgnSnapshots=" + mMetrics +
                ", Deltas=" + mDeltas +
                '}';
    }
}
