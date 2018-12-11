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

package com.tencent.matrix.trace.schedule;

import android.os.Handler;
import android.os.HandlerThread;

/**
 * Created by caichongyang on 2017/5/20.
 * <p>
 * This is a timer class.
 * </p>
 */

public class LazyScheduler {

    private final long delay;
    private final Handler mHandler;
    private volatile boolean isSetUp = false;

    public LazyScheduler(HandlerThread handlerThread, long delay) {
        this.delay = delay;
        mHandler = new Handler(handlerThread.getLooper());
    }

    public boolean isSetUp() {
        return isSetUp;
    }

    public void setUp(final ILazyTask task, boolean cycle) {
        if (null != mHandler) {
            this.isSetUp = true;
            mHandler.removeCallbacksAndMessages(null);
            RetryRunnable retryRunnable = new RetryRunnable(mHandler, delay, task, cycle);
            mHandler.postDelayed(retryRunnable, delay);
        }
    }

    public void cancel() {
        if (null != mHandler) {
            this.isSetUp = false;
            mHandler.removeCallbacksAndMessages(null);
        }
    }

    public void setOff() {
        cancel();
    }

    public interface ILazyTask {
        void onTimeExpire();
    }

    static class RetryRunnable implements Runnable {
        private final Handler handler;
        private final long delay;
        private final ILazyTask lazyTask;
        private final boolean cycle;

        RetryRunnable(Handler handler, long delay, ILazyTask lazyTask, boolean cycle) {
            this.handler = handler;
            this.delay = delay;
            this.lazyTask = lazyTask;
            this.cycle = cycle;
        }

        @Override
        public void run() {
            lazyTask.onTimeExpire();
            if (cycle) {
                handler.postDelayed(this, delay);
            }
        }
    }

}
