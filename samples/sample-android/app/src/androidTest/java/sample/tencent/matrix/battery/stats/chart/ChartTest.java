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

package sample.tencent.matrix.battery.stats.chart;

import android.content.Context;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import androidx.core.util.Pair;
import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;


@RunWith(AndroidJUnit4.class)
public class ChartTest {

    private Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
    }

    @Test
    public void testDataSetPolish() throws Exception {
        List<Pair<Float, Long>> input = Arrays.asList(
                new Pair<>(70f, 0L),
                new Pair<>(100f, 0L),
                new Pair<>(95f, 0L),
                new Pair<>(88f, 0L),
                new Pair<>(80f, 0L),
                new Pair<>(60f, 0L),
                new Pair<>(20f, 0L)
        );
        List<Pair<Float, Long>> output = SimpleLineChart.polishDataSet(input);
        Assert.assertArrayEquals(input.toArray(), output.toArray());

        input = Arrays.asList(
                new Pair<>(70f, 0L),
                new Pair<>(100f, 0L),
                new Pair<>(95f, 0L),
                new Pair<>(88f, 0L),
                new Pair<>(80f, 0L),
                new Pair<>(60f, 0L),
                new Pair<>(20f, 0L),
                new Pair<>(10f, 0L),
                new Pair<>(20f, 0L),
                new Pair<>(20f, 0L),
                new Pair<>(20f, 0L),
                new Pair<>(0f, 0L),
                new Pair<>(20f, 0L),
                new Pair<>(20f, 0L),
                new Pair<>(20f, 0L)
        );
        output = SimpleLineChart.polishDataSet(input);
        Assert.assertEquals(SimpleLineChart.MAX_COUNT, output.size());
        Assert.assertEquals(new Pair<>(70f, 0L), output.get(0));
        Assert.assertEquals(new Pair<>(20f, 0L), output.get(output.size() - 1));
        Assert.assertTrue(output.contains(new Pair<>(100f, 0L)));
        Assert.assertTrue(output.contains(new Pair<>(0f, 0L)));

        input = Arrays.asList(
                new Pair<>(70f, 0L),
                new Pair<>(100f, 0L),
                new Pair<>(0f, 0L)
        );
        input = new ArrayList<>(input);
        for (int i = 0; i < 1000; i++) {
            input.add(new Pair<>(40f, 0L));
        }
        input.add(new Pair<>(20f, 0L));
        output = SimpleLineChart.polishDataSet(input);
        Assert.assertEquals(SimpleLineChart.MAX_COUNT, output.size());
        Assert.assertEquals(new Pair<>(70f, 0L), output.get(0));
        Assert.assertEquals(new Pair<>(20f, 0L), output.get(output.size() - 1));
        Assert.assertTrue(output.contains(new Pair<>(100f, 0L)));
        Assert.assertTrue(output.contains(new Pair<>(0f, 0L)));
    }
}
