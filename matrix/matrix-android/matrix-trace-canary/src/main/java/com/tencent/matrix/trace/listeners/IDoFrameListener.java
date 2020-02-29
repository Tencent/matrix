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

import android.support.annotation.CallSuper;

import com.tencent.matrix.trace.constants.Constants;

import java.util.concurrent.Executor;

/**
 * Created by caichongyang on 2017/5/26.
 **/
public class IDoFrameListener {

    private Executor executor;
    public long time;

    public IDoFrameListener() {

    }

    public IDoFrameListener(Executor executor) {
        this.executor = executor;
    }

    @Deprecated
    public void doFrameAsync(String visibleScene, long taskCost, long frameCostMs, int droppedFrames, boolean isVsyncFrame) {

    }

    @Deprecated
    public void doFrameSync(String visibleScene, long taskCost, long frameCostMs, int droppedFrames, boolean isVsyncFrame) {

    }

    @CallSuper
    public void doFrameAsync(String focusedActivity, long startNs, long endNs, int dropFrame, boolean isVsyncFrame,
                             long intendedFrameTimeNs, long inputCostNs, long animationCostNs, long traversalCostNs) {
        long cost = (endNs - intendedFrameTimeNs) / Constants.TIME_MILLIS_TO_NANO;
        doFrameAsync(focusedActivity, cost, cost, dropFrame, isVsyncFrame);
    }

    @CallSuper
    public void doFrameSync(String focusedActivity, long startNs, long endNs, int dropFrame, boolean isVsyncFrame,
                            long intendedFrameTimeNs, long inputCostNs, long animationCostNs, long traversalCostNs) {
        long cost = (endNs - intendedFrameTimeNs) / Constants.TIME_MILLIS_TO_NANO;
        doFrameSync(focusedActivity, cost, cost, dropFrame, isVsyncFrame);
    }


    public Executor getExecutor() {
        return executor;
    }


}
