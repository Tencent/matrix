package com.tencent.matrix.batterycanary.utils;

import android.content.Context;
import android.os.BatteryManager;

import androidx.annotation.Nullable;

/**
 * Since {@link BatteryManager#BATTERY_PROPERTY_CURRENT_NOW} will return microAmp/millisAmp &
 * positive/negative value in difference devices, here we figure out the unit somehow.
 *
 * @author Kaede
 * @since 9/9/2022
 */
public class BatteryCurrencyInspector {

    /**
     * To get a high currency value, you are supposed to call this API within a high cpu-load
     * activity & without charging. (When the device's cpu-load is very low, we can not tell
     * microAmp or millisAmp.)
     */
    @Nullable
    public static Boolean isMicroAmpCurr(Context context) {
        if (BatteryCanaryUtil.isDeviceCharging(context)) {
            // Currency might be very low in charge
            return null;
        }
        long val = BatteryCanaryUtil.getBatteryCurrencyImmediately(context);
        if (val == -1) {
            return null;
        }
        return isMicroAmp(val);
    }

    public static boolean isMicroAmp(long amp) {
        return amp > 1000L;
    }

    @Nullable
    public static Boolean isPositiveInChargeCurr(Context context) {
        if (!BatteryCanaryUtil.isDeviceCharging(context)) {
            return null;
        }
        long val = BatteryCanaryUtil.getBatteryCurrencyImmediately(context);
        if (val == -1) {
            return null;
        }
        return val > 0;
    }

    @Nullable
    public static Boolean isPositiveOutOfChargeCurr(Context context) {
        if (BatteryCanaryUtil.isDeviceCharging(context)) {
            return null;
        }
        long val = BatteryCanaryUtil.getBatteryCurrencyImmediately(context);
        if (val == -1) {
            return null;
        }
        return val > 0;
    }
}
