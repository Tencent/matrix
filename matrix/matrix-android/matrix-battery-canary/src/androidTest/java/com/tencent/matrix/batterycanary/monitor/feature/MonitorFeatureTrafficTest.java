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

package com.tencent.matrix.batterycanary.monitor.feature;

import android.app.Application;
import android.content.Context;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.batterycanary.monitor.feature.TrafficMonitorFeature.RadioStatSnapshot;
import com.tencent.matrix.batterycanary.utils.RadioStatUtil;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;


@RunWith(AndroidJUnit4.class)
public class MonitorFeatureTrafficTest {
    static final String TAG = "Matrix.test.MonitorFeatureTrafficTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
        if (!Matrix.isInstalled()) {
            Matrix.init(new Matrix.Builder(((Application) mContext.getApplicationContext())).build());
        }
    }

    @After
    public void shutDown() {
    }

    private BatteryMonitorCore mockMonitor() {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(JiffiesMonitorFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(false)
                .wakelockTimeout(1000)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(1000)
                .build();
        return new BatteryMonitorCore(config);
    }


    @Test
    public void testGetRadioSnapshot() throws InterruptedException {
        final TrafficMonitorFeature feature = new TrafficMonitorFeature();
        feature.configure(mockMonitor());

        if (RadioStatUtil.getCurrentStat(mContext) != null) {
            RadioStatSnapshot bgn = feature.currentRadioSnapshot(mContext);
            Assert.assertNotNull(bgn);
            Assert.assertFalse(bgn.isDelta);
            Thread.sleep(100);
            RadioStatSnapshot end = feature.currentRadioSnapshot(mContext);
            Assert.assertNotNull(end);
            Assert.assertFalse(end.isDelta);

            Delta<RadioStatSnapshot> diff = end.diff(bgn);
            Assert.assertNotNull(diff);
            Assert.assertTrue(diff.dlt.isDelta);
            Assert.assertTrue(diff.during >= 100L);
            Assert.assertSame(bgn, diff.bgn);
            Assert.assertSame(end, diff.end);
            Assert.assertEquals(end.time, bgn.time + diff.during);
            Assert.assertEquals(end.wifiRxBytes.get().longValue(), bgn.wifiRxBytes.get() + diff.dlt.wifiRxBytes.get());
            Assert.assertEquals(end.wifiTxBytes.get().longValue(), bgn.wifiTxBytes.get() + diff.dlt.wifiTxBytes.get());
            Assert.assertEquals(end.mobileTxBytes.get().longValue(), bgn.mobileTxBytes.get() + diff.dlt.mobileTxBytes.get());
            Assert.assertEquals(end.mobileRxBytes.get().longValue(), bgn.mobileRxBytes.get() + diff.dlt.mobileRxBytes.get());

            end.mobileRxBytes = MonitorFeature.Snapshot.Entry.DigitEntry.of(end.mobileRxBytes.get() + 10086);
            diff = end.diff(bgn);
            Assert.assertEquals(10086, diff.dlt.mobileRxBytes.get().longValue());
        }
    }
}
