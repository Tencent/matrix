/*
 * Copyright (C) 2015 Square, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.tencent.matrix.resource.watcher;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;

/**
 * Created by tangyinsheng on 2017/6/2.
 *
 * This class is ported from LeakCanary.
 *
 * Some modification was done in order to executeInBackground task with fixed delay and support
 * custom HeapDumpHandler and HandlerThread support.
 * e.g. Some framework needs to wrap default handler class for monitoring.
 */

public class RetryableTaskExecutor {
    private final Handler mBackgroundHandler;
    private final Handler mMainHandler;
    private long mDelayMillis;

    public interface RetryableTask {
        enum Status {
            DONE, RETRY
        }

        Status execute();
    }


    public RetryableTaskExecutor(long delayMillis, HandlerThread handleThread) {
        mBackgroundHandler = new Handler(handleThread.getLooper());
        mMainHandler = new Handler(Looper.getMainLooper());
        mDelayMillis = delayMillis;
    }

    public void setDelayMillis(long delayed) {
        this.mDelayMillis = delayed;
    }

    public void executeInMainThread(final RetryableTask task) {
        postToMainThreadWithDelay(task, 0);
    }

    public void executeInBackground(final RetryableTask task) {
        postToBackgroundWithDelay(task, 0);
    }

    public void clearTasks() {
        mBackgroundHandler.removeCallbacksAndMessages(null);
        mMainHandler.removeCallbacksAndMessages(null);
    }

    public void quit() {
        clearTasks();
    }

    private void postToMainThreadWithDelay(final RetryableTask task, final int failedAttempts) {
        mMainHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                RetryableTask.Status status = task.execute();
                if (status == RetryableTask.Status.RETRY) {
                    postToMainThreadWithDelay(task, failedAttempts + 1);
                }
            }
        }, mDelayMillis);
    }

    private void postToBackgroundWithDelay(final RetryableTask task, final int failedAttempts) {
        mBackgroundHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                RetryableTask.Status status = task.execute();
                if (status == RetryableTask.Status.RETRY) {
                    postToBackgroundWithDelay(task, failedAttempts + 1);
                }
            }
        }, mDelayMillis);
    }
}
