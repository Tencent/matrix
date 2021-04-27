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

package com.tencent.matrix.batterycanary.utils;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener2;
import android.hardware.SensorManager;
import android.os.BatteryManager;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import android.util.Log;

import com.tencent.matrix.batterycanary.TestUtils;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.RandomAccessFile;
import java.text.DecimalFormat;
import java.text.DecimalFormatSymbols;
import java.util.Locale;

import static android.content.Context.SENSOR_SERVICE;
import static org.junit.Assert.assertEquals;

/**
 * Instrumented test, which will execute on an Android device.
 *
 * @see <a href="http://d.android.com/tools/testing">Testing documentation</a>
 */
@RunWith(AndroidJUnit4.class)
public class TemperatureUtilsTest {
    static final String TAG = "Matrix.test.TemperatureUtilsTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    @After
    public void shutDown() {
    }


    @Test
    public void testGetDeviceTemperature() throws InterruptedException {
        if (TestUtils.isAssembleTest()) return;

        // HardwarePropertiesManager hardwarePropertiesManager= (HardwarePropertiesManager) mContext.getSystemService(Context.HARDWARE_PROPERTIES_SERVICE);
        // float[] temp = hardwarePropertiesManager.getDeviceTemperatures(HardwarePropertiesManager.DEVICE_TEMPERATURE_CPU, HardwarePropertiesManager.TEMPERATURE_CURRENT);

        SensorManager manager = (SensorManager) mContext.getSystemService(SENSOR_SERVICE);
        Sensor sensor = manager.getDefaultSensor(Sensor.TYPE_AMBIENT_TEMPERATURE);
        manager.registerListener(new SensorEventListener2() {
            @Override
            public void onFlushCompleted(Sensor sensor) {
                Assert.assertNotNull(sensor);

            }
            @Override
            public void onSensorChanged(SensorEvent event) {
                Assert.assertNotNull(event);
            }
            @Override
            public void onAccuracyChanged(Sensor sensor, int accuracy) {
                Assert.assertNotNull(sensor);

            }
        }, sensor, SensorManager.SENSOR_DELAY_FASTEST);

        while (true) {
            Log.v(TAG, "foo");
            Thread.sleep(100L);
        }
    }

    @Test
    public void testGetDeviceTemperatureR2() throws InterruptedException {
        if (TestUtils.isAssembleTest()) return;
        thermal();
    }

    private static void thermal() {
        String temp, type;
        for (int i = 0; i < 29; i++) {
            temp = thermalTemp(i);
            if (!temp.contains("0.0")) {
                type = thermalType(i);
                if (type != null) {
                    System.out.println("ThermalValues " + type + " : " + temp + "\n");
                }
            }
        }
    }

    private static String thermalTemp(int i) {
        Process process;
        BufferedReader reader;
        String line;
        String t = null;
        float temp = 0;
        try {
            process = Runtime.getRuntime().exec("cat sys/class/thermal/thermal_zone" + i + "/temp");
            process.waitFor();
            reader = new BufferedReader(new InputStreamReader(process.getInputStream()));
            line = reader.readLine();
            if (line != null) {
                temp = Float.parseFloat(line);
            }
            reader.close();
            process.destroy();
            if (!((int) temp == 0)) {
                if ((int) temp > 10000) {
                    temp = temp / 1000;
                } else if ((int) temp > 1000) {
                    temp = temp / 100;
                } else if ((int) temp > 100) {
                    temp = temp / 10;
                }
                t = String.valueOf(temp);
            } else
                t = "0.0";
        } catch (Exception e) {
            e.printStackTrace();
        }
        return t;
    }

    private static String thermalType(int i) {
        Process process;
        BufferedReader reader;
        String line, type = null;
        try {
            process = Runtime.getRuntime().exec("cat sys/class/thermal/thermal_zone" + i + "/type");
            process.waitFor();
            reader = new BufferedReader(new InputStreamReader(process.getInputStream()));
            line = reader.readLine();
            if (line != null) {
                type = line;
            }
            reader.close();
            process.destroy();
        } catch (Exception e) {
            e.printStackTrace();
        }
        return type;
    }

    @Test
    public void testGetDeviceTemperatureR3() throws InterruptedException {
        if (TestUtils.isAssembleTest()) return;
        new BatteryTempWrapper(mContext).getAmbientTemperature(11,30);
    }


    private static class BatteryTempWrapper {

