package com.tencent.matrix.batterycanary.utils;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.XmlResourceParser;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;

import androidx.annotation.Nullable;
import androidx.annotation.RestrictTo;

/**
 * @see com.android.internal.os.PowerProfile
 */
@RestrictTo(RestrictTo.Scope.LIBRARY)
@SuppressWarnings({"JavadocReference", "ConstantConditions", "TryFinallyCanBeTryWithResources"})
public class PowerProfile {
    private static PowerProfile sInstance = null;

    @Nullable
    public static PowerProfile getInstance() {
        return sInstance;
    }

    public static PowerProfile init(Context context) throws IOException {
        synchronized (sLock) {
            try {
                sInstance = new PowerProfile(context).smoke();
                return sInstance;
            } catch (Throwable e) {
                throw new IOException(e);
            }
        }
    }

    public PowerProfile smoke() throws IOException {
        if (getNumCpuClusters() <= 0) {
            throw new IOException("Invalid cpu clusters: " + getNumCpuClusters());
        }
        for (int i = 0; i < getNumCpuClusters(); i++) {
            if (getNumSpeedStepsInCpuCluster(i) <= 0) {
                throw new IOException("Invalid cpu cluster speed-steps: cluster = " + i
                        +  ", steps = " + getNumSpeedStepsInCpuCluster(i));
            }
        }
        int cpuCoreNum = BatteryCanaryUtil.getCpuCoreNum();
        int cpuCoreNumInProfile = getCpuCoreNum();
        if (cpuCoreNum != cpuCoreNumInProfile) {
            throw new IOException("Unmatched cpu core num, sys = " + cpuCoreNum
                    +  ", profile = " + cpuCoreNumInProfile);
        }
        return this;
    }

    public boolean isSupported() {
        try {
            smoke();
            return true;
        } catch (IOException ignored) {
            return false;
        }
    }

    public int getCpuCoreNum() {
        int cpuCoreNumInProfile = 0;
        for (int i = 0; i < getNumCpuClusters(); i++) {
            cpuCoreNumInProfile += getNumCoresInCpuCluster(i);
        }
        return cpuCoreNumInProfile;
    }

    public int getClusterByCpuNum(int cpuCoreNum) {
        int idx = -1;
        if (cpuCoreNum < 0) {
            return idx; // index out of bound
        }
        int delta = 0;
        for (int i = 0; i < mCpuClusters.length; i++) {
            CpuClusterKey cpuCluster = mCpuClusters[i];
            if (cpuCluster.numCpus + delta >= cpuCoreNum + 1) {
                return i;
            }
            delta += cpuCluster.numCpus;
        }
        return -2;
    }

    public static final String POWER_CPU_SUSPEND = "cpu.suspend";
    public static final String POWER_CPU_IDLE = "cpu.idle";
    public static final String POWER_CPU_ACTIVE = "cpu.active";

    /**
     * Power consumption when WiFi driver is scanning for networks.
     */
    public static final String POWER_WIFI_SCAN = "wifi.scan";

    /**
     * Power consumption when WiFi driver is on.
     */
    public static final String POWER_WIFI_ON = "wifi.on";

    /**
     * Power consumption when WiFi driver is transmitting/receiving.
     */
    public static final String POWER_WIFI_ACTIVE = "wifi.active";

    //
    // Updated power constants. These are not estimated, they are real world
    // currents and voltages for the underlying bluetooth and wifi controllers.
    //
    public static final String POWER_WIFI_CONTROLLER_IDLE = "wifi.controller.idle";
    public static final String POWER_WIFI_CONTROLLER_RX = "wifi.controller.rx";
    public static final String POWER_WIFI_CONTROLLER_TX = "wifi.controller.tx";
    public static final String POWER_WIFI_CONTROLLER_TX_LEVELS = "wifi.controller.tx_levels";
    public static final String POWER_WIFI_CONTROLLER_OPERATING_VOLTAGE = "wifi.controller.voltage";

    public static final String POWER_BLUETOOTH_CONTROLLER_IDLE = "bluetooth.controller.idle";
    public static final String POWER_BLUETOOTH_CONTROLLER_RX = "bluetooth.controller.rx";
    public static final String POWER_BLUETOOTH_CONTROLLER_TX = "bluetooth.controller.tx";
    public static final String POWER_BLUETOOTH_CONTROLLER_OPERATING_VOLTAGE =
            "bluetooth.controller.voltage";

