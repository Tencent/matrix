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

package com.tencent.matrix.trace.listeners;

import android.os.Build;

import androidx.annotation.IntRange;
import androidx.annotation.RequiresApi;

@RequiresApi(Build.VERSION_CODES.N)
public interface IActivityFrameListener {

    /**
     * The size returned indicates how many frame metrics will be used to
     * calculate the average frame metrics info.
     *
     * @return The size of frame count.
     */
    @IntRange(from = 1, to = 1_000_000)
    int getSize();

    /**
     * The name returned will be used to match the specified activity.
     * see <code>Activity.getClass().getName()</code>
     *
     * @return Name of the specified activity, null or empty string will match all activity.
     */
    String getName();

    /**
     * Whether skip the first frame.
     *
     * @return <code>true</code> for skip, <code>false</code> for not.
     */
    boolean skipFirstFrame();


    /**
     * This method will be called when average frame metrics available.
     * @param scene last scene
     * @param durations average durations, include draw duration, animation duration, etc.
     * @param dropLevel drop level distribution, sum of this array equals value returned by <code>getSize()</code>
     * @param dropSum dropped frame distribution.
     */
    void onFrameMetricsAvailable(String scene, long[] durations, int[] dropLevel, int[] dropSum, int avgDroppedFrame, float avgRefreshRate);
}
