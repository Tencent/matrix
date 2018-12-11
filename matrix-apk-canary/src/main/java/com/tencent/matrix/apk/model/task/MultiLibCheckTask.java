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

import com.google.gson.JsonArray;
import com.tencent.matrix.apk.model.exception.TaskExecuteException;
import com.tencent.matrix.apk.model.exception.TaskInitException;
import com.tencent.matrix.apk.model.job.JobConfig;
import com.tencent.matrix.apk.model.result.TaskJsonResult;
import com.tencent.matrix.apk.model.result.TaskResult;
import com.tencent.matrix.apk.model.result.TaskResultFactory;
import com.tencent.matrix.javalib.util.Log;
import com.tencent.matrix.javalib.util.Util;

import java.io.File;
import java.util.Map;

import static com.tencent.matrix.apk.model.result.TaskResultFactory.TASK_RESULT_TYPE_JSON;
import static com.tencent.matrix.apk.model.task.TaskFactory.TASK_TYPE_CHECK_MULTILIB;

/**
 * Created by williamjin on 17/6/20.
 */

public class MultiLibCheckTask extends ApkTask {

    private static final String TAG = "Matrix.MultiLibCheckTask";

    private File libDir;

    public MultiLibCheckTask(JobConfig jobConfig, Map<String, String> params) {
        super(jobConfig, params);
        type = TASK_TYPE_CHECK_MULTILIB;
    }

    @Override
    public void init() throws TaskInitException {
        super.init();
        String inputPath = config.getUnzipPath();
        if (!Util.isNullOrNil(inputPath)) {
            Log.d(TAG, "inputPath:%s", inputPath);
            libDir = new File(inputPath, "lib");
        } else {
            throw new TaskInitException(TAG + "---APK-UNZIP-PATH can not be null!");
        }
    }

    @Override
    public TaskResult call() throws TaskExecuteException {
        try {
            TaskResult taskResult = TaskResultFactory.factory(getType(), TASK_RESULT_TYPE_JSON, config);
            if (taskResult == null) {
                return null;
            }
            long startTime = System.currentTimeMillis();
            JsonArray jsonArray = new JsonArray();
            if (libDir.exists() && libDir.isDirectory()) {
                File[] dirs = libDir.listFiles();
                for (File dir : dirs) {
                    if (dir.isDirectory()) {
                        jsonArray.add(dir.getName());
                    }
                }
            }
            ((TaskJsonResult) taskResult).add("lib-dirs", jsonArray);
            if (jsonArray.size() > 1) {
                ((TaskJsonResult) taskResult).add("multi-lib", true);
            } else {
                ((TaskJsonResult) taskResult).add("multi-lib", false);
            }
            taskResult.setStartTime(startTime);
            taskResult.setEndTime(System.currentTimeMillis());
            return taskResult;
        } catch (Exception e) {
            throw new TaskExecuteException(e.getMessage(), e);
        }
    }


}
