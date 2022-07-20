/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.tencent.matrix.batterycanary.stats;

import android.app.Application;
import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorManager;
import android.os.health.HealthStats;
import android.os.health.SystemHealthManager;
import android.os.health.TimerStat;
import android.os.health.UidHealthStats;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryCanary;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.CpuStatFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.batterycanary.stats.HealthStatsFeature.HealthStatsSnapshot;
import com.tencent.matrix.batterycanary.utils.PowerProfile;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;
import java.io.PipedWriter;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import static com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil.JIFFY_MILLIS;
import static com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil.ONE_HOR;


@RunWith(AndroidJUnit4.class)
public class HealthStatsTest {
    static final String TAG = "Matrix.test.HealthStatsTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        if (!Matrix.isInstalled()) {
            Matrix.init(new Matrix.Builder(((Application) mContext.getApplicationContext())).build());
        }
    }

    @After
    public void shutDown() {
    }

    private BatteryMonitorCore mockMonitor() {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(CpuStatFeature.class)
                .enable(HealthStatsFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(false)
                .wakelockTimeout(1000)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(1000)
                .build();
        return new BatteryMonitorCore(config);
    }


    @Test
    public void testGetSenorsHandle() throws NoSuchFieldException, IllegalAccessException, NoSuchMethodException, InvocationTargetException {
        SensorManager sm = (SensorManager) mContext.getSystemService(Context.SENSOR_SERVICE);
        Assert.assertNotNull(sm);
        List<Sensor> sensorList = sm.getSensorList(Sensor.TYPE_ALL);
        Assert.assertFalse(sensorList.isEmpty());

        for (Sensor item : sensorList) {
            String name = item.getName();
            int id = item.getId();
            int type = item.getType();
            float power = item.getPower();
            Method method = item.getClass().getDeclaredMethod("getHandle");
            int handle = (int) method.invoke(item);
            Assert.assertTrue(handle > 0);
        }
    }

    @Test
    public void testLiterateStats() {
        SystemHealthManager manager = (SystemHealthManager) mContext.getSystemService(Context.SYSTEM_HEALTH_SERVICE);
        HealthStats healthStats = manager.takeMyUidSnapshot();
        Assert.assertNotNull(healthStats);
        Assert.assertEquals("UidHealthStats", healthStats.getDataType());

        List<HealthStats> statsList = new ArrayList<>();
        statsList.add(healthStats);

        int statsKeyCount = healthStats.getStatsKeyCount();
        Assert.assertTrue(statsKeyCount > 0);

        for (int i = 0; i < statsKeyCount; i++) {
            int key = healthStats.getStatsKeyAt(i);
            if (healthStats.hasStats(key)) {
                Map<String, HealthStats> stats = healthStats.getStats(key);
                for (HealthStats item : stats.values()) {
                    Assert.assertTrue(Arrays.asList(
                            "PidHealthStats",
                            "ProcessHealthStats",
                            "PackageHealthStats",
                            "ServiceHealthStats"
                    ).contains(item.getDataType()));

                    statsList.add(item);

                    int subKeyCount = item.getStatsKeyCount();
                    if (subKeyCount > 0) {
                        for (int j = 0; j < subKeyCount; j++) {
                            int subKey = item.getStatsKeyAt(j);
                            if (item.hasStats(subKey)) {
                                statsList.addAll(item.getStats(subKey).values());
                            }
                        }
                    }
                }
            }
        }

        for (HealthStats item : statsList) {
            int count = item.getMeasurementKeyCount();
            for (int i = 0; i < count; i++) {
                int key = item.getMeasurementKeyAt(i);
                long value = item.getMeasurement(key);
                Assert.assertTrue(item.getDataType() + ": " + key + "=" + value, value >= 0);
            }
        }
        for (HealthStats item : statsList) {
            int count = item.getMeasurementsKeyCount();
            for (int i = 0; i < count; i++) {
                int key = item.getMeasurementsKeyAt(i);
                Map<String, Long> values = item.getMeasurements(key);
                for (Map.Entry<String, Long> entry : values.entrySet()) {
                    Assert.assertTrue(item.getDataType() + ": " + entry.getKey() + "=" + entry.getValue(), entry.getValue() >= 0);
                }
            }
        }

        for (HealthStats item : statsList) {
            int count = item.getTimerKeyCount();
            for (int i = 0; i < count; i++) {
                int key = item.getTimerKeyAt(i);
                TimerStat timerStat = item.getTimer(key);
                Assert.assertTrue(item.getDataType() + ": " + key + "=" + timerStat.getCount() + "," + timerStat.getTime(), timerStat.getCount() >= 0);
                Assert.assertTrue(item.getDataType() + ": " + key + "=" + timerStat.getCount() + "," + timerStat.getTime(), timerStat.getTime() >= 0);
            }
        }
        for (HealthStats item : statsList) {
            int count = item.getTimersKeyCount();
            for (int i = 0; i < count; i++) {
                int key = item.getTimersKeyAt(i);
                Map<String, TimerStat> timerStatMap = item.getTimers(key);
                for (Map.Entry<String, TimerStat> entry : timerStatMap.entrySet()) {
                    Assert.assertTrue(item.getDataType() + ": " + entry.getKey() + "=" + entry.getValue().getCount() + "," + entry.getValue().getTime(), entry.getValue().getCount() >= 0);
                    Assert.assertTrue(item.getDataType() + ": " + entry.getKey() + "=" + entry.getValue().getCount() + "," + entry.getValue().getTime(), entry.getValue().getTime() >= 0);
                }
            }
        }
    }

    @Test
    public void testEstimateCpuPower() throws IOException {
        PowerProfile powerProfile = PowerProfile.init(mContext);
        Assert.assertNotNull(powerProfile);
        Assert.assertTrue(powerProfile.isSupported());

        double cpuActivePower = powerProfile.getAveragePower("cpu.active");
        Assert.assertTrue(cpuActivePower > 0);
        UsageBasedPowerEstimator etmCpuActivePower = new UsageBasedPowerEstimator(cpuActivePower);
    }

    @Test
    public void testEstimateCpuPowerByHealthStats() throws IOException {
        SystemHealthManager manager = (SystemHealthManager) mContext.getSystemService(Context.SYSTEM_HEALTH_SERVICE);
        HealthStats healthStats = manager.takeMyUidSnapshot();
        Assert.assertNotNull(healthStats);

        if (healthStats.hasStats(UidHealthStats.MEASUREMENT_CPU_POWER_MAMS)) {
            double powerMah = healthStats.getMeasurement(UidHealthStats.MEASUREMENT_CPU_POWER_MAMS) / (1000.0 * 60 * 60);
            Assert.assertTrue(powerMah >= 0);
        }
    }

    @Test
    public void testEstimateCpuPowerByCpuStats() throws IOException {
        CpuStatFeature feature = new CpuStatFeature();
        feature.configure(mockMonitor());
        feature.onTurnOn();

        Assert.assertTrue(feature.isSupported());
        CpuStatFeature.CpuStateSnapshot cpuStateSnapshot = feature.currentCpuStateSnapshot();
        Assert.assertTrue(cpuStateSnapshot.procCpuCoreStates.size() > 0);

        SystemHealthManager manager = (SystemHealthManager) mContext.getSystemService(Context.SYSTEM_HEALTH_SERVICE);
        HealthStats healthStats = manager.takeMyUidSnapshot();
        Assert.assertNotNull(healthStats);

        healthStats.hasMeasurement(UidHealthStats.MEASUREMENT_USER_CPU_TIME_MS);
        healthStats.hasMeasurement(UidHealthStats.MEASUREMENT_SYSTEM_CPU_TIME_MS);
        long cpuTimeMs = healthStats.getMeasurement(UidHealthStats.MEASUREMENT_USER_CPU_TIME_MS) +  healthStats.getMeasurement(UidHealthStats.MEASUREMENT_SYSTEM_CPU_TIME_MS);
        double powerMah = estimateCpuPowerByCpuStats(cpuTimeMs);
        Assert.assertTrue(powerMah >= 0);

        double calcCpuPower = HealthStatsHelper.calcCpuPower(feature.getPowerProfile(), healthStats);
        Assert.assertEquals(powerMah, calcCpuPower, 1d);
    }

    private static double estimateCpuPowerByCpuStats(long cpuTimeMs) {
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
                        double powerMa = feat.getPowerProfile().getAveragePowerForCpuCore(i, j);
                        powerMah += new UsageBasedPowerEstimator(powerMa).calculatePower(figuredCpuTimeMs);
                    }
                }
                return powerMah;
            }
        }
        return 0;
    }

    @Test
    public void testEstimateMemoryPower() throws IOException {
    }

    @Test
    public void testEstimateWakelockPower() throws IOException {
        PowerProfile powerProfile = PowerProfile.init(mContext);
        Assert.assertNotNull(powerProfile);
        Assert.assertTrue(powerProfile.isSupported());

        Assert.assertTrue(powerProfile.getAveragePower("cpu.idle") > 0);

        SystemHealthManager manager = (SystemHealthManager) mContext.getSystemService(Context.SYSTEM_HEALTH_SERVICE);
        HealthStats healthStats = manager.takeMyUidSnapshot();
        Assert.assertNotNull(healthStats);

        double powerMah = 0;
        if (healthStats.hasTimers(UidHealthStats.TIMERS_WAKELOCKS_PARTIAL)) {
            Map<String, TimerStat> timers = healthStats.getTimers(UidHealthStats.TIMERS_WAKELOCKS_PARTIAL);
            long timeMs = 0;
            for (TimerStat item : timers.values()) {
                timeMs += item.getTime();
            }
            double powerMa = powerProfile.getAveragePower("cpu.idle");
            powerMah += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }
        Assert.assertTrue(powerMah >= 0);

        double calcPower = HealthStatsHelper.calcWakelocksPower(powerProfile, healthStats);
        Assert.assertEquals(powerMah, calcPower, 1d);
    }

    @Test
    public void testEstimateMobileRadioPower() throws IOException {
        PowerProfile powerProfile = PowerProfile.init(mContext);
        Assert.assertNotNull(powerProfile);
        Assert.assertTrue(powerProfile.isSupported());

        SystemHealthManager manager = (SystemHealthManager) mContext.getSystemService(Context.SYSTEM_HEALTH_SERVICE);
        HealthStats healthStats = manager.takeMyUidSnapshot();
        Assert.assertNotNull(healthStats);

        if (healthStats.hasMeasurement(UidHealthStats.MEASUREMENT_MOBILE_POWER_MAMS)) {
            double powerMahByHealthStats = healthStats.getMeasurement(UidHealthStats.MEASUREMENT_MOBILE_POWER_MAMS) / (1000.0 * 60 * 60);
            Assert.assertTrue(powerMahByHealthStats >= 0);
        }

        double powerMahByRadio = 0;
        if (powerProfile.getAveragePower("radio.active") > 0) {
            if (healthStats.hasTimer(UidHealthStats.TIMER_MOBILE_RADIO_ACTIVE)) {
                long timeMs = healthStats.getTimerTime(UidHealthStats.TIMER_MOBILE_RADIO_ACTIVE);
                double powerMa = powerProfile.getAveragePower("radio.active");
                powerMahByRadio += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
            }
        }
        Assert.assertTrue(powerMahByRadio >= 0);

        double calcPower = HealthStatsHelper.calcMobilePowerByRadioActive(powerProfile, healthStats);
        Assert.assertEquals(powerMahByRadio, calcPower, 1d);

        double powerMahByTime = 0;
        if (healthStats.hasMeasurement(UidHealthStats.MEASUREMENT_MOBILE_IDLE_MS)) {
            long timeMs = healthStats.getMeasurement(UidHealthStats.MEASUREMENT_MOBILE_IDLE_MS);
            double powerMa = powerProfile.getAveragePower("modem.controller.idle");
            powerMahByTime += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }
        if (healthStats.hasMeasurement(UidHealthStats.MEASUREMENT_MOBILE_RX_MS)) {
            long timeMs = healthStats.getMeasurement(UidHealthStats.MEASUREMENT_MOBILE_RX_MS);
            double powerMa = powerProfile.getAveragePower("modem.controller.rx");
            powerMahByTime += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }
        if (healthStats.hasMeasurement(UidHealthStats.MEASUREMENT_MOBILE_TX_MS)) {
            long timeMs = healthStats.getMeasurement(UidHealthStats.MEASUREMENT_MOBILE_TX_MS);
            double powerMa = powerProfile.getAveragePower("modem.controller.tx");
            powerMahByTime += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }
        Assert.assertTrue(powerMahByTime >= 0);

        calcPower = HealthStatsHelper.calcMobilePower(powerProfile, healthStats);
        Assert.assertEquals(powerMahByTime, calcPower, 1d);
    }

    @Test
    public void testEstimateWifiPower() throws IOException {
        PowerProfile powerProfile = PowerProfile.init(mContext);
        Assert.assertNotNull(powerProfile);
        Assert.assertTrue(powerProfile.isSupported());


        double wifiIdlePower = powerProfile.getAveragePower("wifi.controller.idle");
        Assert.assertTrue(wifiIdlePower > 0);
        UsageBasedPowerEstimator etmWifiIdlePower = new UsageBasedPowerEstimator(wifiIdlePower);
        double wifiRxPower = powerProfile.getAveragePower("wifi.controller.rx");
        Assert.assertTrue(wifiRxPower > 0);
        UsageBasedPowerEstimator etmWifiRxPower = new UsageBasedPowerEstimator(wifiIdlePower);
        double wifiTxPower = powerProfile.getAveragePower("wifi.controller.tx");
        Assert.assertTrue(wifiTxPower > 0);
        UsageBasedPowerEstimator etmWifiTxPower = new UsageBasedPowerEstimator(wifiIdlePower);

        SystemHealthManager manager = (SystemHealthManager) mContext.getSystemService(Context.SYSTEM_HEALTH_SERVICE);
        HealthStats healthStats = manager.takeMyUidSnapshot();
        Assert.assertNotNull(healthStats);

        if (healthStats.hasMeasurement(UidHealthStats.MEASUREMENT_WIFI_POWER_MAMS)) {
            double powerMahByHealthStats = healthStats.getMeasurement(UidHealthStats.MEASUREMENT_WIFI_POWER_MAMS) / (1000.0 * 60 * 60);
            Assert.assertTrue(powerMahByHealthStats >= 0);
        }

        double powerMah = 0;
        if (healthStats.hasMeasurement(UidHealthStats.MEASUREMENT_WIFI_IDLE_MS)) {
            long idleMs = healthStats.getMeasurement(UidHealthStats.MEASUREMENT_WIFI_IDLE_MS);
            powerMah += etmWifiIdlePower.calculatePower(idleMs);
        }
        if (healthStats.hasMeasurement(UidHealthStats.MEASUREMENT_WIFI_RX_MS)) {
            long rxMs = healthStats.getMeasurement(UidHealthStats.MEASUREMENT_WIFI_RX_MS);
            powerMah += etmWifiRxPower.calculatePower(rxMs);
        }
        if (healthStats.hasMeasurement(UidHealthStats.MEASUREMENT_WIFI_TX_MS)) {
            long txMs = healthStats.getMeasurement(UidHealthStats.MEASUREMENT_WIFI_TX_MS);
            powerMah += etmWifiTxPower.calculatePower(txMs);
        }
        Assert.assertTrue(powerMah >= 0);

        double calcPower = HealthStatsHelper.calcWifiPower(powerProfile, healthStats);
        Assert.assertEquals(powerMah, calcPower, 1d);
    }

    @Test
    public void testEstimateBlueToothPower() throws IOException {
        PowerProfile powerProfile = PowerProfile.init(mContext);
        Assert.assertNotNull(powerProfile);
        Assert.assertTrue(powerProfile.isSupported());

        Assert.assertTrue(powerProfile.getAveragePower("bluetooth.controller.idle") > 0);
        Assert.assertTrue(powerProfile.getAveragePower("bluetooth.controller.rx") > 0);
        Assert.assertTrue(powerProfile.getAveragePower("bluetooth.controller.tx") > 0);

        SystemHealthManager manager = (SystemHealthManager) mContext.getSystemService(Context.SYSTEM_HEALTH_SERVICE);
        HealthStats healthStats = manager.takeMyUidSnapshot();
        Assert.assertNotNull(healthStats);

        if (healthStats.hasMeasurement(UidHealthStats.MEASUREMENT_BLUETOOTH_POWER_MAMS)) {
            double powerMahByHealthStats = healthStats.getMeasurement(UidHealthStats.MEASUREMENT_BLUETOOTH_POWER_MAMS) / (1000.0 * 60 * 60);
            Assert.assertTrue(powerMahByHealthStats >= 0);
        }

        double powerMah = 0;
        if (healthStats.hasMeasurement(UidHealthStats.MEASUREMENT_BLUETOOTH_IDLE_MS)) {
            long timeMs = healthStats.getMeasurement(UidHealthStats.MEASUREMENT_BLUETOOTH_IDLE_MS);
            double powerMa = powerProfile.getAveragePower("bluetooth.controller.idle");
            powerMah += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }
        if (healthStats.hasMeasurement(UidHealthStats.MEASUREMENT_BLUETOOTH_RX_MS)) {
            long timeMs = healthStats.getMeasurement(UidHealthStats.MEASUREMENT_BLUETOOTH_RX_MS);
            double powerMa = powerProfile.getAveragePower("bluetooth.controller.rx");
            powerMah += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }
        if (healthStats.hasMeasurement(UidHealthStats.MEASUREMENT_BLUETOOTH_TX_MS)) {
            long timeMs = healthStats.getMeasurement(UidHealthStats.MEASUREMENT_BLUETOOTH_TX_MS);
            double powerMa = powerProfile.getAveragePower("bluetooth.controller.tx");
            powerMah += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }
        Assert.assertTrue(powerMah >= 0);

        double calcPower = HealthStatsHelper.calcBlueToothPower(powerProfile, healthStats);
        Assert.assertEquals(powerMah, calcPower, 1d);
    }

    @Test
    public void testEstimateGpsPower() throws IOException {
        PowerProfile powerProfile = PowerProfile.init(mContext);
        Assert.assertNotNull(powerProfile);
        Assert.assertTrue(powerProfile.isSupported());

        double powerMa = 0;
        powerMa = powerProfile.getAveragePower("gps.on");
        if (powerMa <= 0) {
            int num = powerProfile.getNumElements("gps.signalqualitybased");
            double sumMa = 0;
            for (int i = 0; i < num; i++) {
                sumMa += powerProfile.getAveragePower("gps.signalqualitybased", i);
            }
            powerMa = sumMa / num;
        }

        Assert.assertTrue(powerMa > 0);

        SystemHealthManager manager = (SystemHealthManager) mContext.getSystemService(Context.SYSTEM_HEALTH_SERVICE);
        HealthStats healthStats = manager.takeMyUidSnapshot();
        Assert.assertNotNull(healthStats);

        double powerMah = 0;
        if (healthStats.hasTimer(UidHealthStats.TIMER_GPS_SENSOR)) {
            long timeMs = healthStats.getTimerTime(UidHealthStats.TIMER_GPS_SENSOR);
            powerMah += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }
        Assert.assertTrue(powerMah >= 0);

        double calcPower = HealthStatsHelper.calcGpsPower(powerProfile, healthStats);
        Assert.assertEquals(powerMah, calcPower, 1d);
    }

    @Test
    public void testEstimateSensorsPower() throws InvocationTargetException, IllegalAccessException, NoSuchMethodException {
        SystemHealthManager manager = (SystemHealthManager) mContext.getSystemService(Context.SYSTEM_HEALTH_SERVICE);
        HealthStats healthStats = manager.takeMyUidSnapshot();
        Assert.assertNotNull(healthStats);

        double powerMah = 0;
        if (healthStats.hasTimers(UidHealthStats.TIMERS_SENSORS)) {
            SensorManager sm = (SensorManager) mContext.getSystemService(Context.SENSOR_SERVICE);
            Assert.assertNotNull(sm);
            List<Sensor> sensorList = sm.getSensorList(Sensor.TYPE_ALL);
            Assert.assertFalse(sensorList.isEmpty());

            Map<String, Sensor> sensorMap = new HashMap<>();
            for (Sensor item : sensorList) {
                Method method = item.getClass().getDeclaredMethod("getHandle");
                int handle = (int) method.invoke(item);
                sensorMap.put(String.valueOf(handle), item);
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
                    powerMah += new UsageBasedPowerEstimator(sensor.getPower()).calculatePower(timeMs);
                }
            }
        }
        Assert.assertTrue(powerMah >= 0);
    }

    @Test
    public void testEstimateMediaAndHwPower() throws IOException {
        PowerProfile powerProfile = PowerProfile.init(mContext);
        Assert.assertNotNull(powerProfile);
        Assert.assertTrue(powerProfile.isSupported());


        SystemHealthManager manager = (SystemHealthManager) mContext.getSystemService(Context.SYSTEM_HEALTH_SERVICE);
        HealthStats healthStats = manager.takeMyUidSnapshot();
        Assert.assertNotNull(healthStats);

        double powerMah = 0;

        // Camera
        Assert.assertTrue(powerProfile.getAveragePower("camera.avg") > 0);
        if (healthStats.hasTimer(UidHealthStats.TIMER_CAMERA)) {
            long timeMs = healthStats.getTimerTime(UidHealthStats.TIMER_CAMERA);
            double powerMa = powerProfile.getAveragePower("camera.avg");
            powerMah += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }

        double calcPower = HealthStatsHelper.calcCameraPower(powerProfile, healthStats);
        Assert.assertEquals(powerMah, calcPower, 1d);

        // Flash Light
        Assert.assertTrue(powerProfile.getAveragePower("camera.flashlight") > 0);
        if (healthStats.hasTimer(UidHealthStats.TIMER_FLASHLIGHT)) {
            long timeMs = healthStats.getTimerTime(UidHealthStats.TIMER_FLASHLIGHT);
            double powerMa = powerProfile.getAveragePower("camera.flashlight");
            powerMah += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }

        calcPower = HealthStatsHelper.calcFlashLightPower(powerProfile, healthStats);
        Assert.assertEquals(powerMah, calcPower, 1d);

        // Media
        Assert.assertTrue(powerProfile.getAveragePower("audio") > 0);
        Assert.assertTrue(powerProfile.getAveragePower("video") > 0);
        if (healthStats.hasTimer(UidHealthStats.TIMER_AUDIO)) {
            long timeMs = healthStats.getTimerTime(UidHealthStats.TIMER_AUDIO);
            double powerMa = powerProfile.getAveragePower("audio");
            powerMah += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }

        calcPower = HealthStatsHelper.calcAudioPower(powerProfile, healthStats);
        Assert.assertEquals(powerMah, calcPower, 1d);

        if (healthStats.hasTimer(UidHealthStats.TIMER_VIDEO)) {
            long timeMs = healthStats.getTimerTime(UidHealthStats.TIMER_VIDEO);
            double powerMa = powerProfile.getAveragePower("video");
            powerMah += new UsageBasedPowerEstimator(powerMa).calculatePower(timeMs);
        }
        Assert.assertTrue(powerMah >= 0);

        calcPower = HealthStatsHelper.calcVideoPower(powerProfile, healthStats);
        Assert.assertEquals(powerMah, calcPower, 1d);
    }

    @Test
    public void testEstimateScreenPower() throws IOException {
        PowerProfile powerProfile = PowerProfile.init(mContext);
        Assert.assertNotNull(powerProfile);
        Assert.assertTrue(powerProfile.isSupported());

        Assert.assertTrue(powerProfile.getAveragePower("screen.on") > 0);
        Assert.assertTrue(powerProfile.getAveragePower("screen.full") > 0);

        SystemHealthManager manager = (SystemHealthManager) mContext.getSystemService(Context.SYSTEM_HEALTH_SERVICE);
        HealthStats healthStats = manager.takeMyUidSnapshot();
        Assert.assertNotNull(healthStats);

        double calcPower = HealthStatsHelper.calcScreenPower(powerProfile, healthStats);
        Assert.assertTrue(calcPower >= 0);
    }

    @Test
    public void testEstimateSystemServicePower() throws IOException {
    }

    @Test
    public void testEstimateIdlePower() throws IOException {
    }

    @Test
    public void testGetCurrSnapshot() {
        HealthStatsFeature feature = new HealthStatsFeature();
        feature.configure(mockMonitor());
        feature.onTurnOn();

        Assert.assertNotNull(feature.currHealthStats());

        HealthStatsSnapshot bgn = feature.currHealthStatsSnapshot();
        Assert.assertNotNull(bgn);
        Assert.assertNotNull(bgn.healthStats);

        HealthStatsSnapshot end = feature.currHealthStatsSnapshot();
        Assert.assertNotNull(end);
        Assert.assertNotNull(end.healthStats);

        Delta<HealthStatsSnapshot> delta = end.diff(bgn);
        Assert.assertNotNull(delta);
        Assert.assertNull(delta.dlt.healthStats);
    }


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

}