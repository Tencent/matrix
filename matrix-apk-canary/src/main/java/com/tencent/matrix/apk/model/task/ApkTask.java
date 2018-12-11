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

package com.tencent.matrix.apk.model.task;


import com.tencent.matrix.apk.model.exception.TaskExecuteException;
import com.tencent.matrix.apk.model.exception.TaskInitException;
import com.tencent.matrix.apk.model.job.JobConfig;
import com.tencent.matrix.apk.model.result.TaskResult;

import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Callable;

/**
 * Created by williamjin on 17/5/23.
 */

public abstract class ApkTask implements Callable<TaskResult> {

    private static final String TAG = "Matrix.ApkTask";

    protected int type;
    protected JobConfig config;
    protected Map<String, String> params;
    protected List<ApkTaskProgressListener> progressListeners;

    public interface ApkTaskProgressListener {
        void getProgress(int progress, String message);
    }


    public ApkTask(JobConfig config, Map<String, String> params) {
        this.params = params;
        this.config = config;
        progressListeners = new LinkedList<>();
    }

    public int getType() {
        return type;
    }

    public void init() throws TaskInitException {
        if (config == null) {
            throw new TaskInitException(TAG + "---jobConfig can not be null!");
        }

        if (params == null) {
            throw new TaskInitException(TAG + "---params can not be null!");
        }
    }

    public void addProgressListener(ApkTaskProgressListener listener) {
        if (listener != null) {
            progressListeners.add(listener);
        }
    }

    public void removeProgressListener(ApkTaskProgressListener listener) {
        if (listener != null) {
            progressListeners.remove(listener);
        }
    }

    protected void notifyProgress(int progress, String message) {
        for (ApkTaskProgressListener listener : progressListeners) {
            listener.getProgress(progress, message);
        }
    }

    @Override
    public abstract TaskResult call() throws TaskExecuteException;
}
