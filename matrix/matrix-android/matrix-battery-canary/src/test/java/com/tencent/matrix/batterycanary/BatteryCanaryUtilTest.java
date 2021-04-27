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

package com.tencent.matrix.batterycanary;

import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import static com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil.ONE_MIN;
import static org.junit.Assert.assertEquals;


@SuppressWarnings("ResultOfMethodCallIgnored")
@RunWith(JUnit4.class)
public class BatteryCanaryUtilTest {

    @Test
    public void testComputeMinuteAvg() throws Exception {
        Assert.assertEquals(100, BatteryCanaryUtil.computeAvgByMinute(100, 60 * 1000L), 5);
        Assert.assertEquals(200, BatteryCanaryUtil.computeAvgByMinute(100, 30 * 1000L), 5);
        Assert.assertEquals(600, BatteryCanaryUtil.computeAvgByMinute(100, 10 * 1000L), 5);

        for (int i = 0; i < 100; i++) {
            int minute = i + 1;
            for (int j = 1000; j < 100000; j++) {
                Assert.assertEquals(j / minute, BatteryCanaryUtil.computeAvgByMinute(j, ONE_MIN * minute), 5);
            }
        }
    }
}
