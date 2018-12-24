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


import com.android.utils.Pair;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import com.tencent.matrix.apk.model.exception.TaskExecuteException;
import com.tencent.matrix.apk.model.exception.TaskInitException;
import com.tencent.matrix.apk.model.job.JobConfig;
import com.tencent.matrix.apk.model.job.JobConstants;
import com.tencent.matrix.apk.model.result.TaskJsonResult;
import com.tencent.matrix.apk.model.result.TaskResult;
import com.tencent.matrix.apk.model.result.TaskResultFactory;
import com.tencent.matrix.javalib.util.FileUtil;
import com.tencent.matrix.javalib.util.Util;

import java.io.File;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import static com.tencent.matrix.apk.model.task.TaskFactory.TASK_TYPE_UNCOMPRESSED_FILE;

/**
 * Created by jinqiuchen on 17/6/21.
 */

public class UncompressedFileTask extends ApkTask {

    private static final String TAG = "Matrix.UncompressedFileTask";

    private File inputFile;
    private Set<String> filterSuffix;
    private Map<String, Long> uncompressSizeMap;
    private Map<String, Long> compressSizeMap;

    public UncompressedFileTask(JobConfig config, Map<String, String> params) {
        super(config, params);
        type = TASK_TYPE_UNCOMPRESSED_FILE;
    }

    @Override
    public void init() throws TaskInitException {
        super.init();
        String inputPath = config.getApkPath();

        if (Util.isNullOrNil(inputPath)) {
            throw new TaskInitException(TAG + "---APK-FILE-PATH can not be null!");
        }
        inputFile = new File(inputPath);
        if (!FileUtil.isLegalFile(inputFile)) {
            throw new TaskInitException(TAG + "---APK-FILE-PATH '" + inputPath + "' is illegal!");
        }

        filterSuffix = new HashSet<>();

        if (params.containsKey(JobConstants.PARAM_SUFFIX) && !Util.isNullOrNil(params.get(JobConstants.PARAM_SUFFIX))) {
            String[]  suffix = params.get(JobConstants.PARAM_SUFFIX).split(",");
            for (String suffixStr : suffix) {
                filterSuffix.add(suffixStr.trim());
            }
        }

        uncompressSizeMap = new HashMap<>();
        compressSizeMap = new HashMap<>();
    }

    private String getSuffix(String name) {
        int index = name.indexOf('.');
        if (index >= 0 && index < name.length() - 1) {
            return name.substring(index + 1);
        }
        return "";
    }

    @Override
    public TaskResult call() throws TaskExecuteException {
        try {
            TaskResult taskResult = TaskResultFactory.factory(type, TaskResultFactory.TASK_RESULT_TYPE_JSON, config);
            if (taskResult == null) {
                return null;
            }
            long startTime = System.currentTimeMillis();
            JsonArray jsonArray = new JsonArray();
            Map<String, Pair<Long, Long>> entrySizeMap = config.getEntrySizeMap();
            if (!entrySizeMap.isEmpty()) {                                                          //take advantage of the result of UnzipTask.
                for (Map.Entry<String, Pair<Long, Long>> entry : entrySizeMap.entrySet()) {
                    final String suffix = getSuffix(entry.getKey());
                    Pair<Long, Long> size = entry.getValue();
                    if (filterSuffix.isEmpty() || filterSuffix.contains(suffix)) {
                        if (!uncompressSizeMap.containsKey(suffix)) {
                            uncompressSizeMap.put(suffix, size.getFirst());
                        } else {
                            uncompressSizeMap.put(suffix, uncompressSizeMap.get(suffix) + size.getFirst());
                        }
                        if (!compressSizeMap.containsKey(suffix)) {
                            compressSizeMap.put(suffix, size.getSecond());
                        } else {
                            compressSizeMap.put(suffix, compressSizeMap.get(suffix) + size.getSecond());
                        }
                    } else {
//                        Log.d(TAG, "file: %s, filter by suffix.", entry.getKey());
                    }
                }
            }

            for (String suffix : uncompressSizeMap.keySet()) {
                if (uncompressSizeMap.get(suffix).equals(compressSizeMap.get(suffix))) {
                    JsonObject fileItem = new JsonObject();
                    fileItem.addProperty("suffix", suffix);
                    fileItem.addProperty("total-size", uncompressSizeMap.get(suffix));
                    jsonArray.add(fileItem);
                }
            }
            ((TaskJsonResult) taskResult).add("files", jsonArray);
            taskResult.setStartTime(startTime);
            taskResult.setEndTime(System.currentTimeMillis());
            return taskResult;
        } catch (Exception e) {
            throw new TaskExecuteException(e.getMessage(), e);
        }
    }
}
