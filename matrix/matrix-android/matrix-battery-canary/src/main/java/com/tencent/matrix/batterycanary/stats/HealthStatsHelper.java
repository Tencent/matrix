package com.tencent.matrix.batterycanary.stats;

import android.annotation.SuppressLint;
import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorManager;
import android.os.Build;
import android.os.health.HealthStats;
import android.os.health.SystemHealthManager;
import android.os.health.TimerStat;
import android.os.health.UidHealthStats;

import com.tencent.matrix.batterycanary.BatteryCanary;
import com.tencent.matrix.batterycanary.monitor.feature.CpuStatFeature;
import com.tencent.matrix.batterycanary.monitor.feature.CpuStatFeature.CpuStateSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.DigitEntry;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.ListEntry;
import com.tencent.matrix.batterycanary.utils.PowerProfile;
import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import androidx.annotation.ChecksSdkIntAtLeast;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

/**
 * totalPowerMah = usagePowerMah + wifiPowerMah + gpsPowerMah + cpuPowerMah +
 *                 sensorPowerMah + mobileRadioPowerMah + wakeLockPowerMah + cameraPowerMah +
 *                 flashlightPowerMah + bluetoothPowerMah + audioPowerMah + videoPowerMah
 *                 + systemServiceCpuPowerMah;
 * if (customMeasuredPowerMah != null) {
 *     for (int idx = 0; idx < customMeasuredPowerMah.length; idx++) {
 *         totalPowerMah += customMeasuredPowerMah[idx];
 *     }
 * }
 * // powerAttributedToOtherSippersMah is negative or zero
 * totalPowerMah = totalPowerMah + powerReattributedToOtherSippersMah;
 * totalSmearedPowerMah = totalPowerMah + screenPowerMah + proportionalSmearMah;
 *
 * @see com.android.internal.os.BatterySipper#sumPower
 * @see com.android.internal.os.BatteryStatsHelper
 * @see com.android.internal.os.BatteryStatsImpl.Uid
 *
 * @author Kaede
 * @since 6/7/2022
 */
@SuppressWarnings("JavadocReference")
@SuppressLint("RestrictedApi")
public final class HealthStatsHelper {
    public static final String TAG = "HealthStatsHelper";

    public static class UsageBasedPowerEstimator {
        private static final double MILLIS_IN_HOUR = 1000.0 * 60 * 60;
        private final double mAveragePowerMahPerMs;
        public UsageBasedPowerEstimator(double averagePowerMilliAmp) {
            mAveragePowerMahPerMs = averagePowerMilliAmp / MILLIS_IN_HOUR;
        }
        public boolean isSupported() {
            return mAveragePowerMahPerMs != 0;
        }
        public double calculatePower(long durationMs) {
            return mAveragePowerMahPerMs * durationMs;
        }
    }

