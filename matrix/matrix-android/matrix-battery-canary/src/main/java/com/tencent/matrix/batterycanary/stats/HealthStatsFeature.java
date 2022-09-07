package com.tencent.matrix.batterycanary.stats;

import android.annotation.SuppressLint;
import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorManager;
import android.os.Build;
import android.os.health.HealthStats;
import android.os.health.PidHealthStats;
import android.os.health.ProcessHealthStats;
import android.os.health.TimerStat;
import android.os.health.UidHealthStats;

import com.tencent.matrix.batterycanary.monitor.feature.AbsMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.CpuStatFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.DigitEntry;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.PowerProfile;
import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Method;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

/**
 * @author Kaede
 * @since 18/7/2022
 */
public class HealthStatsFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.HealthStats";

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    public int weight() {
        return 0;
    }

    public HealthStats currHealthStats() {
        return HealthStatsHelper.getCurrStats(mCore.getContext());
    }

    @SuppressLint("VisibleForTests")
    public HealthStatsSnapshot currHealthStatsSnapshot() {
        HealthStatsSnapshot snapshot = new HealthStatsSnapshot();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
            return snapshot;
        }
        final HealthStats healthStats = currHealthStats();
        if (healthStats != null) {
            snapshot.healthStats = healthStats;

            // Power
            CpuStatFeature cpuStatFeat = mCore.getMonitorFeature(CpuStatFeature.class);
            if (cpuStatFeat != null) {
                PowerProfile powerProfile = cpuStatFeat.getPowerProfile();
                if (powerProfile != null && powerProfile.isSupported()) {
                    snapshot.cpuPower = DigitEntry.of(HealthStatsHelper.calcCpuPower(powerProfile, healthStats));
                    snapshot.wakelocksPower = DigitEntry.of(HealthStatsHelper.calcWakelocksPower(powerProfile, healthStats));
                    snapshot.mobilePower = DigitEntry.of(HealthStatsHelper.calcMobilePower(powerProfile, healthStats));
                    snapshot.wifiPower = DigitEntry.of(HealthStatsHelper.calcWifiPower(powerProfile, healthStats));
                    snapshot.blueToothPower = DigitEntry.of(HealthStatsHelper.calcBlueToothPower(powerProfile, healthStats));
                    snapshot.gpsPower = DigitEntry.of(HealthStatsHelper.calcGpsPower(powerProfile, healthStats));
                    snapshot.sensorsPower = DigitEntry.of(HealthStatsHelper.calcSensorsPower(mCore.getContext(), healthStats));
                    snapshot.cameraPower = DigitEntry.of(HealthStatsHelper.calcCameraPower(powerProfile, healthStats));
                    snapshot.flashLightPower = DigitEntry.of(HealthStatsHelper.calcFlashLightPower(powerProfile, healthStats));
                    snapshot.audioPower = DigitEntry.of(HealthStatsHelper.calcAudioPower(powerProfile, healthStats));
                    snapshot.videoPower = DigitEntry.of(HealthStatsHelper.calcVideoPower(powerProfile, healthStats));
                    snapshot.screenPower = DigitEntry.of(HealthStatsHelper.calcScreenPower(powerProfile, healthStats));
                    snapshot.systemServicePower = DigitEntry.of(HealthStatsHelper.calcSystemServicePower(powerProfile, healthStats));
                    snapshot.idlePower = DigitEntry.of(HealthStatsHelper.calcIdlePower(powerProfile, healthStats));
                }
            }

            // Meta data
            snapshot.cpuPowerMams = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_CPU_POWER_MAMS));
            snapshot.cpuUsrTimeMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_USER_CPU_TIME_MS));
            snapshot.cpuSysTimeMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_SYSTEM_CPU_TIME_MS));
            snapshot.realTimeMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_REALTIME_BATTERY_MS));
            snapshot.upTimeMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_UPTIME_BATTERY_MS));
            snapshot.offRealTimeMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_REALTIME_SCREEN_OFF_BATTERY_MS));
            snapshot.offUpTimeMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_UPTIME_SCREEN_OFF_BATTERY_MS));

            snapshot.mobilePowerMams = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_MOBILE_POWER_MAMS));
            snapshot.mobileRadioActiveMs = DigitEntry.of(HealthStatsHelper.getTimerTime(healthStats, UidHealthStats.TIMER_MOBILE_RADIO_ACTIVE) / 1000L);
            snapshot.mobileIdleMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_MOBILE_IDLE_MS));
            snapshot.mobileRxMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_MOBILE_RX_MS));
            snapshot.mobileTxMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_MOBILE_TX_MS));

            snapshot.mobileRxBytes = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_MOBILE_RX_BYTES));
            snapshot.mobileTxBytes = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_MOBILE_TX_BYTES));
            snapshot.mobileRxPackets = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_MOBILE_RX_PACKETS));
            snapshot.mobileTxPackets = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_MOBILE_TX_PACKETS));

            snapshot.wifiPowerMams = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_POWER_MAMS));
            snapshot.wifiIdleMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_IDLE_MS));
            snapshot.wifiRxMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_RX_MS));
            snapshot.wifiTxMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_TX_MS));
            snapshot.wifiRunningMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_RUNNING_MS));
            snapshot.wifiLockMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_FULL_LOCK_MS));
            snapshot.wifiScanMs = DigitEntry.of(HealthStatsHelper.getTimerTime(healthStats, UidHealthStats.TIMER_WIFI_SCAN));
            snapshot.wifiMulticastMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_MULTICAST_MS));
            snapshot.wifiRxBytes = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_RX_BYTES));
            snapshot.wifiTxBytes = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_TX_BYTES));
            snapshot.wifiRxPackets = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_RX_PACKETS));
            snapshot.wifiTxPackets = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_TX_PACKETS));

            snapshot.blueToothPowerMams = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_BLUETOOTH_POWER_MAMS));
            snapshot.blueToothIdleMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_BLUETOOTH_IDLE_MS));
            snapshot.blueToothRxMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_BLUETOOTH_RX_MS));
            snapshot.blueToothTxMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_BLUETOOTH_TX_MS));

            {
                if (healthStats.hasTimers(UidHealthStats.TIMERS_WAKELOCKS_PARTIAL)) {
                    Map<String, TimerStat> timers = healthStats.getTimers(UidHealthStats.TIMERS_WAKELOCKS_PARTIAL);
                    long timeMs = 0;
                    for (TimerStat item : timers.values()) {
                        timeMs += item.getTime();
                    }
                    snapshot.wakelocksPartialMs = DigitEntry.of(timeMs);
                }
                if (healthStats.hasTimers(UidHealthStats.TIMERS_WAKELOCKS_FULL)) {
                    Map<String, TimerStat> timers = healthStats.getTimers(UidHealthStats.TIMERS_WAKELOCKS_FULL);
                    long timeMs = 0;
                    for (TimerStat item : timers.values()) {
                        timeMs += item.getTime();
                    }
                    snapshot.wakelocksFullMs = DigitEntry.of(timeMs);
                }
                if (healthStats.hasTimers(UidHealthStats.TIMERS_WAKELOCKS_WINDOW)) {
                    Map<String, TimerStat> timers = healthStats.getTimers(UidHealthStats.TIMERS_WAKELOCKS_WINDOW);
                    long timeMs = 0;
                    for (TimerStat item : timers.values()) {
                        timeMs += item.getTime();
                    }
                    snapshot.wakelocksWindowMs = DigitEntry.of(timeMs);
                }
                if (healthStats.hasTimers(UidHealthStats.TIMERS_WAKELOCKS_DRAW)) {
                    Map<String, TimerStat> timers = healthStats.getTimers(UidHealthStats.TIMERS_WAKELOCKS_DRAW);
                    long timeMs = 0;
                    for (TimerStat item : timers.values()) {
                        timeMs += item.getTime();
                    }
                    snapshot.wakelocksDrawMs = DigitEntry.of(timeMs);
                }
                if (healthStats.hasStats(UidHealthStats.STATS_PIDS)) {
                    long sum = 0;
                    Map<String, HealthStats> pidStats = healthStats.getStats(UidHealthStats.STATS_PIDS);
                    for (HealthStats item : pidStats.values()) {
                        if (item.hasMeasurement(PidHealthStats.MEASUREMENT_WAKE_SUM_MS)) {
                            sum += item.getMeasurement(PidHealthStats.MEASUREMENT_WAKE_SUM_MS);
                        }
                    }
                    snapshot.wakelocksPidSum = DigitEntry.of(sum);
                }
            }
            snapshot.gpsMs = DigitEntry.of(HealthStatsHelper.getTimerTime(healthStats, UidHealthStats.TIMER_GPS_SENSOR));
            if (healthStats.hasTimers(UidHealthStats.TIMERS_SENSORS)) {
                SensorManager sm = (SensorManager) mCore.getContext().getSystemService(Context.SENSOR_SERVICE);
                List<Sensor> sensorList = sm.getSensorList(Sensor.TYPE_ALL);
                Map<String, Sensor> sensorMap = new HashMap<>();
                for (Sensor item : sensorList) {
                    try {
                        //noinspection JavaReflectionMemberAccess
                        @SuppressLint("DiscouragedPrivateApi")
                        Method method = item.getClass().getDeclaredMethod("getHandle");
                        //noinspection ConstantConditions
                        int handle = (int) method.invoke(item);
                        sensorMap.put(String.valueOf(handle), item);
                    } catch (Throwable e) {
                        MatrixLog.w(TAG, "getSensorHandle err: " + e.getMessage());
                    }
                }

                long sensorsPowerMams = 0;
                Map<String, TimerStat> timers = healthStats.getTimers(UidHealthStats.TIMERS_SENSORS);
                for (Map.Entry<String, TimerStat> item : timers.entrySet()) {
                    String handle = item.getKey();
                    long timeMs = item.getValue().getTime();
                    if (handle.equals("-10000")) {
                        continue; // skip GPS Sensors
                    }
                    Sensor sensor = sensorMap.get(handle);
                    if (sensor != null) {
                        sensorsPowerMams += sensor.getPower() * timeMs;
                    }
                }
                snapshot.sensorsPowerMams = DigitEntry.of(sensorsPowerMams);
            }
            snapshot.cameraMs = DigitEntry.of(HealthStatsHelper.getTimerTime(healthStats, UidHealthStats.TIMER_CAMERA));
            snapshot.flashLightMs = DigitEntry.of(HealthStatsHelper.getTimerTime(healthStats, UidHealthStats.TIMER_FLASHLIGHT));
            snapshot.audioMs = DigitEntry.of(HealthStatsHelper.getTimerTime(healthStats, UidHealthStats.TIMER_AUDIO));
            snapshot.videoMs = DigitEntry.of(HealthStatsHelper.getTimerTime(healthStats, UidHealthStats.TIMER_VIDEO));
            if (healthStats.hasTimers(UidHealthStats.TIMERS_JOBS)) {
                long timeMs = 0;
                Map<String, TimerStat> timers = healthStats.getTimers(UidHealthStats.TIMERS_JOBS);
                for (TimerStat item : timers.values()) {
                    timeMs += item.getTime();
                }
                snapshot.jobsMs = DigitEntry.of(timeMs);
            }
            if (healthStats.hasTimers(UidHealthStats.TIMERS_SYNCS)) {
                long timeMs = 0;
                Map<String, TimerStat> timers = healthStats.getTimers(UidHealthStats.TIMERS_SYNCS);
                for (TimerStat item : timers.values()) {
                    timeMs += item.getTime();
                }
                snapshot.syncMs = DigitEntry.of(timeMs);
            }

            snapshot.fgActMs = DigitEntry.of(HealthStatsHelper.getTimerTime(healthStats, UidHealthStats.TIMER_FOREGROUND_ACTIVITY));
            snapshot.procTopAppMs = DigitEntry.of(HealthStatsHelper.getTimerTime(healthStats, UidHealthStats.TIMER_PROCESS_STATE_TOP_MS));
            snapshot.procTopSleepMs = DigitEntry.of(HealthStatsHelper.getTimerTime(healthStats, UidHealthStats.TIMER_PROCESS_STATE_TOP_SLEEPING_MS));
            snapshot.procFgMs = DigitEntry.of(HealthStatsHelper.getTimerTime(healthStats, UidHealthStats.TIMER_PROCESS_STATE_FOREGROUND_MS));
            snapshot.procFgSrvMs = DigitEntry.of(HealthStatsHelper.getTimerTime(healthStats, UidHealthStats.TIMER_PROCESS_STATE_FOREGROUND_SERVICE_MS));
            snapshot.procBgMs = DigitEntry.of(HealthStatsHelper.getTimerTime(healthStats, UidHealthStats.TIMER_PROCESS_STATE_BACKGROUND_MS));
            snapshot.procCacheMs = DigitEntry.of(HealthStatsHelper.getTimerTime(healthStats, UidHealthStats.TIMER_PROCESS_STATE_CACHED_MS));

            {
                if (healthStats.hasStats(UidHealthStats.STATS_PROCESSES)) {
                    Map<String, HealthStats> processes = healthStats.getStats(UidHealthStats.STATS_PROCESSES);
                    for (Map.Entry<String, HealthStats> item : processes.entrySet()) {
                        String pkg = item.getKey();
                        HealthStats procStats = item.getValue();
                        if (procStats != null) {
                            if (snapshot.procStatsCpuUsrTimeMs.isEmpty()) {
                                snapshot.procStatsCpuUsrTimeMs = new HashMap<>();
                            }
                            snapshot.procStatsCpuUsrTimeMs.put(pkg, DigitEntry.of(HealthStatsHelper.getMeasure(procStats, ProcessHealthStats.MEASUREMENT_USER_TIME_MS)));

                            if (snapshot.procStatsCpuSysTimeMs.isEmpty()) {
                                snapshot.procStatsCpuSysTimeMs = new HashMap<>();
                            }
                            snapshot.procStatsCpuSysTimeMs.put(pkg, DigitEntry.of(HealthStatsHelper.getMeasure(procStats, ProcessHealthStats.MEASUREMENT_SYSTEM_TIME_MS)));

                            if (snapshot.procStatsCpuFgTimeMs.isEmpty()) {
                                snapshot.procStatsCpuFgTimeMs = new HashMap<>();
                            }
                            snapshot.procStatsCpuFgTimeMs.put(pkg, DigitEntry.of(HealthStatsHelper.getMeasure(procStats, ProcessHealthStats.MEASUREMENT_FOREGROUND_MS)));
                        }
                    }
                }
            }
        }
        return snapshot;
    }

    public static class HealthStatsSnapshot extends Snapshot<HealthStatsSnapshot> {
        @VisibleForTesting
        @Nullable
        public HealthStats healthStats;
        public Map<String, Object> extras = Collections.emptyMap();

        // Estimated Powers
        public DigitEntry<Double> cpuPower = DigitEntry.of(0D);
        public DigitEntry<Double> wakelocksPower = DigitEntry.of(0D);
        public DigitEntry<Double> mobilePower = DigitEntry.of(0D);
        public DigitEntry<Double> wifiPower = DigitEntry.of(0D);
        public DigitEntry<Double> blueToothPower = DigitEntry.of(0D);
        public DigitEntry<Double> gpsPower = DigitEntry.of(0D);
        public DigitEntry<Double> sensorsPower = DigitEntry.of(0D);
        public DigitEntry<Double> cameraPower = DigitEntry.of(0D);
        public DigitEntry<Double> flashLightPower = DigitEntry.of(0D);
        public DigitEntry<Double> audioPower = DigitEntry.of(0D);
        public DigitEntry<Double> videoPower = DigitEntry.of(0D);
        public DigitEntry<Double> screenPower = DigitEntry.of(0D);
        public DigitEntry<Double> systemServicePower = DigitEntry.of(0D); // WIP
        public DigitEntry<Double> idlePower = DigitEntry.of(0D);

        // Meta Data:
        // CPU
        public DigitEntry<Long> cpuPowerMams = DigitEntry.of(0L);
        public DigitEntry<Long> cpuUsrTimeMs = DigitEntry.of(0L);
        public DigitEntry<Long> cpuSysTimeMs = DigitEntry.of(0L);
        public DigitEntry<Long> realTimeMs = DigitEntry.of(0L);
        public DigitEntry<Long> upTimeMs = DigitEntry.of(0L);
        public DigitEntry<Long> offRealTimeMs = DigitEntry.of(0L);
        public DigitEntry<Long> offUpTimeMs = DigitEntry.of(0L);

        // Network
        public DigitEntry<Long> mobilePowerMams = DigitEntry.of(0L);
        public DigitEntry<Long> mobileRadioActiveMs = DigitEntry.of(0L);
        public DigitEntry<Long> mobileIdleMs = DigitEntry.of(0L);
        public DigitEntry<Long> mobileRxMs = DigitEntry.of(0L);
        public DigitEntry<Long> mobileTxMs = DigitEntry.of(0L);
        public DigitEntry<Long> mobileRxBytes = DigitEntry.of(0L);
        public DigitEntry<Long> mobileTxBytes = DigitEntry.of(0L);
        public DigitEntry<Long> mobileRxPackets = DigitEntry.of(0L);
        public DigitEntry<Long> mobileTxPackets = DigitEntry.of(0L);

        public DigitEntry<Long> wifiPowerMams = DigitEntry.of(0L);
        public DigitEntry<Long> wifiIdleMs = DigitEntry.of(0L);
        public DigitEntry<Long> wifiRxMs = DigitEntry.of(0L);
        public DigitEntry<Long> wifiTxMs = DigitEntry.of(0L);
        public DigitEntry<Long> wifiRunningMs = DigitEntry.of(0L);
        public DigitEntry<Long> wifiLockMs = DigitEntry.of(0L);
        public DigitEntry<Long> wifiScanMs = DigitEntry.of(0L);
        public DigitEntry<Long> wifiMulticastMs = DigitEntry.of(0L);
        public DigitEntry<Long> wifiRxBytes = DigitEntry.of(0L);
        public DigitEntry<Long> wifiTxBytes = DigitEntry.of(0L);
        public DigitEntry<Long> wifiRxPackets = DigitEntry.of(0L);
        public DigitEntry<Long> wifiTxPackets = DigitEntry.of(0L);

        public DigitEntry<Long> blueToothPowerMams = DigitEntry.of(0L);
        public DigitEntry<Long> blueToothIdleMs = DigitEntry.of(0L);
        public DigitEntry<Long> blueToothRxMs = DigitEntry.of(0L);
        public DigitEntry<Long> blueToothTxMs = DigitEntry.of(0L);

        // SystemService & Media
        public DigitEntry<Long> wakelocksPartialMs = DigitEntry.of(0L);
        public DigitEntry<Long> wakelocksFullMs = DigitEntry.of(0L);
        public DigitEntry<Long> wakelocksWindowMs = DigitEntry.of(0L);
        public DigitEntry<Long> wakelocksDrawMs = DigitEntry.of(0L);
        public DigitEntry<Long> wakelocksPidSum = DigitEntry.of(0L);
        public DigitEntry<Long> gpsMs = DigitEntry.of(0L);
        public DigitEntry<Long> sensorsPowerMams = DigitEntry.of(0L);
        public DigitEntry<Long> cameraMs = DigitEntry.of(0L);
        public DigitEntry<Long> flashLightMs = DigitEntry.of(0L);
        public DigitEntry<Long> audioMs = DigitEntry.of(0L);
        public DigitEntry<Long> videoMs = DigitEntry.of(0L);
        public DigitEntry<Long> jobsMs = DigitEntry.of(0L);
        public DigitEntry<Long> syncMs = DigitEntry.of(0L);

        // Foreground
        public DigitEntry<Long> fgActMs = DigitEntry.of(0L);
        public DigitEntry<Long> procTopAppMs = DigitEntry.of(0L);
        public DigitEntry<Long> procTopSleepMs = DigitEntry.of(0L);
        public DigitEntry<Long> procFgMs = DigitEntry.of(0L);
        public DigitEntry<Long> procFgSrvMs = DigitEntry.of(0L);
        public DigitEntry<Long> procBgMs = DigitEntry.of(0L);
        public DigitEntry<Long> procCacheMs = DigitEntry.of(0L);

        // Process Stats
        public Map<String, DigitEntry<Long>> procStatsCpuUsrTimeMs = Collections.emptyMap();
        public Map<String, DigitEntry<Long>> procStatsCpuSysTimeMs = Collections.emptyMap();
        public Map<String, DigitEntry<Long>> procStatsCpuFgTimeMs = Collections.emptyMap();

        public double getTotalPower() {
            return cpuPower.get()
                    + wakelocksPower.get()
                    + mobilePower.get()
                    + wifiPower.get()
                    + blueToothPower.get()
                    + gpsPower.get()
                    + sensorsPower.get()
                    + flashLightPower.get()
                    + audioPower.get()
                    + videoPower.get()
                    + screenPower.get()
                    // + cameraPower.get()        // WIP
                    // + systemServicePower.get() // WIP
                    + idlePower.get();
        }

        @Override
        public Delta<HealthStatsSnapshot> diff(HealthStatsSnapshot bgn) {
            return new Delta<HealthStatsSnapshot>(bgn, this) {
                @Override
                protected HealthStatsSnapshot computeDelta() {
                    HealthStatsSnapshot delta = new HealthStatsSnapshot();
                    // For test & tunings
                    delta.extras = new LinkedHashMap<>();

                    // UID
                    delta.cpuPower = Differ.DigitDiffer.globalDiff(bgn.cpuPower, end.cpuPower);
                    delta.wakelocksPower = Differ.DigitDiffer.globalDiff(bgn.wakelocksPower, end.wakelocksPower);
                    delta.mobilePower = Differ.DigitDiffer.globalDiff(bgn.mobilePower, end.mobilePower);
                    delta.wifiPower = Differ.DigitDiffer.globalDiff(bgn.wifiPower, end.wifiPower);
                    delta.blueToothPower = Differ.DigitDiffer.globalDiff(bgn.blueToothPower, end.blueToothPower);
                    delta.gpsPower = Differ.DigitDiffer.globalDiff(bgn.gpsPower, end.gpsPower);
                    delta.sensorsPower = Differ.DigitDiffer.globalDiff(bgn.sensorsPower, end.sensorsPower);
                    delta.cameraPower = Differ.DigitDiffer.globalDiff(bgn.cameraPower, end.cameraPower);
                    delta.flashLightPower = Differ.DigitDiffer.globalDiff(bgn.flashLightPower, end.flashLightPower);
                    delta.audioPower = Differ.DigitDiffer.globalDiff(bgn.audioPower, end.audioPower);
                    delta.videoPower = Differ.DigitDiffer.globalDiff(bgn.videoPower, end.videoPower);
                    delta.screenPower = Differ.DigitDiffer.globalDiff(bgn.screenPower, end.screenPower);
                    delta.systemServicePower = Differ.DigitDiffer.globalDiff(bgn.systemServicePower, end.systemServicePower);
                    delta.idlePower = Differ.DigitDiffer.globalDiff(bgn.idlePower, end.idlePower);

                    delta.cpuPowerMams = Differ.DigitDiffer.globalDiff(bgn.cpuPowerMams, end.cpuPowerMams);
                    delta.cpuUsrTimeMs = Differ.DigitDiffer.globalDiff(bgn.cpuUsrTimeMs, end.cpuUsrTimeMs);
                    delta.cpuSysTimeMs = Differ.DigitDiffer.globalDiff(bgn.cpuSysTimeMs, end.cpuSysTimeMs);
                    delta.realTimeMs = Differ.DigitDiffer.globalDiff(bgn.realTimeMs, end.realTimeMs);
                    delta.upTimeMs = Differ.DigitDiffer.globalDiff(bgn.upTimeMs, end.upTimeMs);

                    delta.mobilePowerMams = Differ.DigitDiffer.globalDiff(bgn.mobilePowerMams, end.mobilePowerMams);
                    delta.mobileRadioActiveMs = Differ.DigitDiffer.globalDiff(bgn.mobileRadioActiveMs, end.mobileRadioActiveMs);
                    delta.mobileIdleMs = Differ.DigitDiffer.globalDiff(bgn.mobileIdleMs, end.mobileIdleMs);
                    delta.mobileRxMs = Differ.DigitDiffer.globalDiff(bgn.mobileRxMs, end.mobileRxMs);
                    delta.mobileTxMs = Differ.DigitDiffer.globalDiff(bgn.mobileTxMs, end.mobileTxMs);
                    delta.mobileRxBytes = Differ.DigitDiffer.globalDiff(bgn.mobileRxBytes, end.mobileRxBytes);
                    delta.mobileTxBytes = Differ.DigitDiffer.globalDiff(bgn.mobileTxBytes, end.mobileTxBytes);
                    delta.mobileRxPackets = Differ.DigitDiffer.globalDiff(bgn.mobileRxPackets, end.mobileRxPackets);
                    delta.mobileTxPackets = Differ.DigitDiffer.globalDiff(bgn.mobileTxPackets, end.mobileTxPackets);


                    delta.wifiPowerMams = Differ.DigitDiffer.globalDiff(bgn.wifiPowerMams, end.wifiPowerMams);
                    delta.wifiIdleMs = Differ.DigitDiffer.globalDiff(bgn.wifiIdleMs, end.wifiIdleMs);
                    delta.wifiRxMs = Differ.DigitDiffer.globalDiff(bgn.wifiRxMs, end.wifiRxMs);
                    delta.wifiTxMs = Differ.DigitDiffer.globalDiff(bgn.wifiTxMs, end.wifiTxMs);
                    delta.wifiRunningMs = Differ.DigitDiffer.globalDiff(bgn.wifiRunningMs, end.wifiRunningMs);
                    delta.wifiLockMs = Differ.DigitDiffer.globalDiff(bgn.wifiLockMs, end.wifiLockMs);
                    delta.wifiScanMs = Differ.DigitDiffer.globalDiff(bgn.wifiScanMs, end.wifiScanMs);
                    delta.wifiMulticastMs = Differ.DigitDiffer.globalDiff(bgn.wifiMulticastMs, end.wifiMulticastMs);
                    delta.wifiRxBytes = Differ.DigitDiffer.globalDiff(bgn.wifiRxBytes, end.wifiRxBytes);
                    delta.wifiTxBytes = Differ.DigitDiffer.globalDiff(bgn.wifiTxBytes, end.wifiTxBytes);
                    delta.wifiRxPackets = Differ.DigitDiffer.globalDiff(bgn.wifiRxPackets, end.wifiRxPackets);
                    delta.wifiTxPackets = Differ.DigitDiffer.globalDiff(bgn.wifiTxPackets, end.wifiTxPackets);

                    delta.blueToothPowerMams = Differ.DigitDiffer.globalDiff(bgn.blueToothPowerMams, end.blueToothPowerMams);
                    delta.blueToothIdleMs = Differ.DigitDiffer.globalDiff(bgn.blueToothIdleMs, end.blueToothIdleMs);
                    delta.blueToothRxMs = Differ.DigitDiffer.globalDiff(bgn.blueToothRxMs, end.blueToothRxMs);
                    delta.blueToothTxMs = Differ.DigitDiffer.globalDiff(bgn.blueToothTxMs, end.blueToothTxMs);

                    delta.wakelocksPartialMs = Differ.DigitDiffer.globalDiff(bgn.wakelocksPartialMs, end.wakelocksPartialMs);
                    delta.wakelocksFullMs = Differ.DigitDiffer.globalDiff(bgn.wakelocksFullMs, end.wakelocksFullMs);
                    delta.wakelocksWindowMs = Differ.DigitDiffer.globalDiff(bgn.wakelocksWindowMs, end.wakelocksWindowMs);
                    delta.wakelocksDrawMs = Differ.DigitDiffer.globalDiff(bgn.wakelocksDrawMs, end.wakelocksDrawMs);
                    delta.wakelocksPidSum = Differ.DigitDiffer.globalDiff(bgn.wakelocksPidSum, end.wakelocksPidSum);
                    delta.gpsMs = Differ.DigitDiffer.globalDiff(bgn.gpsMs, end.gpsMs);
                    delta.sensorsPowerMams = Differ.DigitDiffer.globalDiff(bgn.sensorsPowerMams, end.sensorsPowerMams);
                    delta.cameraMs = Differ.DigitDiffer.globalDiff(bgn.cameraMs, end.cameraMs);
                    delta.flashLightMs = Differ.DigitDiffer.globalDiff(bgn.flashLightMs, end.flashLightMs);
                    delta.audioMs = Differ.DigitDiffer.globalDiff(bgn.audioMs, end.audioMs);
                    delta.videoMs = Differ.DigitDiffer.globalDiff(bgn.videoMs, end.videoMs);
                    delta.jobsMs = Differ.DigitDiffer.globalDiff(bgn.jobsMs, end.jobsMs);
                    delta.syncMs = Differ.DigitDiffer.globalDiff(bgn.syncMs, end.syncMs);

                    delta.fgActMs = Differ.DigitDiffer.globalDiff(bgn.fgActMs, end.fgActMs);
                    delta.procTopAppMs = Differ.DigitDiffer.globalDiff(bgn.procTopAppMs, end.procTopAppMs);
                    delta.procTopSleepMs = Differ.DigitDiffer.globalDiff(bgn.procTopSleepMs, end.procTopSleepMs);
                    delta.procFgMs = Differ.DigitDiffer.globalDiff(bgn.procFgMs, end.procFgMs);
                    delta.procFgSrvMs = Differ.DigitDiffer.globalDiff(bgn.procFgSrvMs, end.procFgSrvMs);
                    delta.procBgMs = Differ.DigitDiffer.globalDiff(bgn.procBgMs, end.procBgMs);
                    delta.procCacheMs = Differ.DigitDiffer.globalDiff(bgn.procCacheMs, end.procCacheMs);

                    // PID
                    {
                        Map<String, DigitEntry<Long>> map = new HashMap<>();
                        for (Map.Entry<String, DigitEntry<Long>> entry : end.procStatsCpuUsrTimeMs.entrySet()) {
                            String key = entry.getKey();
                            DigitEntry<Long> endEntry = entry.getValue();
                            long bgnValue = 0L;
                            DigitEntry<Long> bgnEntry = bgn.procStatsCpuUsrTimeMs.get(key);
                            if (bgnEntry != null) {
                                bgnValue = bgnEntry.get();
                            }
                            map.put(key, Differ.DigitDiffer.globalDiff(DigitEntry.of(bgnValue), endEntry));
                        }
                        delta.procStatsCpuUsrTimeMs = decrease(map);
                    }
                    {
                        Map<String, DigitEntry<Long>> map = new HashMap<>();
                        for (Map.Entry<String, DigitEntry<Long>> entry : end.procStatsCpuSysTimeMs.entrySet()) {
                            String key = entry.getKey();
                            DigitEntry<Long> endEntry = entry.getValue();
                            long bgnValue = 0L;
                            DigitEntry<Long> bgnEntry = bgn.procStatsCpuSysTimeMs.get(key);
                            if (bgnEntry != null) {
                                bgnValue = bgnEntry.get();
                            }
                            map.put(key, Differ.DigitDiffer.globalDiff(DigitEntry.of(bgnValue), endEntry));
                        }
                        delta.procStatsCpuSysTimeMs = decrease(map);
                    }
                    {
                        Map<String, DigitEntry<Long>> map = new HashMap<>();
                        for (Map.Entry<String, DigitEntry<Long>> entry : end.procStatsCpuFgTimeMs.entrySet()) {
                            String key = entry.getKey();
                            DigitEntry<Long> endEntry = entry.getValue();
                            long bgnValue = 0L;
                            DigitEntry<Long> bgnEntry = bgn.procStatsCpuFgTimeMs.get(key);
                            if (bgnEntry != null) {
                                bgnValue = bgnEntry.get();
                            }
                            map.put(key, Differ.DigitDiffer.globalDiff(DigitEntry.of(bgnValue), endEntry));
                        }
                        delta.procStatsCpuFgTimeMs = decrease(map);
                    }
                    return delta;
                }

                private Map<String, DigitEntry<Long>> decrease(Map<String, DigitEntry<Long>> input) {
                    return BatteryCanaryUtil.sortMapByValue(input, new Comparator<Map.Entry<String, DigitEntry<Long>>>() {
                        @Override
                        public int compare(Map.Entry<String, DigitEntry<Long>> o1, Map.Entry<String, DigitEntry<Long>> o2) {
                            long sumLeft = o1.getValue().get(), sumRight = o2.getValue().get();
                            long minus = sumLeft - sumRight;
                            if (minus == 0) return 0;
                            if (minus > 0) return -1;
                            return 1;
                        }
                    });
                }
            };
        }

        public static double getPower(@NonNull Map<String, ?> extra, String key) {
            Object val = extra.get(key);
            if (val instanceof Double) {
                return (double) val;
            }
            return 0;
        }
    }
}
