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
import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

@RequiresApi(Build.VERSION_CODES.N)
public interface IActivityFrameListener {

    /**
     * The interval returned indicates how long to call back {@code onFrameMetricsAvailable}.
     * Usually, this value should not less than 17. Because for those display with a 60Hz
     * refresh rate, it takes at least 16.6ms to generate a frame.
     *
     * @return The interval, measured in milliseconds.
     */
    @IntRange(from = 1)
    int getIntervalMs();

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
     * Frame metrics whose dropped frames less than threshold will be skipped.
     * We always assume the refresh rate of display is 60Hz, and the threshold
     * will be converted to corresponding value while the real refresh rate
     * is not 60Hz.
     * <br/>
     * For example, if the threshold is 10 and refresh rate is 90Hz, all frame
     * metrics whose dropped frames less than 15 will be skipped.
     *
     * @return integer value of threshold, zero indicates all frame metrics will be
     * added to calculation.
     */
    @IntRange(from = 0)
    int getThreshold();


    /**
     * This method will be called when average frame metrics available.
     *
     * @param activityName    name of activity.
     * @param avgDurations    average avgDurations, include draw duration, animation duration, etc.
     *                        See {@link com.tencent.matrix.trace.tracer.FrameTracer.FrameDuration}.
     * @param dropLevel       drop level distribution, sum of this array equals value returned by <code>getSize()</code>.
     *                        See {@link com.tencent.matrix.trace.tracer.FrameTracer.DropStatus}.
     * @param dropSum         dropped frame distribution.
     */
    void onFrameMetricsAvailable(@NonNull String activityName, long[] avgDurations, int[] dropLevel, int[] dropSum, float avgDroppedFrame, float avgRefreshRate, float avgFps);
}
