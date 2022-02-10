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

import androidx.annotation.CallSuper;

import com.tencent.matrix.trace.constants.Constants;

import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.Executor;

/**
 * Created by caichongyang on 2017/5/26.
 **/
public class IDoFrameListener {

    private Executor executor;
    public long time;
    private int intervalFrame = 0;

    private final static LinkedList<FrameReplay> sPool = new LinkedList<>();

    public static final class FrameReplay {
        public String focusedActivity;
        public long startNs;
        public long endNs;
        public int dropFrame;
        public boolean isVsyncFrame;
        public long intendedFrameTimeNs;
        public long inputCostNs;
        public long animationCostNs;
        public long insetsAnimationCostNs;
        public long traversalCostNs;
        public long commitCostNs;

        public void recycle() {
            if (sPool.size() <= 1000) {
                this.focusedActivity = "";
                this.startNs = 0;
                this.endNs = 0;
                this.dropFrame = 0;
                this.isVsyncFrame = false;
                this.intendedFrameTimeNs = 0;
                this.inputCostNs = 0;
                this.animationCostNs = 0;
                this.insetsAnimationCostNs = 0;
                this.traversalCostNs = 0;
                this.commitCostNs = 0;
                synchronized (sPool) {
                    sPool.add(this);
                }
            }
        }

        public static FrameReplay create() {
            FrameReplay replay;
            synchronized (sPool) {
                replay = sPool.poll();
            }
            if (replay == null) {
                return new FrameReplay();
            }
            return replay;
        }
    }

    private final List<FrameReplay> list = new LinkedList<>();

    public IDoFrameListener() {
        intervalFrame = getIntervalFrameReplay();
    }

    public IDoFrameListener(Executor executor) {
        this.executor = executor;
    }

    @CallSuper
    public void collect(String focusedActivity, long startNs, long endNs, int dropFrame, boolean isVsyncFrame, long intendedFrameTimeNs,
                        long inputCostNs, long animationCostNs, long insetsAnimationCostNs, long traversalCostNs, long commitCostNs) {
        FrameReplay replay = FrameReplay.create();
        replay.focusedActivity = focusedActivity;
        replay.startNs = startNs;
        replay.endNs = endNs;
        replay.dropFrame = dropFrame;
        replay.isVsyncFrame = isVsyncFrame;
        replay.intendedFrameTimeNs = intendedFrameTimeNs;
        replay.inputCostNs = inputCostNs;
        replay.animationCostNs = animationCostNs;
        replay.insetsAnimationCostNs = insetsAnimationCostNs;
        replay.traversalCostNs = traversalCostNs;
        replay.commitCostNs = commitCostNs;
        list.add(replay);
        if (list.size() >= intervalFrame && getExecutor() != null) {
            final List<FrameReplay> copy = new LinkedList<>(list);
            list.clear();
            getExecutor().execute(new Runnable() {
                @Override
                public void run() {
                    doReplay(copy);
                    for (FrameReplay record : copy) {
                        record.recycle();
                    }
                }
            });
        }
    }

    @Deprecated
    public void doFrameAsync(String visibleScene, long taskCost, long frameCostMs, int droppedFrames, boolean isVsyncFrame) {

    }

    @Deprecated
    public void doFrameSync(String visibleScene, long taskCost, long frameCostMs, int droppedFrames, boolean isVsyncFrame) {

    }

    @CallSuper
    public void doFrameAsync(String focusedActivity, long startNs, long endNs, int dropFrame, boolean isVsyncFrame,
                             long intendedFrameTimeNs, long inputCostNs, long animationCostNs, long insetsAnimationCostNs, long traversalCostNs, long commitCostNs) {
        long cost = (endNs - intendedFrameTimeNs) / Constants.TIME_MILLIS_TO_NANO;
        doFrameAsync(focusedActivity, cost, cost, dropFrame, isVsyncFrame);
    }

    @CallSuper
    public void doFrameSync(String focusedActivity, long startNs, long endNs, int dropFrame, boolean isVsyncFrame,
                            long intendedFrameTimeNs, long inputCostNs, long animationCostNs, long insetsAnimationCostNs, long traversalCostNs, long commitCostNs) {
        long cost = (endNs - intendedFrameTimeNs) / Constants.TIME_MILLIS_TO_NANO;
        doFrameSync(focusedActivity, cost, cost, dropFrame, isVsyncFrame);
    }

    public void doReplay(List<FrameReplay> list) {

    }


    public Executor getExecutor() {
        return executor;
    }

    public int getIntervalFrameReplay() {
        return 0;
    }


}
