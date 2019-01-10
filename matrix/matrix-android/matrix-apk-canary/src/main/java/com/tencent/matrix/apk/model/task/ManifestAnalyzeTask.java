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

import com.google.gson.JsonObject;
import com.tencent.matrix.apk.model.task.util.ApkConstants;
import com.tencent.matrix.apk.model.exception.TaskExecuteException;
import com.tencent.matrix.apk.model.exception.TaskInitException;
import com.tencent.matrix.apk.model.job.JobConfig;
import com.tencent.matrix.apk.model.result.TaskJsonResult;
import com.tencent.matrix.apk.model.result.TaskResult;
import com.tencent.matrix.apk.model.result.TaskResultFactory;
import com.tencent.matrix.apk.model.task.util.ManifestParser;
import com.tencent.matrix.javalib.util.FileUtil;
import com.tencent.matrix.javalib.util.Log;
import com.tencent.matrix.javalib.util.Util;

import java.io.File;
import java.util.Map;

import static com.tencent.matrix.apk.model.result.TaskResultFactory.TASK_RESULT_TYPE_JSON;
import static com.tencent.matrix.apk.model.task.TaskFactory.TASK_TYPE_MANIFEST;

/**
 * Created by jinqiuchen on 17/5/27.
 */

public class ManifestAnalyzeTask extends ApkTask {

    private static final String TAG = "Matrix.ManifestAnalyzeTask";

    private File inputFile;
    private File arscFile;

    public ManifestAnalyzeTask(JobConfig config, Map<String, String> params) {
        super(config, params);
        type = TASK_TYPE_MANIFEST;
    }

    @Override
    public void init() throws TaskInitException {
        super.init();
        String inputPath = config.getUnzipPath();
        if (Util.isNullOrNil(inputPath)) {
            throw new TaskInitException(TAG + "---APK-UNZIP-PATH can not be null!");
        }
        Log.i(TAG, "inputPath:%s", inputPath);
        inputFile = new File(inputPath, ApkConstants.MANIFEST_FILE_NAME);
        if (!inputFile.exists()) {
            throw new TaskInitException(TAG + "---Manifest file '" + inputPath + File.separator + ApkConstants.MANIFEST_FILE_NAME + "' is not exist!");
        }

        arscFile = new File(inputPath, ApkConstants.ARSC_FILE_NAME);
    }

    @Override
    public TaskResult call() throws TaskExecuteException {
        try {
            ManifestParser manifestParser = null;
            if (!FileUtil.isLegalFile(arscFile)) {
                manifestParser = new ManifestParser(inputFile);
            } else {
                manifestParser = new ManifestParser(inputFile, arscFile);
            }
            TaskResult taskResult = TaskResultFactory.factory(getType(), TASK_RESULT_TYPE_JSON, config);
            if (taskResult == null) {
                return null;
            }
            long startTime = System.currentTimeMillis();
            JsonObject jsonObject = manifestParser.parse();
            Log.d(TAG, jsonObject.toString());
            ((TaskJsonResult) taskResult).add("manifest", jsonObject);
            taskResult.setStartTime(startTime);
            taskResult.setEndTime(System.currentTimeMillis());
            return taskResult;
        } catch (Exception e) {
            throw new TaskExecuteException(e.getMessage(), e);
        }
    }
}
