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
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature;
import com.tencent.matrix.batterycanary.utils.PowerProfile;
import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import androidx.annotation.ChecksSdkIntAtLeast;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

/**
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

        public double calculatePower(long durationMs) {
            return mAveragePowerMahPerMs * durationMs;
        }
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

    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcCpuPower(PowerProfile powerProfile, HealthStats healthStats) {
        double power = getMeasure(healthStats, UidHealthStats.MEASUREMENT_CPU_POWER_MAMS) / UsageBasedPowerEstimator.MILLIS_IN_HOUR;
        if (power > 0) {
            MatrixLog.i(TAG, "estimate CPU by mams");
        } else if (power == 0) {
            long cpuTimeMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_USER_CPU_TIME_MS) + getMeasure(healthStats, UidHealthStats.MEASUREMENT_SYSTEM_CPU_TIME_MS);
            power = estimateCpuPowerByCpuStats(powerProfile, cpuTimeMs);
        }
        return power;
    }

    private static double estimateCpuPowerByCpuStats(PowerProfile powerProfile, long cpuTimeMs) {
        CpuStatFeature feat = BatteryCanary.getMonitorFeature(CpuStatFeature.class);
        if (feat != null && feat.isSupported()) {
            CpuStatFeature.CpuStateSnapshot cpuStateSnapshot = feat.currentCpuStateSnapshot();
            if (cpuStateSnapshot != null) {
                long jiffySum = 0;
                for (MonitorFeature.Snapshot.Entry.ListEntry<MonitorFeature.Snapshot.Entry.DigitEntry<Long>> stepJiffies : cpuStateSnapshot.procCpuCoreStates) {
                    for (MonitorFeature.Snapshot.Entry.DigitEntry<Long> item : stepJiffies.getList()) {
                        jiffySum += item.get();
                    }
                }
                double powerMah = 0;
                for (int i = 0; i < cpuStateSnapshot.procCpuCoreStates.size(); i++) {
                    List<MonitorFeature.Snapshot.Entry.DigitEntry<Long>> stepJiffies = cpuStateSnapshot.procCpuCoreStates.get(i).getList();
                    for (int j = 0; j < stepJiffies.size(); j++) {
                        long jiffy = stepJiffies.get(j).get();
                        long figuredCpuTimeMs = (long) ((jiffy * 1.0f / jiffySum) * cpuTimeMs);
                        double powerMa = powerProfile.getAveragePowerForCpuCore(i, j);
                        powerMah += new UsageBasedPowerEstimator(powerMa).calculatePower(figuredCpuTimeMs);
                    }
                }
                return powerMah;
            }
        }
        return 0;
    }

    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcWakelocksPower(PowerProfile powerProfile, HealthStats healthStats) {
        double power = 0;
        if (healthStats.hasTimers(UidHealthStats.TIMERS_WAKELOCKS_PARTIAL)) {
            Map<String, TimerStat> timers = healthStats.getTimers(UidHealthStats.TIMERS_WAKELOCKS_PARTIAL);
            long timeMs = 0;
            for (TimerStat item : timers.values()) {
                timeMs += item.getTime();
            }
            double powerMa = powerProfile.getAveragePower(PowerProfile.POWER_CPU_IDLE);
            power = new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }
        return power;
    }

    /**
     * @see com.android.internal.os.MobileRadioPowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcMobilePowerByRadioActive(PowerProfile powerProfile, HealthStats healthStats) {
        long timeMs = getTimerTime(healthStats, UidHealthStats.TIMER_MOBILE_RADIO_ACTIVE);
        double powerMa = powerProfile.getAveragePower(PowerProfile.POWER_RADIO_ACTIVE);
        return new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
    }

    /**
     * @see com.android.internal.os.MobileRadioPowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcMobilePower(PowerProfile powerProfile, HealthStats healthStats) {
        double power = getMeasure(healthStats, UidHealthStats.MEASUREMENT_MOBILE_POWER_MAMS) / UsageBasedPowerEstimator.MILLIS_IN_HOUR;
        if (power > 0) {
            MatrixLog.i(TAG, "estimate Mobile by mams");
        }
        if (power == 0) {
            {
                long timeMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_MOBILE_IDLE_MS);
                double powerMa = powerProfile.getAveragePower(PowerProfile.POWER_MODEM_CONTROLLER_IDLE);
                power += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
            }
            {
                long timeMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_MOBILE_RX_MS);
                double powerMa = powerProfile.getAveragePower(PowerProfile.POWER_MODEM_CONTROLLER_RX);
                power += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
            }
            {
                long timeMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_MOBILE_TX_MS);
                double powerMa = powerProfile.getAveragePower(PowerProfile.POWER_MODEM_CONTROLLER_TX);
                power += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
            }
        }
        return power;
    }

    /**
     * @see com.android.internal.os.WifiPowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcWifiPower(PowerProfile powerProfile, HealthStats healthStats) {
        double power = getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_POWER_MAMS) / UsageBasedPowerEstimator.MILLIS_IN_HOUR;
        if (power > 0) {
            MatrixLog.i(TAG, "estimate WIFI by mams");
        }
        if (power == 0) {
            {
                double wifiIdlePower = powerProfile.getAveragePower(PowerProfile.POWER_WIFI_CONTROLLER_IDLE);
                long idleMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_IDLE_MS);
                UsageBasedPowerEstimator etmWifiIdlePower = new UsageBasedPowerEstimator(wifiIdlePower);
                power += etmWifiIdlePower.calculatePower(idleMs);
            }
            {
                double wifiRxPower = powerProfile.getAveragePower(PowerProfile.POWER_WIFI_CONTROLLER_RX);
                UsageBasedPowerEstimator etmWifiRxPower = new UsageBasedPowerEstimator(wifiRxPower);
                long rxMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_RX_MS);
                power += etmWifiRxPower.calculatePower(rxMs);
            }
            {
                double wifiTxPower = powerProfile.getAveragePower(PowerProfile.POWER_WIFI_CONTROLLER_TX);
                long txMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_TX_MS);
                UsageBasedPowerEstimator etmWifiTxPower = new UsageBasedPowerEstimator(wifiTxPower);
                power += etmWifiTxPower.calculatePower(txMs);
            }
        }
        return power;
    }

    /**
     * @see com.android.internal.os.BluetoothPowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcBlueToothPower(PowerProfile powerProfile, HealthStats healthStats) {
        double power = getMeasure(healthStats, UidHealthStats.MEASUREMENT_BLUETOOTH_POWER_MAMS) / UsageBasedPowerEstimator.MILLIS_IN_HOUR;
        if (power > 0) {
            MatrixLog.i(TAG, "etmMobilePower BLE by mams");
        }
        if (power == 0) {
            {
                long timeMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_BLUETOOTH_IDLE_MS);
                double powerMa = powerProfile.getAveragePower(PowerProfile.POWER_BLUETOOTH_CONTROLLER_IDLE);
                power += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
            }
            {
                long timeMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_BLUETOOTH_RX_MS);
                double powerMa = powerProfile.getAveragePower(PowerProfile.POWER_BLUETOOTH_CONTROLLER_RX);
                power += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
            }
            {
                long timeMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_BLUETOOTH_TX_MS);
                double powerMa = powerProfile.getAveragePower(PowerProfile.POWER_BLUETOOTH_CONTROLLER_TX);
                power += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
            }
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
            if (powerProfile.getAveragePower(PowerProfile.POWER_GPS_OPERATING_VOLTAGE) > 0) {
                powerMa = powerProfile.getAveragePower(PowerProfile.POWER_GPS_ON);
                if (powerMa <= 0) {
                    int num = powerProfile.getNumElements(PowerProfile.POWER_GPS_SIGNAL_QUALITY_BASED);
                    double sumMa = 0;
                    for (int i = 0; i < num; i++) {
                        sumMa += powerProfile.getAveragePower(PowerProfile.POWER_GPS_SIGNAL_QUALITY_BASED, i);
                    }
                    powerMa = sumMa / num;
                }
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
        double powerMa = powerProfile.getAveragePower(PowerProfile.POWER_CAMERA);
        return new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
    }

    /**
     * @see com.android.internal.os.FlashlightPowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcFlashLightPower(PowerProfile powerProfile, HealthStats healthStats) {
        long timeMs = getTimerTime(healthStats, UidHealthStats.TIMER_FLASHLIGHT);
        double powerMa = powerProfile.getAveragePower(PowerProfile.POWER_FLASHLIGHT);
        return new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
    }

    /**
     * @see com.android.internal.os.MediaPowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcAudioPower(PowerProfile powerProfile, HealthStats healthStats) {
        long timeMs = getTimerTime(healthStats, UidHealthStats.TIMER_AUDIO);
        double powerMa = powerProfile.getAveragePower(PowerProfile.POWER_AUDIO);
        return new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
    }

    /**
     * @see com.android.internal.os.MediaPowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcVideoPower(PowerProfile powerProfile, HealthStats healthStats) {
        long timeMs = getTimerTime(healthStats, UidHealthStats.TIMER_VIDEO);
        double powerMa = powerProfile.getAveragePower(PowerProfile.POWER_VIDEO);
        return new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
    }

    /**
     * @see com.android.internal.os.ScreenPowerCalculator
     */
    @RequiresApi(api = Build.VERSION_CODES.N)
    public static double calcScreenPower(PowerProfile powerProfile, HealthStats healthStats) {
        long totalTimeMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_REALTIME_BATTERY_MS);
        long screenOffTimeMs = getMeasure(healthStats, UidHealthStats.MEASUREMENT_REALTIME_SCREEN_OFF_BATTERY_MS);
        long screenOnTimeMs = totalTimeMs - screenOffTimeMs;
        double powerMa = powerProfile.getAveragePower(PowerProfile.POWER_SCREEN_ON);
        return new UsageBasedPowerEstimator(powerMa).calculatePower(screenOnTimeMs);
    }
}
