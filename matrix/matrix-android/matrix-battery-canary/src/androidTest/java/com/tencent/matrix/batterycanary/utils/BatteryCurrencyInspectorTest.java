package com.tencent.matrix.batterycanary.utils;

import android.app.Application;
import android.content.Context;

import com.tencent.matrix.Matrix;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

/**
 * @author Kaede
 * @since 9/9/2022
 */
@RunWith(AndroidJUnit4.class)
public class BatteryCurrencyInspectorTest {
    static final String TAG = "Matrix.test.BatteryCurrencyInspectorTest";

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

    @Test
    @SuppressWarnings("ConstantConditions")
    public void testInspection() {
        boolean charging = BatteryCanaryUtil.isDeviceCharging(mContext);
        if (charging) {
            Assert.assertNull(BatteryCurrencyInspector.isMicroAmpCurr(mContext));
            Assert.assertTrue(BatteryCurrencyInspector.isPositiveInChargeCurr(mContext));
            Assert.assertNull(BatteryCurrencyInspector.isPositiveOutOfChargeCurr(mContext));
        } else {
            Assert.assertTrue(BatteryCurrencyInspector.isMicroAmpCurr(mContext));
            Assert.assertNull(BatteryCurrencyInspector.isPositiveInChargeCurr(mContext));
            Assert.assertFalse(BatteryCurrencyInspector.isPositiveOutOfChargeCurr(mContext));
        }
    }
}