    public static double round(double input, int decimalPlace) {
        double decimal = Math.pow(10.0, decimalPlace);
        return Math.round(input * decimal) / decimal;
    }

    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.N)
    public static boolean isSupported() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.N;
    }

    @Nullable
    public static HealthStats getCurrStats(Context context) {
        if (isSupported()) {
            try {
                SystemHealthManager shm = (SystemHealthManager) context.getSystemService(Context.SYSTEM_HEALTH_SERVICE);
                return shm.takeMyUidSnapshot();
            } catch (Exception e) {
                MatrixLog.w(TAG, "takeMyUidSnapshot err: " + e);
            }
        }
        return null;
    }

    @RequiresApi(api = Build.VERSION_CODES.N)
    static long getMeasure(HealthStats healthStats, int key) {
        if (healthStats.hasMeasurement(key)) {
            return healthStats.getMeasurement(key);
        }
        return 0L;
    }

    @RequiresApi(api = Build.VERSION_CODES.N)
    static long getTimerTime(HealthStats healthStats, int key) {
        if (healthStats.hasTimer(key)) {
            return healthStats.getTimerTime(key);
        }
        return 0L;
    }

    /**
     * @see com.android.internal.os.CpuPowerCalculator
     * @see com.android.internal.os.PowerProfile
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcCpuPower(PowerProfile powerProfile, HealthStats healthStats) {
        // double mams = getMeasure(healthStats, UidHealthStats.MEASUREMENT_CPU_POWER_MAMS) / (UsageBasedPowerEstimator.MILLIS_IN_HOUR * 1000L);
        // if (mams > 0) {
        //     MatrixLog.i(TAG, "estimate CPU by mams");
        //     return mams;
        // }
        double power = 0;
        /*
         * POWER_CPU_SUSPEND: Power consumption when CPU is in power collapse mode.
         * POWER_CPU_IDLE: Power consumption when CPU is awake (when a wake lock is held). This should
         *                 be zero on devices that can go into full CPU power collapse even when a wake
         *                 lock is held. Otherwise, this is the power consumption in addition to
         * POWER_CPU_SUSPEND due to a wake lock being held but with no CPU activity.
         * POWER_CPU_ACTIVE: Power consumption when CPU is running, excluding power consumed by clusters
         *                   and cores.
         *
         * CPU Power Equation (assume two clusters):
         * Total power = POWER_CPU_SUSPEND  (always added)
         *               + POWER_CPU_IDLE   (skip this and below if in power collapse mode)
         *               + POWER_CPU_ACTIVE (skip this and below if CPU is not running, but a wakelock
         *                                   is held)
         *               + cluster_power.cluster0 + cluster_power.cluster1 (skip cluster not running)
         *               + core_power.cluster0 * num running cores in cluster 0
         *               + core_power.cluster1 * num running cores in cluster 1
         */
        long cpuTimeMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_USER_CPU_TIME_MS) + getMeasure(healthStats, UidHealthStats.MEASUREMENT_SYSTEM_CPU_TIME_MS);
        power += estimateCpuActivePower(powerProfile, cpuTimeMs);
        CpuStatFeature feat = BatteryCanary.getMonitorFeature(CpuStatFeature.class);
        if (feat != null && feat.isSupported()) {
            CpuStateSnapshot snapshot = feat.currentCpuStateSnapshot();
            if (snapshot != null) {
                power += estimateCpuClustersPower(powerProfile, snapshot, cpuTimeMs, false);
                power += estimateCpuCoresPower(powerProfile, snapshot, cpuTimeMs, false);
            }
        }
        if (power > 0) {
            return power;
        }
        return 0;
    }

    @VisibleForTesting
    public static double estimateCpuActivePower(PowerProfile powerProfile, long cpuTimeMs) {
        //noinspection UnnecessaryLocalVariable
        long timeMs = cpuTimeMs;
        double powerMa = powerProfile.getAveragePowerUni(PowerProfile.POWER_CPU_ACTIVE);
        return new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
    }

    @VisibleForTesting
    public static double estimateCpuClustersPower(PowerProfile powerProfile, CpuStateSnapshot snapshot, long cpuTimeMs, boolean scaled) {
        boolean isUidStatsAvailable = false;
        for (ListEntry<DigitEntry<Long>> listEntry : snapshot.procCpuCoreStates) {
            for (DigitEntry<Long> item : listEntry.getList()) {
                if (item.get() > 0) {
                    isUidStatsAvailable = true;
                    break;
                }
            }
        }
        if (isUidStatsAvailable) {
            return estimateCpuClustersPowerByUidStats(powerProfile, snapshot, cpuTimeMs, scaled);
        } else {
            MatrixLog.i(TAG, "estimate CPU by device stats");
            return estimateCpuClustersPowerByDevStats(powerProfile, snapshot, cpuTimeMs);
        }
    }

    @VisibleForTesting
    public static double estimateCpuCoresPower(PowerProfile powerProfile, CpuStateSnapshot snapshot, long cpuTimeMs, boolean scaled) {
        boolean isUidStatsAvailable = false;
        for (ListEntry<DigitEntry<Long>> listEntry : snapshot.procCpuCoreStates) {
            for (DigitEntry<Long> item : listEntry.getList()) {
                if (item.get() > 0) {
                    isUidStatsAvailable = true;
                    break;
                }
            }
        }
        if (isUidStatsAvailable) {
            return estimateCpuCoresPowerByUidStats(powerProfile, snapshot, cpuTimeMs, scaled);
        } else {
            MatrixLog.i(TAG, "estimate CPU by device stats");
            return estimateCpuCoresPowerByDevStats(powerProfile, snapshot, cpuTimeMs);
        }
    }

    @VisibleForTesting
    public static double estimateCpuClustersPowerByUidStats(PowerProfile powerProfile, CpuStateSnapshot snapshot, long cpuTimeMs, boolean scaled) {
        if (cpuTimeMs > 0) {
            /*
             * procCpuCoreStates
             * [
             *     [step1Jiffies, step2Jiffies ...], // Cluster 1
             *     [step1Jiffies, step2Jiffies ...], // Cluster 2
             *     ...
             * ]
             */
            long jiffySum = 0;
            for (int i = 0; i < snapshot.procCpuCoreStates.size(); i++) {
                List<DigitEntry<Long>> stepJiffies = snapshot.procCpuCoreStates.get(i).getList();
                int scale = scaled ? powerProfile.getNumCoresInCpuCluster(i) : 1;
                for (DigitEntry<Long> item : stepJiffies) {
                    jiffySum += item.get() * scale;
                }
            }
            double powerMah = 0;
            for (int i = 0; i < snapshot.procCpuCoreStates.size(); i++) {
                List<DigitEntry<Long>> stepJiffies = snapshot.procCpuCoreStates.get(i).getList();
                int scale = scaled ? powerProfile.getNumCoresInCpuCluster(i) : 1;
                long jiffySumInCluster = 0;
                for (int j = 0; j < stepJiffies.size(); j++) {
                    long jiffy = stepJiffies.get(j).get();
                    jiffySumInCluster += jiffy * scale;
                }
                long figuredCpuTimeMs = (long) ((jiffySumInCluster * 1.0f / jiffySum) * cpuTimeMs);
                double powerMa = powerProfile.getAveragePowerForCpuCluster(i);
                powerMah += new UsageBasedPowerEstimator(powerMa).calculatePower(figuredCpuTimeMs);
            }
            return powerMah;
        }
        return 0;
    }

    @VisibleForTesting
    public static double estimateCpuCoresPowerByUidStats(PowerProfile powerProfile, CpuStateSnapshot snapshot, long cpuTimeMs, boolean scaled) {
        if (cpuTimeMs > 0) {
            /*
             * procCpuCoreStates
             * [
             *     [step1Jiffies, step2Jiffies ...], // Cluster 1
             *     [step1Jiffies, step2Jiffies ...], // Cluster 2
             *     ...
             * ]
             */
            long jiffySum = 0;
            for (int i = 0; i < snapshot.procCpuCoreStates.size(); i++) {
                List<DigitEntry<Long>> stepJiffies = snapshot.procCpuCoreStates.get(i).getList();
                int scale = scaled ? powerProfile.getNumCoresInCpuCluster(i) : 1;
                for (DigitEntry<Long> item : stepJiffies) {
                    jiffySum += item.get() * scale;
                }
            }
            double powerMah = 0;
            for (int i = 0; i < snapshot.procCpuCoreStates.size(); i++) {
                List<DigitEntry<Long>> stepJiffies = snapshot.procCpuCoreStates.get(i).getList();
                int scale = scaled ? powerProfile.getNumCoresInCpuCluster(i) : 1;
                for (int j = 0; j < stepJiffies.size(); j++) {
                    long jiffy = stepJiffies.get(j).get();
                    long figuredCpuTimeMs = (long) ((jiffy * scale * 1.0f / jiffySum) * cpuTimeMs);
                    double powerMa = powerProfile.getAveragePowerForCpuCore(i, j);
                    powerMah += new UsageBasedPowerEstimator(powerMa).calculatePower(figuredCpuTimeMs);
                }
            }
            return powerMah;
        }
        return 0;
    }

    @VisibleForTesting
    public static double estimateCpuClustersPowerByDevStats(PowerProfile powerProfile, CpuStateSnapshot snapshot, long cpuTimeMs) {
        if (cpuTimeMs > 0) {
            /*
             * cpuCoreStates
             * [
             *     [step1Jiffies, step2Jiffies ...], // CpuCore 1
             *     [step1Jiffies, step2Jiffies ...], // CpuCore 2
             *     ...
             * ]
             */
            long jiffySum = 0;
            for (int i = 0; i < snapshot.cpuCoreStates.size(); i++) {
                List<DigitEntry<Long>> stepJiffies = snapshot.cpuCoreStates.get(i).getList();
                for (DigitEntry<Long> item : stepJiffies) {
                    jiffySum += item.get();
                }
            }
            double powerMah = 0;
            for (int i = 0; i < snapshot.cpuCoreStates.size(); i++) {
                List<DigitEntry<Long>> stepJiffies = snapshot.cpuCoreStates.get(i).getList();
                long jiffySumInCluster = 0;
                for (int j = 0; j < stepJiffies.size(); j++) {
                    long jiffy = stepJiffies.get(j).get();
                    jiffySumInCluster += jiffy;
                }
                long figuredCpuTimeMs = (long) ((jiffySumInCluster * 1.0f / jiffySum) * cpuTimeMs);
                int clusterNum = powerProfile.getClusterByCpuNum(i);
                if (clusterNum >= 0 && clusterNum < powerProfile.getNumCpuClusters()) {
                    double powerMa = powerProfile.getAveragePowerForCpuCluster(clusterNum);
                    powerMah += new UsageBasedPowerEstimator(powerMa).calculatePower(figuredCpuTimeMs);
                }
            }
            return powerMah;
        }
        return 0;
    }

    @VisibleForTesting
    public static double estimateCpuCoresPowerByDevStats(PowerProfile powerProfile, CpuStateSnapshot snapshot, long cpuTimeMs) {
        if (cpuTimeMs > 0) {
            /*
             * cpuCoreStates
             * [
             *     [step1Jiffies, step2Jiffies ...], // CpuCore 1
             *     [step1Jiffies, step2Jiffies ...], // CpuCore 2
             *     ...
             * ]
             */
            long jiffySum = 0;
            for (int i = 0; i < snapshot.cpuCoreStates.size(); i++) {
                List<DigitEntry<Long>> stepJiffies = snapshot.cpuCoreStates.get(i).getList();
                for (DigitEntry<Long> item : stepJiffies) {
                    jiffySum += item.get();
                }
            }
            double powerMah = 0;
            for (int i = 0; i < snapshot.cpuCoreStates.size(); i++) {
                List<DigitEntry<Long>> stepJiffies = snapshot.cpuCoreStates.get(i).getList();
                for (int j = 0; j < stepJiffies.size(); j++) {
                    long jiffy = stepJiffies.get(j).get();
                    long figuredCpuTimeMs = (long) ((jiffy * 1.0f / jiffySum) * cpuTimeMs);
                    int clusterNum = powerProfile.getClusterByCpuNum(i);
                    if (clusterNum >= 0 && clusterNum < powerProfile.getNumCpuClusters()) {
                        double powerMa = powerProfile.getAveragePowerForCpuCore(clusterNum, j);
                        powerMah += new UsageBasedPowerEstimator(powerMa).calculatePower(figuredCpuTimeMs);
                    }
                }
            }
            return powerMah;
        }
        return 0;
    }

    /**
     * WIP
     *
     * @see com.android.internal.os.MemoryPowerCalculator
     */
    public static double calcMemoryPower(PowerProfile powerProfile) {
        double power = 0;
        int numBuckets = powerProfile.getNumElements(PowerProfile.POWER_MEMORY);
        for (int i = 0; i < numBuckets; i++) {
            long timeMs = 0; // TODO: Memory TimeStats supported, see "com.android.internal.os.KernelMemoryBandwidthStats"
            power += new UsageBasedPowerEstimator(powerProfile.getAveragePower(PowerProfile.POWER_MEMORY, i)).calculatePower(timeMs);
        }
        return power;
    }

    /**
     * @see com.android.internal.os.WakelockPowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcWakelocksPower(PowerProfile powerProfile, HealthStats healthStats) {
        double power = 0;
        if (healthStats.hasTimers(UidHealthStats.TIMERS_WAKELOCKS_PARTIAL)) {
            Map<String, TimerStat> timers = healthStats.getTimers(UidHealthStats.TIMERS_WAKELOCKS_PARTIAL);
            long timeMs = 0;
            for (TimerStat item : timers.values()) {
                timeMs += item.getTime();
            }
            double powerMa = powerProfile.getAveragePowerUni(PowerProfile.POWER_CPU_IDLE);
            power = new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }
        return power;
    }

    /**
     * @see com.android.internal.os.MobileRadioPowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcMobilePower(PowerProfile powerProfile, HealthStats healthStats) {
        // double mams = getMeasure(healthStats, UidHealthStats.MEASUREMENT_MOBILE_POWER_MAMS) / UsageBasedPowerEstimator.MILLIS_IN_HOUR;
        // if (mams > 0) {
        //     MatrixLog.i(TAG, "estimate Mobile by mams");
        //     return mams;
        // }
        double power = calcMobilePowerByRadioActive(powerProfile, healthStats);
        if (power > 0) {
            MatrixLog.i(TAG, "estimate Mobile by radioActive");
            return power;
        }
        power = calcMobilePowerByController(powerProfile, healthStats);
        if (power > 0) {
            MatrixLog.i(TAG, "estimate Mobile by controller");
            return power;
        }
        return 0;
    }

    @RequiresApi(api = Build.VERSION_CODES.N)
    @VisibleForTesting
    public static double calcMobilePowerByRadioActive(PowerProfile powerProfile, HealthStats healthStats) {
        // calc from radio active
        // for some aosp mistakes, radio active timer was given in time unit us:
        // https://cs.android.com/android/_/android/platform/frameworks/base/+/bee44ae8e5da109cd8273a057b566dc6925d6a71
        long timeMs = getTimerTime(healthStats, UidHealthStats.TIMER_MOBILE_RADIO_ACTIVE) / 1000;
        double powerMa = powerProfile.getAveragePowerUni(PowerProfile.POWER_RADIO_ACTIVE);
        if (powerMa <= 0) {
            double sum = 0;
            sum += powerProfile.getAveragePower(PowerProfile.POWER_MODEM_CONTROLLER_RX);
            int num = powerProfile.getNumElements(PowerProfile.POWER_MODEM_CONTROLLER_TX);
            for (int i = 0; i < num; i++) {
                sum += powerProfile.getAveragePower(PowerProfile.POWER_MODEM_CONTROLLER_TX, i);
            }
            powerMa = sum / (num + 1);
        }
        return new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
    }

    @RequiresApi(api = Build.VERSION_CODES.N)
    @VisibleForTesting
    public static double calcMobilePowerByController(PowerProfile powerProfile, HealthStats healthStats) {
        // calc from controller
        double power = 0;
        {
            long timeMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_MOBILE_IDLE_MS);
            double powerMa = powerProfile.getAveragePowerUni(PowerProfile.POWER_MODEM_CONTROLLER_IDLE);
            power += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }
        {
            long timeMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_MOBILE_RX_MS);
            double powerMa = powerProfile.getAveragePowerUni(PowerProfile.POWER_MODEM_CONTROLLER_RX);
            power += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }
        {
            long timeMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_MOBILE_TX_MS);
            double powerMa = powerProfile.getAveragePowerUni(PowerProfile.POWER_MODEM_CONTROLLER_TX);
            power += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }
        return power;
    }

    /**
     * @see com.android.internal.os.WifiPowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcWifiPower(PowerProfile powerProfile, HealthStats healthStats) {
        // double mams = getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_POWER_MAMS) / UsageBasedPowerEstimator.MILLIS_IN_HOUR;
        // if (mams > 0) {
        //     MatrixLog.i(TAG, "estimate WIFI by mams");
        //     return mams;
        // }
        double power = calcWifiPowerByController(powerProfile, healthStats);
        if (power > 0) {
            MatrixLog.i(TAG, "estimate WIFI by controller");
            return power;
        }
        power = calcWifiPowerByPackets(powerProfile, healthStats);
        if (power > 0) {
            MatrixLog.i(TAG, "estimate WIFI by packets");
            return power;
        }
        return 0;
    }

    @RequiresApi(api = Build.VERSION_CODES.N)
    @VisibleForTesting
    public static double calcWifiPowerByController(PowerProfile powerProfile, HealthStats healthStats) {
        // calc from controller
        double power = 0;
        {
            double wifiIdlePower = powerProfile.getAveragePowerUni(PowerProfile.POWER_WIFI_CONTROLLER_IDLE);
            long idleMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_IDLE_MS);
            UsageBasedPowerEstimator etmWifiIdlePower = new UsageBasedPowerEstimator(wifiIdlePower);
            power += etmWifiIdlePower.calculatePower(idleMs);
        }
        {
            double wifiRxPower = powerProfile.getAveragePowerUni(PowerProfile.POWER_WIFI_CONTROLLER_RX);
            UsageBasedPowerEstimator etmWifiRxPower = new UsageBasedPowerEstimator(wifiRxPower);
            long rxMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_RX_MS);
            power += etmWifiRxPower.calculatePower(rxMs);
        }
        {
            double wifiTxPower = powerProfile.getAveragePowerUni(PowerProfile.POWER_WIFI_CONTROLLER_TX);
            long txMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_TX_MS);
            UsageBasedPowerEstimator etmWifiTxPower = new UsageBasedPowerEstimator(wifiTxPower);
            power += etmWifiTxPower.calculatePower(txMs);
        }
        return power;
    }

    @RequiresApi(api = Build.VERSION_CODES.N)
    @VisibleForTesting
    public static double calcWifiPowerByPackets(PowerProfile powerProfile, HealthStats healthStats) {
        // calc from packets
        double power = 0;
        MatrixLog.i(TAG, "estimate WIFI by packets");
        {
            final long wifiBps = 1000000;
            final double averageWifiActivePower = powerProfile.getAveragePowerUni(PowerProfile.POWER_WIFI_ACTIVE) / 3600;
            double powerMaPerPacket = averageWifiActivePower / (((double) wifiBps) / 8 / 2048);
            long packets = getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_RX_PACKETS) + getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_TX_PACKETS);
            power += powerMaPerPacket * packets;
        }
        {
            double powerMa = powerProfile.getAveragePowerUni(PowerProfile.POWER_WIFI_ON);
            long timeMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_RUNNING_MS);
            power += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }
        {
            double powerMa = powerProfile.getAveragePowerUni(PowerProfile.POWER_WIFI_SCAN);
            long timeMs = getTimerTime(healthStats, UidHealthStats.TIMER_WIFI_SCAN);
            power += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }
        return power;
    }

    /**
     * @see com.android.internal.os.BluetoothPowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcBlueToothPower(PowerProfile powerProfile, HealthStats healthStats) {
        // double mams = getMeasure(healthStats, UidHealthStats.MEASUREMENT_BLUETOOTH_POWER_MAMS) / UsageBasedPowerEstimator.MILLIS_IN_HOUR;
        // if (mams > 0) {
        //     MatrixLog.i(TAG, "etmMobilePower BLE by mams");
        //     return mams;
        // }
        double power = 0;
        {
            long timeMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_BLUETOOTH_IDLE_MS);
            double powerMa = powerProfile.getAveragePowerUni(PowerProfile.POWER_BLUETOOTH_CONTROLLER_IDLE);
            power += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }
        {
            long timeMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_BLUETOOTH_RX_MS);
            double powerMa = powerProfile.getAveragePowerUni(PowerProfile.POWER_BLUETOOTH_CONTROLLER_RX);
            power += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }
        {
            long timeMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_BLUETOOTH_TX_MS);
            double powerMa = powerProfile.getAveragePowerUni(PowerProfile.POWER_BLUETOOTH_CONTROLLER_TX);
            power += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }
        return power;
    }

    /**
     * @see com.android.internal.os.GnssPowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcGpsPower(PowerProfile powerProfile, HealthStats healthStats) {
        long timeMs = getTimerTime(healthStats, UidHealthStats.TIMER_GPS_SENSOR);
        double powerMa = 0;
        if (timeMs > 0) {
            powerMa = powerProfile.getAveragePowerUni(PowerProfile.POWER_GPS_ON);
            if (powerMa <= 0) {
                int num = powerProfile.getNumElements(PowerProfile.POWER_GPS_SIGNAL_QUALITY_BASED);
                double sumMa = 0;
                for (int i = 0; i < num; i++) {
                    sumMa += powerProfile.getAveragePower(PowerProfile.POWER_GPS_SIGNAL_QUALITY_BASED, i);
                }
                powerMa = sumMa / num;
            }
        }
        return new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
    }

    /**
     * @see com.android.internal.os.SensorPowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcSensorsPower(Context context, HealthStats healthStats) {
        double power = 0;
        if (healthStats.hasTimers(UidHealthStats.TIMERS_SENSORS)) {
            SensorManager sm = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);
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

            Map<String, TimerStat> timers = healthStats.getTimers(UidHealthStats.TIMERS_SENSORS);
            for (Map.Entry<String, TimerStat> item : timers.entrySet()) {
                String handle = item.getKey();
                long timeMs = item.getValue().getTime();
                if (handle.equals("-10000")) {
                    continue; // skip GPS Sensors
                }
                Sensor sensor = sensorMap.get(handle);
                if (sensor != null) {
                    power += new UsageBasedPowerEstimator(sensor.getPower()).calculatePower(timeMs);
                }
            }
        }
        return power;
    }

    /**
     * @see com.android.internal.os.CameraPowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcCameraPower(PowerProfile powerProfile, HealthStats healthStats) {
        long timeMs = getTimerTime(healthStats, UidHealthStats.TIMER_CAMERA);
        double powerMa = powerProfile.getAveragePowerUni(PowerProfile.POWER_CAMERA);
        return new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
    }

    /**
     * @see com.android.internal.os.FlashlightPowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcFlashLightPower(PowerProfile powerProfile, HealthStats healthStats) {
        long timeMs = getTimerTime(healthStats, UidHealthStats.TIMER_FLASHLIGHT);
        double powerMa = powerProfile.getAveragePowerUni(PowerProfile.POWER_FLASHLIGHT);
        return new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
    }

    /**
     * @see com.android.internal.os.MediaPowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcAudioPower(PowerProfile powerProfile, HealthStats healthStats) {
        long timeMs = getTimerTime(healthStats, UidHealthStats.TIMER_AUDIO);
        double powerMa = powerProfile.getAveragePowerUni(PowerProfile.POWER_AUDIO);
        return new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
    }

    /**
     * @see com.android.internal.os.MediaPowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcVideoPower(PowerProfile powerProfile, HealthStats healthStats) {
        long timeMs = getTimerTime(healthStats, UidHealthStats.TIMER_VIDEO);
        double powerMa = powerProfile.getAveragePowerUni(PowerProfile.POWER_VIDEO);
        return new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
    }

    /**
     * @see com.android.internal.os.ScreenPowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcScreenPower(PowerProfile powerProfile, HealthStats healthStats) {
        long topAppMs = getTimerTime(healthStats, UidHealthStats.TIMER_PROCESS_STATE_TOP_MS);
        long fgActivityMs = getTimerTime(healthStats, UidHealthStats.TIMER_FOREGROUND_ACTIVITY);
        long screenOnTimeMs = Math.min(topAppMs, fgActivityMs);
        double powerMa = powerProfile.getAveragePowerUni(PowerProfile.POWER_SCREEN_ON);
        return new UsageBasedPowerEstimator(powerMa).calculatePower(screenOnTimeMs);
    }

    /**
     * @see com.android.internal.os.SystemServicePowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcSystemServicePower(PowerProfile powerProfile, HealthStats healthStats) {
        double power = 0;
        long timeMs = 0;
        if (healthStats.hasTimers(UidHealthStats.TIMERS_JOBS)) {
            Map<String, TimerStat> timers = healthStats.getTimers(UidHealthStats.TIMERS_JOBS);
            for (TimerStat item : timers.values()) {
                timeMs += item.getTime();
            }
        }
        if (healthStats.hasTimers(UidHealthStats.TIMERS_SYNCS)) {
            Map<String, TimerStat> timers = healthStats.getTimers(UidHealthStats.TIMERS_SYNCS);
            for (TimerStat item : timers.values()) {
                timeMs += item.getTime();
            }
        }

        power += estimateCpuActivePower(powerProfile, timeMs);
        CpuStatFeature feat = BatteryCanary.getMonitorFeature(CpuStatFeature.class);
        if (feat != null && feat.isSupported()) {
            CpuStateSnapshot snapshot = feat.currentCpuStateSnapshot();
            if (snapshot != null) {
                power += estimateCpuClustersPower(powerProfile, snapshot, timeMs, false);
                power += estimateCpuCoresPower(powerProfile, snapshot, timeMs, false);
            }
        }
        return power;
    }

    /**
     * @see com.android.internal.os.IdlePowerCalculator#IdlePowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcIdlePower(PowerProfile powerProfile, HealthStats healthStats) {
        long batteryRealtimeMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_REALTIME_BATTERY_MS);
        long batteryUptimeMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_UPTIME_BATTERY_MS);
        double suspendPowerMah = new UsageBasedPowerEstimator(powerProfile.getAveragePowerUni(PowerProfile.POWER_CPU_SUSPEND)).calculatePower(batteryRealtimeMs);
        double idlePowerMah = new UsageBasedPowerEstimator(powerProfile.getAveragePowerUni(PowerProfile.POWER_CPU_IDLE)).calculatePower(batteryUptimeMs);
        return suspendPowerMah + idlePowerMah;
    }
}