    public static final String POWER_MODEM_CONTROLLER_SLEEP = "modem.controller.sleep";
    public static final String POWER_MODEM_CONTROLLER_IDLE = "modem.controller.idle";
    public static final String POWER_MODEM_CONTROLLER_RX = "modem.controller.rx";
    public static final String POWER_MODEM_CONTROLLER_TX = "modem.controller.tx";
    public static final String POWER_MODEM_CONTROLLER_OPERATING_VOLTAGE =
            "modem.controller.voltage";

    /**
     * Power consumption when GPS is on.
     */
    public static final String POWER_GPS_ON = "gps.on";

    /**
     * GPS power parameters based on signal quality
     */
    public static final String POWER_GPS_SIGNAL_QUALITY_BASED = "gps.signalqualitybased";
    public static final String POWER_GPS_OPERATING_VOLTAGE = "gps.voltage";

    /**
     * Power consumption when Bluetooth driver is on.
     *
     * @deprecated
     */
    @Deprecated
    public static final String POWER_BLUETOOTH_ON = "bluetooth.on";

    /**
     * Power consumption when Bluetooth driver is transmitting/receiving.
     *
     * @deprecated
     */
    @Deprecated
    public static final String POWER_BLUETOOTH_ACTIVE = "bluetooth.active";

    /**
     * Power consumption when Bluetooth driver gets an AT command.
     *
     * @deprecated
     */
    @Deprecated
    public static final String POWER_BLUETOOTH_AT_CMD = "bluetooth.at";

    /**
     * Power consumption when screen is in doze/ambient/always-on mode, including backlight power.
     */
    public static final String POWER_AMBIENT_DISPLAY = "ambient.on";

    /**
     * Power consumption when screen is on, not including the backlight power.
     */
    public static final String POWER_SCREEN_ON = "screen.on";

    /**
     * Power consumption when cell radio is on but not on a call.
     */
    public static final String POWER_RADIO_ON = "radio.on";

    /**
     * Power consumption when cell radio is hunting for a signal.
     */
    public static final String POWER_RADIO_SCANNING = "radio.scanning";

    /**
     * Power consumption when talking on the phone.
     */
    public static final String POWER_RADIO_ACTIVE = "radio.active";

    /**
     * Power consumption at full backlight brightness. If the backlight is at
     * 50% brightness, then this should be multiplied by 0.5
     */
    public static final String POWER_SCREEN_FULL = "screen.full";

    /**
     * Power consumed by the audio hardware when playing back audio content. This is in addition
     * to the CPU power, probably due to a DSP and / or amplifier.
     */
    public static final String POWER_AUDIO = "audio";

    /**
     * Power consumed by any media hardware when playing back video content. This is in addition
     * to the CPU power, probably due to a DSP.
     */
    public static final String POWER_VIDEO = "video";

    /**
     * Average power consumption when camera flashlight is on.
     */
    public static final String POWER_FLASHLIGHT = "camera.flashlight";

    /**
     * Power consumption when DDR is being used.
     */
    public static final String POWER_MEMORY = "memory.bandwidths";

    /**
     * Average power consumption when the camera is on over all standard use cases.
     * <p>
     * TODO: Add more fine-grained camera power metrics.
     */
    public static final String POWER_CAMERA = "camera.avg";

    /**
     * Power consumed by wif batched scaning.  Broken down into bins by
     * Channels Scanned per Hour.  May do 1-720 scans per hour of 1-100 channels
     * for a range of 1-72,000.  Going logrithmic (1-8, 9-64, 65-512, 513-4096, 4097-)!
     */
    public static final String POWER_WIFI_BATCHED_SCAN = "wifi.batchedscan";

    /**
     * Battery capacity in milliAmpHour (mAh).
     */
    public static final String POWER_BATTERY_CAPACITY = "battery.capacity";

    /**
     * A map from Power Use Item to its power consumption.
     */
    static final HashMap<String, Double> sPowerItemMap = new HashMap<>();
    /**
     * A map from Power Use Item to an array of its power consumption
     * (for items with variable power e.g. CPU).
     */
    static final HashMap<String, Double[]> sPowerArrayMap = new HashMap<>();

    private static final String TAG_DEVICE = "device";
    private static final String TAG_ITEM = "item";
    private static final String TAG_ARRAY = "array";
    private static final String TAG_ARRAYITEM = "value";
    private static final String ATTR_NAME = "name";

    private static final Object sLock = new Object();