        final Context mContext;
        // EXAMPLE ONLY - RESET HIGHEST AT 500°C

        public Double closestTemperature = 500.0;
        public Long resetTime = 0L;
        public BatteryTempWrapper(Context context) {
            mContext = context;
        }

        public String getAmbientTemperature(int averageRunningTemp, int period)
        {
            // CHECK MINUTES PASSED ( STOP CONSTANT LOW VALUE )
            Boolean passed30 = ((System.currentTimeMillis() - resetTime) > ((1000 * 60)*period));
            if (passed30)
            {
                resetTime = System.currentTimeMillis();
                closestTemperature = 500.0;
            }

            // FORMAT DECIMALS TO 00.0°C
            DecimalFormatSymbols dfs = new DecimalFormatSymbols(Locale.US);
            dfs.setDecimalSeparator('.');
            DecimalFormat decimalFormat = new DecimalFormat("##.0", dfs);

            // READ CPU & BATTERY
            try
            {
                // BYPASS ANDROID RESTRICTIONS ON THERMAL ZONE FILES & READ CPU THERMAL

                RandomAccessFile restrictedFile = new RandomAccessFile("sys/class/thermal/thermal_zone1/temp", "r");
                Double cpuTemp = (Double.parseDouble(restrictedFile.readLine()) / 1000);

                // RUN BATTERY INTENT
                Intent batIntent = mContext.registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
                // GET DATA FROM INTENT WITH BATTERY
                Double batTemp = (double) batIntent.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, 0) / 10;

                // CHECK IF DATA EXISTS
                if (cpuTemp != null)
                {
                    // CPU FILE OK - CHECK LOWEST TEMP
                    if (cpuTemp - averageRunningTemp < closestTemperature)
                        closestTemperature = cpuTemp - averageRunningTemp;
                }
                else if (batTemp != null)
                {
                    // BAT OK - CHECK LOWEST TEMP
                    if (batTemp - averageRunningTemp < closestTemperature)
                        closestTemperature = batTemp - averageRunningTemp;
                }
                else
                {
                    // NO CPU OR BATTERY TEMP FOUND - RETURN 0°C
                    closestTemperature = 0.0;
                }
            }
            catch (Exception e)
            {
                // NO CPU OR BATTERY TEMP FOUND - RETURN 0°C
                closestTemperature = 0.0;
            }
            // FORMAT & RETURN
            return decimalFormat.format(closestTemperature);
        }

    }


    @Test
    public void testCatDeviceSysFiles() throws Exception {
        String[] filePaths = new String[] {
                "/sys/devices/system/cpu/cpu0/cpufreq/cpu_temp",
                "/sys/devices/system/cpu/cpu0/cpufreq/FakeShmoo_cpu_temp",
                "/sys/class/thermal/thermal_zone1/temp",
                "/sys/class/i2c-adapter/i2c-4/4-004c/temperature",
                "/sys/devices/platform/tegra-i2c.3/i2c-4/4-004c/temperature",
                "/sys/devices/platform/omap/omap_temp_sensor.0/temperature",
                "/sys/devices/platform/tegra_tmon/temp1_input",
                "/sys/kernel/debug/tegra_thermal/temp_tj",
                "/sys/devices/platform/s5p-tmu/temperature",
                "/sys/class/thermal/thermal_zone0/temp",
                "/sys/devices/virtual/thermal/thermal_zone0/temp",
                "/sys/class/hwmon/hwmon0/device/temp1_input",
                "/sys/devices/virtual/thermal/thermal_zone1/temp",
                "/sys/devices/platform/s5p-tmu/curr_temp"
        };

        for (String item : filePaths) {
            try {
                RandomAccessFile restrictedFile = new RandomAccessFile(item, "r");
                String cat = restrictedFile.readLine();
                Assert.assertNotNull(cat);
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    @Test
    public void testGetBatteryTemperature() throws Exception {
        Intent batIntent = mContext.registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        Assert.assertNotNull(batIntent);
        Double batTemp = (double) batIntent.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, 0) / 10;
        Assert.assertNotNull(batTemp);
    }

    @Test
    public void testGetCpuFreq() throws Exception {
        int[] cpuCurrentFreq = BatteryCanaryUtil.getCpuCurrentFreq();
        Assert.assertNotNull(cpuCurrentFreq);
        Assert.assertTrue(cpuCurrentFreq.length > 0);
    }
}
