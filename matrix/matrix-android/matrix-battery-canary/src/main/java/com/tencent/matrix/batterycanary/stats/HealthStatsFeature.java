package com.tencent.matrix.batterycanary.stats;

import android.annotation.SuppressLint;
import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorManager;
import android.os.Build;
import android.os.health.HealthStats;
import android.os.health.TimerStat;
import android.os.health.UidHealthStats;

import com.tencent.matrix.batterycanary.monitor.feature.AbsMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.CpuStatFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.DigitEntry;
import com.tencent.matrix.batterycanary.utils.PowerProfile;
import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

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
                }
            }

            // Meta data
            snapshot.cpuPowerMams = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_CPU_POWER_MAMS));
            snapshot.cpuUsrTimeMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_USER_CPU_TIME_MS));
            snapshot.cpuSysTimeMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_SYSTEM_CPU_TIME_MS));

            if (healthStats.hasTimers(UidHealthStats.TIMERS_WAKELOCKS_PARTIAL)) {
                Map<String, TimerStat> timers = healthStats.getTimers(UidHealthStats.TIMERS_WAKELOCKS_PARTIAL);
                long timeMs = 0;
                for (TimerStat item : timers.values()) {
                    timeMs += item.getTime();
                }
                snapshot.wakelocksPartialMs = DigitEntry.of(timeMs);
            }

            snapshot.mobilePowerMams = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_MOBILE_POWER_MAMS));
            snapshot.mobileRadioActiveMs = DigitEntry.of(HealthStatsHelper.getTimerTime(healthStats, UidHealthStats.TIMER_MOBILE_RADIO_ACTIVE));
            snapshot.mobileIdleMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_MOBILE_IDLE_MS));
            snapshot.mobileRxMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_MOBILE_RX_MS));
            snapshot.mobileTxMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_MOBILE_TX_MS));

            snapshot.wifiPowerMams = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_POWER_MAMS));
            snapshot.wifiIdleMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_IDLE_MS));
            snapshot.wifiRxMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_RX_MS));
            snapshot.wifiTxMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_WIFI_TX_MS));

            snapshot.blueToothPowerMams = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_BLUETOOTH_POWER_MAMS));
            snapshot.blueToothIdleMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_BLUETOOTH_IDLE_MS));
            snapshot.blueToothRxMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_BLUETOOTH_RX_MS));
            snapshot.blueToothTxMs = DigitEntry.of(HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_BLUETOOTH_TX_MS));

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

            {
                long totalTimeMs = HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_REALTIME_BATTERY_MS);
                long screenOffTimeMs = HealthStatsHelper.getMeasure(healthStats, UidHealthStats.MEASUREMENT_REALTIME_SCREEN_OFF_BATTERY_MS);
                long screenOnTimeMs = totalTimeMs - screenOffTimeMs;
                snapshot.screenOnMs = DigitEntry.of(screenOnTimeMs);
            }
        }
        return snapshot;
    }

    public static class HealthStatsSnapshot extends Snapshot<HealthStatsSnapshot> {
        @VisibleForTesting
        public HealthStats healthStats;

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

        // CPU
        public DigitEntry<Long> cpuPowerMams = DigitEntry.of(0L);
        public DigitEntry<Long> cpuUsrTimeMs = DigitEntry.of(0L);
        public DigitEntry<Long> cpuSysTimeMs = DigitEntry.of(0L);

        // SystemService & Sensors
        public DigitEntry<Long> wakelocksPartialMs = DigitEntry.of(0L);
        public DigitEntry<Long> gpsMs = DigitEntry.of(0L);
        public DigitEntry<Long> sensorsPowerMams = DigitEntry.of(0L);
        public DigitEntry<Long> cameraMs = DigitEntry.of(0L);
        public DigitEntry<Long> flashLightMs = DigitEntry.of(0L);

        // Network
        public DigitEntry<Long> mobilePowerMams = DigitEntry.of(0L);
        public DigitEntry<Long> mobileRadioActiveMs = DigitEntry.of(0L);
        public DigitEntry<Long> mobileIdleMs = DigitEntry.of(0L);
        public DigitEntry<Long> mobileRxMs = DigitEntry.of(0L);
        public DigitEntry<Long> mobileTxMs = DigitEntry.of(0L);

        public DigitEntry<Long> wifiPowerMams = DigitEntry.of(0L);
        public DigitEntry<Long> wifiIdleMs = DigitEntry.of(0L);
        public DigitEntry<Long> wifiRxMs = DigitEntry.of(0L);
        public DigitEntry<Long> wifiTxMs = DigitEntry.of(0L);

        public DigitEntry<Long> blueToothPowerMams = DigitEntry.of(0L);
        public DigitEntry<Long> blueToothIdleMs = DigitEntry.of(0L);
        public DigitEntry<Long> blueToothRxMs = DigitEntry.of(0L);
        public DigitEntry<Long> blueToothTxMs = DigitEntry.of(0L);

        // Media & Hardware
        public DigitEntry<Long> audioMs = DigitEntry.of(0L);
        public DigitEntry<Long> videoMs = DigitEntry.of(0L);
        public DigitEntry<Long> screenOnMs = DigitEntry.of(0L);


        @Override
        public Delta<HealthStatsSnapshot> diff(HealthStatsSnapshot bgn) {
            return new Delta<HealthStatsSnapshot>(bgn, this) {
                @Override
                protected HealthStatsSnapshot computeDelta() {
                    HealthStatsSnapshot delta = new HealthStatsSnapshot();
                    delta.cpuPowerMams = Differ.DigitDiffer.globalDiff(bgn.cpuPowerMams, end.cpuPowerMams);
                    delta.cpuUsrTimeMs = Differ.DigitDiffer.globalDiff(bgn.cpuUsrTimeMs, end.cpuUsrTimeMs);
                    delta.cpuSysTimeMs = Differ.DigitDiffer.globalDiff(bgn.cpuSysTimeMs, end.cpuSysTimeMs);

                    delta.wakelocksPartialMs = Differ.DigitDiffer.globalDiff(bgn.wakelocksPartialMs, end.wakelocksPartialMs);
                    delta.gpsMs = Differ.DigitDiffer.globalDiff(bgn.gpsMs, end.gpsMs);
                    delta.sensorsPowerMams = Differ.DigitDiffer.globalDiff(bgn.sensorsPowerMams, end.sensorsPowerMams);
                    delta.cameraMs = Differ.DigitDiffer.globalDiff(bgn.cameraMs, end.cameraMs);
                    delta.flashLightMs = Differ.DigitDiffer.globalDiff(bgn.flashLightMs, end.flashLightMs);

                    delta.mobilePowerMams = Differ.DigitDiffer.globalDiff(bgn.mobilePowerMams, end.mobilePowerMams);
                    delta.mobileRadioActiveMs = Differ.DigitDiffer.globalDiff(bgn.mobileRadioActiveMs, end.mobileRadioActiveMs);
                    delta.mobileIdleMs = Differ.DigitDiffer.globalDiff(bgn.mobileIdleMs, end.mobileIdleMs);
                    delta.mobileRxMs = Differ.DigitDiffer.globalDiff(bgn.mobileRxMs, end.mobileRxMs);
                    delta.mobileTxMs = Differ.DigitDiffer.globalDiff(bgn.mobileTxMs, end.mobileTxMs);

                    delta.wifiPowerMams = Differ.DigitDiffer.globalDiff(bgn.wifiPowerMams, end.wifiPowerMams);
                    delta.wifiIdleMs = Differ.DigitDiffer.globalDiff(bgn.wifiIdleMs, end.wifiIdleMs);
                    delta.wifiRxMs = Differ.DigitDiffer.globalDiff(bgn.wifiRxMs, end.wifiRxMs);
                    delta.wifiTxMs = Differ.DigitDiffer.globalDiff(bgn.wifiTxMs, end.wifiTxMs);

                    delta.blueToothPowerMams = Differ.DigitDiffer.globalDiff(bgn.blueToothPowerMams, end.blueToothPowerMams);
                    delta.blueToothIdleMs = Differ.DigitDiffer.globalDiff(bgn.blueToothIdleMs, end.blueToothIdleMs);
                    delta.blueToothRxMs = Differ.DigitDiffer.globalDiff(bgn.blueToothRxMs, end.blueToothRxMs);
                    delta.blueToothTxMs = Differ.DigitDiffer.globalDiff(bgn.blueToothTxMs, end.blueToothTxMs);

                    delta.audioMs = Differ.DigitDiffer.globalDiff(bgn.audioMs, end.audioMs);
                    delta.videoMs = Differ.DigitDiffer.globalDiff(bgn.videoMs, end.videoMs);
                    delta.screenOnMs = Differ.DigitDiffer.globalDiff(bgn.screenOnMs, end.screenOnMs);
                    return delta;
                }
            };
        }
    }
}
