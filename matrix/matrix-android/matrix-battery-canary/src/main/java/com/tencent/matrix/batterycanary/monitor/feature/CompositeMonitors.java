package com.tencent.matrix.batterycanary.monitor.feature;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.batterycanary.utils.Consumer;

import java.util.HashMap;
import java.util.Map;

import androidx.annotation.Nullable;

/**
 * @author Kaede
 * @since 2021/9/18
 */
public class CompositeMonitors {
    private final Map<Class<? extends Snapshot<?>>, Snapshot<?>> mBgnSnapshots = new HashMap<>();
    private final Map<Class<? extends Snapshot<?>>, Delta<?>> mDeltas = new HashMap<>();

    private final BatteryMonitorCore mMonitor;

    public CompositeMonitors(BatteryMonitorCore core) {
        mMonitor = core;
    }

    public void clear() {
        mBgnSnapshots.clear();
        mDeltas.clear();
    }

    @Nullable
    public <T extends MonitorFeature> T getFeature(Class<T> clazz) {
        for (MonitorFeature plugin : mMonitor.getConfig().features) {
            if (clazz.isAssignableFrom(plugin.getClass())) {
                //noinspection unchecked
                return (T) plugin;
            }
        }
        return null;
    }

    public <T extends MonitorFeature> void getFeature(Class<T> clazz, Consumer<T> block) {
        MonitorFeature feature = getFeature(clazz);
        if (feature != null) {
            //noinspection unchecked
            block.accept((T) feature);
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

    public void configureAllSnapshot() {
        statCurrSnapshot(AlarmMonitorFeature.AlarmSnapshot.class);
        statCurrSnapshot(BlueToothMonitorFeature.BlueToothSnapshot.class);
        statCurrSnapshot(DeviceStatMonitorFeature.CpuFreqSnapshot.class);
        statCurrSnapshot(DeviceStatMonitorFeature.BatteryTmpSnapshot.class);
        statCurrSnapshot(JiffiesMonitorFeature.JiffiesSnapshot.class);
        statCurrSnapshot(LocationMonitorFeature.LocationSnapshot.class);
        statCurrSnapshot(TrafficMonitorFeature.RadioStatSnapshot.class);
        statCurrSnapshot(WakeLockMonitorFeature.WakeLockSnapshot.class);
        statCurrSnapshot(WifiMonitorFeature.WifiSnapshot.class);
        statCurrSnapshot(CpuStatFeature.CpuStateSnapshot.class);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    public void configureDeltas() {
        for (Map.Entry<Class<? extends Snapshot<?>>, Snapshot<?>> item : mBgnSnapshots.entrySet()) {
            Snapshot lastSnapshot = item.getValue();
            if (lastSnapshot != null) {
                Class<? extends Snapshot<?>> snapshotClass = item.getKey();
                Snapshot currSnapshot = statCurrSnapshot(snapshotClass);
                if (currSnapshot != null && currSnapshot.getClass() == lastSnapshot.getClass()) {
                    mDeltas.put(snapshotClass, currSnapshot.diff(lastSnapshot));
                }
            }
        }
    }

    private Snapshot<?> statCurrSnapshot(Class<? extends Snapshot<?>> snapshotClass) {
        Snapshot<?> snapshot = null;
        if (snapshotClass == AlarmMonitorFeature.AlarmSnapshot.class) {
            AlarmMonitorFeature feature = mMonitor.getMonitorFeature(AlarmMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentAlarms();
                mBgnSnapshots.put(snapshotClass, snapshot);
            }
            return snapshot;
        }
        if (snapshotClass == BlueToothMonitorFeature.BlueToothSnapshot.class) {
            BlueToothMonitorFeature feature = mMonitor.getMonitorFeature(BlueToothMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentSnapshot();
                mBgnSnapshots.put(snapshotClass, snapshot);
            }
            return snapshot;
        }
        if (snapshotClass == DeviceStatMonitorFeature.CpuFreqSnapshot.class) {
            DeviceStatMonitorFeature feature = mMonitor.getMonitorFeature(DeviceStatMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentCpuFreq();
                mBgnSnapshots.put(snapshotClass, snapshot);
            }
            return snapshot;
        }
        if (snapshotClass == DeviceStatMonitorFeature.BatteryTmpSnapshot.class) {
            DeviceStatMonitorFeature feature = mMonitor.getMonitorFeature(DeviceStatMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentBatteryTemperature(mMonitor.getContext());
                mBgnSnapshots.put(snapshotClass, snapshot);
            }
            return snapshot;
        }
        if (snapshotClass == JiffiesMonitorFeature.JiffiesSnapshot.class) {
            JiffiesMonitorFeature feature = mMonitor.getMonitorFeature(JiffiesMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentJiffiesSnapshot();
                mBgnSnapshots.put(snapshotClass, snapshot);
            }
            return snapshot;
        }
        if (snapshotClass == LocationMonitorFeature.LocationSnapshot.class) {
            LocationMonitorFeature feature = mMonitor.getMonitorFeature(LocationMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentSnapshot();
                mBgnSnapshots.put(snapshotClass, snapshot);
            }
            return snapshot;
        }
        if (snapshotClass == TrafficMonitorFeature.RadioStatSnapshot.class) {
            TrafficMonitorFeature feature = mMonitor.getMonitorFeature(TrafficMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentRadioSnapshot(mMonitor.getContext());
                mBgnSnapshots.put(snapshotClass, snapshot);
            }
            return snapshot;
        }
        if (snapshotClass == WakeLockMonitorFeature.WakeLockSnapshot.class) {
            WakeLockMonitorFeature feature = mMonitor.getMonitorFeature(WakeLockMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentWakeLocks();
                mBgnSnapshots.put(snapshotClass, snapshot);
            }
            return snapshot;
        }
        if (snapshotClass == WifiMonitorFeature.WifiSnapshot.class) {
            WifiMonitorFeature feature = mMonitor.getMonitorFeature(WifiMonitorFeature.class);
            if (feature != null) {
                snapshot = feature.currentSnapshot();
                mBgnSnapshots.put(snapshotClass, snapshot);
            }
            return snapshot;
        }
        if (snapshotClass == CpuStatFeature.CpuStateSnapshot.class) {
            CpuStatFeature feature = mMonitor.getMonitorFeature(CpuStatFeature.class);
            if (feature != null && feature.isSupported()) {
                snapshot = feature.currentCpuStateSnapshot();
                mBgnSnapshots.put(snapshotClass, snapshot);
            }
            return snapshot;
        }
        return null;
    }
}
