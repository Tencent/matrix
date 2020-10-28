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

package com.tencent.matrix.batterycanary.utils;

import android.os.Handler;
import android.os.HandlerThread;
import android.support.annotation.RestrictTo;

import com.tencent.matrix.util.MatrixHandlerThread;

/**
 *  Schedule the detect task(runnable) in a single thread and in FIFO.
 *
 * @author liyongjie
 *         Created by liyongjie on 2017/8/14.
 */
@RestrictTo(RestrictTo.Scope.LIBRARY)
public class BatteryCanaryDetectScheduler {
    private static final String TAG = "Matrix.BatteryCanaryDetectScheduler";

    private Handler mDetectHandler;
    private boolean started = false;

    public BatteryCanaryDetectScheduler() {
    }

    /**
     * Add to the end. Run in the called thread
     *
     * @param detectTask
     */
    public void addDetectTask(Runnable detectTask) {
        mDetectHandler.post(detectTask);
    }

    public void addDetectTask(Runnable detectTask, long delayInMillis) {
        mDetectHandler.postDelayed(detectTask, delayInMillis);
    }

    public void start() {
        if (started) {
            return;
        }
        HandlerThread detectThread = MatrixHandlerThread.getDefaultHandlerThread();
        mDetectHandler = new Handler(detectThread.getLooper());
        started = true;
    }

    public void quit() {
        if (started) {
            mDetectHandler.removeCallbacksAndMessages(null);
            started = false;
        }
    }
}