    PowerProfile(Context context) {
        // Read the XML file for the given profile (normally only one per device)
        synchronized (sLock) {
            if (sPowerItemMap.size() == 0 && sPowerArrayMap.size() == 0) {
                readPowerValuesFromXml(context);
            }
            initCpuClusters();
        }
    }

    @SuppressWarnings({"ToArrayCallWithZeroLengthArrayArgument", "UnnecessaryBoxing", "CatchMayIgnoreException", "TryWithIdenticalCatches"})
    private void readPowerValuesFromXml(Context context) {
        final int id = context.getResources().getIdentifier("power_profile", "xml", "android");
        final Resources resources = context.getResources();
        XmlResourceParser parser = resources.getXml(id);
        boolean parsingArray = false;
        ArrayList<Double> array = new ArrayList<>();
        String arrayName = null;

        try {
            XmlUtils.beginDocument(parser, TAG_DEVICE);

            while (true) {
                XmlUtils.nextElement(parser);

                String element = parser.getName();
                if (element == null) break;

                if (parsingArray && !element.equals(TAG_ARRAYITEM)) {
                    // Finish array
                    sPowerArrayMap.put(arrayName, array.toArray(new Double[array.size()]));
                    parsingArray = false;
                }
                if (element.equals(TAG_ARRAY)) {
                    parsingArray = true;
                    array.clear();
                    arrayName = parser.getAttributeValue(null, ATTR_NAME);
                } else if (element.equals(TAG_ITEM) || element.equals(TAG_ARRAYITEM)) {
                    String name = null;
                    if (!parsingArray) name = parser.getAttributeValue(null, ATTR_NAME);
                    if (parser.next() == XmlPullParser.TEXT) {
                        String power = parser.getText();
                        double value = 0;
                        try {
                            value = Double.valueOf(power);
                        } catch (NumberFormatException nfe) {
                        }
                        if (element.equals(TAG_ITEM)) {
                            sPowerItemMap.put(name, value);
                        } else if (parsingArray) {
                            array.add(value);
                        }
                    }
                }
            }
            if (parsingArray) {
                sPowerArrayMap.put(arrayName, array.toArray(new Double[array.size()]));
            }
        } catch (XmlPullParserException e) {
            throw new RuntimeException(e);
        } catch (IOException e) {
            throw new RuntimeException(e);
        } finally {
            parser.close();
        }

        // Now collect other config variables.
        int[] configResIds = new int[]{
                context.getResources().getIdentifier("config_bluetooth_idle_cur_ma", "integer", "android"),
                context.getResources().getIdentifier("config_bluetooth_rx_cur_ma", "integer", "android"),
                context.getResources().getIdentifier("config_bluetooth_tx_cur_ma", "integer", "android"),
                context.getResources().getIdentifier("config_bluetooth_operating_voltage_mv", "integer", "android"),
        };

        String[] configResIdKeys = new String[]{
                POWER_BLUETOOTH_CONTROLLER_IDLE,
                POWER_BLUETOOTH_CONTROLLER_RX,
                POWER_BLUETOOTH_CONTROLLER_TX,
                POWER_BLUETOOTH_CONTROLLER_OPERATING_VOLTAGE,
        };

        for (int i = 0; i < configResIds.length; i++) {
            String key = configResIdKeys[i];
            // if we already have some of these parameters in power_profile.xml, ignore the
            // value in config.xml
            if ((sPowerItemMap.containsKey(key) && sPowerItemMap.get(key) > 0)) {
                continue;
            }
            int value = resources.getInteger(configResIds[i]);
            if (value > 0) {
                sPowerItemMap.put(key, (double) value);
            }
        }
    }

    private CpuClusterKey[] mCpuClusters;

    private static final String CPU_PER_CLUSTER_CORE_COUNT = "cpu.clusters.cores";
    private static final String CPU_CLUSTER_POWER_COUNT = "cpu.cluster_power.cluster";
    private static final String CPU_CORE_SPEED_PREFIX = "cpu.core_speeds.cluster";
    private static final String CPU_CORE_POWER_PREFIX = "cpu.core_power.cluster";

    private void initCpuClusters() {
        if (sPowerArrayMap.containsKey(CPU_PER_CLUSTER_CORE_COUNT)) {
            final Double[] data = sPowerArrayMap.get(CPU_PER_CLUSTER_CORE_COUNT);
            mCpuClusters = new CpuClusterKey[data.length];
            for (int cluster = 0; cluster < data.length; cluster++) {
                int numCpusInCluster = (int) Math.round(data[cluster]);
                mCpuClusters[cluster] = new CpuClusterKey(
                        CPU_CORE_SPEED_PREFIX + cluster, CPU_CLUSTER_POWER_COUNT + cluster,
                        CPU_CORE_POWER_PREFIX + cluster, numCpusInCluster);
            }
        } else {
            // Default to single.
            mCpuClusters = new CpuClusterKey[1];
            int numCpus = 1;
            if (sPowerItemMap.containsKey(CPU_PER_CLUSTER_CORE_COUNT)) {
                numCpus = (int) Math.round(sPowerItemMap.get(CPU_PER_CLUSTER_CORE_COUNT));
            }
            mCpuClusters[0] = new CpuClusterKey(CPU_CORE_SPEED_PREFIX + 0,
                    CPU_CLUSTER_POWER_COUNT + 0, CPU_CORE_POWER_PREFIX + 0, numCpus);
        }
    }

    public static class CpuClusterKey {
        private final String freqKey;
        private final String clusterPowerKey;
        private final String corePowerKey;
        private final int numCpus;

        private CpuClusterKey(String freqKey, String clusterPowerKey,
                              String corePowerKey, int numCpus) {
            this.freqKey = freqKey;
            this.clusterPowerKey = clusterPowerKey;
            this.corePowerKey = corePowerKey;
            this.numCpus = numCpus;
        }
    }

    public int getNumCpuClusters() {
        return mCpuClusters.length;
    }

    public int getNumCoresInCpuCluster(int cluster) {
        return mCpuClusters[cluster].numCpus;
    }

    public int getNumSpeedStepsInCpuCluster(int cluster) {
        if (cluster < 0 || cluster >= mCpuClusters.length) {
            return 0; // index out of bound
        }
        if (sPowerArrayMap.containsKey(mCpuClusters[cluster].freqKey)) {
            return sPowerArrayMap.get(mCpuClusters[cluster].freqKey).length;
        }
        return 1; // Only one speed
    }

    public double getAveragePowerForCpuCluster(int cluster) {
        if (cluster >= 0 && cluster < mCpuClusters.length) {
            return getAveragePower(mCpuClusters[cluster].clusterPowerKey);
        }
        return 0;
    }

    public double getAveragePowerForCpuCore(int cluster, int step) {
        if (cluster >= 0 && cluster < mCpuClusters.length) {
            return getAveragePower(mCpuClusters[cluster].corePowerKey, step);
        }
        return 0;
    }

    public int getNumElements(String key) {
        if (sPowerItemMap.containsKey(key)) {
            return 1;
        } else if (sPowerArrayMap.containsKey(key)) {
            return sPowerArrayMap.get(key).length;
        }
        return 0;
    }


    public double getAveragePowerOrDefault(String type, double defaultValue) {
        if (sPowerItemMap.containsKey(type)) {
            return sPowerItemMap.get(type);
        } else if (sPowerArrayMap.containsKey(type)) {
            return sPowerArrayMap.get(type)[0];
        } else {
            return defaultValue;
        }
    }


    public double getAveragePower(String type) {
        return getAveragePowerOrDefault(type, 0);
    }


    public double getAveragePower(String type, int level) {
        if (sPowerItemMap.containsKey(type)) {
            return sPowerItemMap.get(type);
        } else if (sPowerArrayMap.containsKey(type)) {
            final Double[] values = sPowerArrayMap.get(type);
            if (values.length > level && level >= 0) {
                return values[level];
            } else if (level < 0 || values.length == 0) {
                return 0;
            } else {
                return values[values.length - 1];
            }
        } else {
            return 0;
        }
    }

    public double getBatteryCapacity() {
        return getAveragePower(POWER_BATTERY_CAPACITY);
    }

    @SuppressWarnings("StatementWithEmptyBody")
    static class XmlUtils {
        public static void beginDocument(XmlPullParser parser, String firstElementName) throws XmlPullParserException, IOException {
            int type;
            while ((type = parser.next()) != parser.START_TAG
                    && type != parser.END_DOCUMENT) {
            }

            if (type != parser.START_TAG) {
                throw new XmlPullParserException("No start tag found");
            }

            if (!parser.getName().equals(firstElementName)) {
                throw new XmlPullParserException("Unexpected start tag: found " + parser.getName() +
                        ", expected " + firstElementName);
            }
        }

        public static void nextElement(XmlPullParser parser) throws XmlPullParserException, IOException {
            int type;
            while ((type = parser.next()) != parser.START_TAG
                    && type != parser.END_DOCUMENT) {
            }
        }
    }
}