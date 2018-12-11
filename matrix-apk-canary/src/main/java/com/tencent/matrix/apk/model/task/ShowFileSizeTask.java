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
import com.google.gson.JsonObject;
import com.tencent.matrix.apk.model.task.util.ApkConstants;
import com.tencent.matrix.apk.model.exception.TaskExecuteException;
import com.tencent.matrix.apk.model.exception.TaskInitException;
import com.tencent.matrix.apk.model.job.JobConfig;
import com.tencent.matrix.apk.model.job.JobConstants;
import com.tencent.matrix.apk.model.result.TaskJsonResult;
import com.tencent.matrix.apk.model.result.TaskResult;
import com.tencent.matrix.apk.model.result.TaskResultFactory;
import com.android.utils.Pair;
import com.tencent.matrix.javalib.util.Log;
import com.tencent.matrix.javalib.util.Util;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import static com.tencent.matrix.apk.model.result.TaskResultFactory.TASK_RESULT_TYPE_JSON;
import static com.tencent.matrix.apk.model.task.TaskFactory.TASK_TYPE_SHOW_FILE_SIZE;


/**
 * Created by williamjin on 17/6/1.
 */

public class ShowFileSizeTask extends ApkTask {

    private static final String TAG = "Matrix.ShowFileSizeTask";
    private File inputFile;
    private String order = JobConstants.ORDER_DESC;
    private long             downLimit;
    private Set<String> filterSuffix;
    private List<Pair<String, Long>> entryList;


    public ShowFileSizeTask(JobConfig jobConfig, Map<String, String> params) {
        super(jobConfig, params);
        type = TASK_TYPE_SHOW_FILE_SIZE;
    }

    @Override
    public void init() throws TaskInitException {
        super.init();
        String inputPath = config.getUnzipPath();
        if (Util.isNullOrNil(inputPath)) {
            throw new TaskInitException(TAG + "---APK-UNZIP-PATH can not be null!");
        }
        Log.d(TAG, "inputPath:%s", inputPath);
        inputFile = new File(inputPath);
        if (!inputFile.exists()) {
            throw new TaskInitException(TAG + "---APK-UNZIP-PATH '" + inputPath + "' is not exist!");
        } else if (!inputFile.isDirectory()) {
            throw new TaskInitException(TAG + "---APK-UNZIP-PATH '" + inputPath + "' is not directory!");
        }
        if (params.containsKey(JobConstants.PARAM_MIN_SIZE_IN_KB)) {
            try {
                downLimit = Long.parseLong(params.get(JobConstants.PARAM_MIN_SIZE_IN_KB));
            } catch (NumberFormatException e) {
                Log.e(TAG, "DOWN-LIMIT-SIZE '" + params.get(JobConstants.PARAM_MIN_SIZE_IN_KB) + "' is not number format!");
            }
        }

        if (params.containsKey(JobConstants.PARAM_ORDER)) {
            if (JobConstants.ORDER_ASC.equals(params.get(JobConstants.PARAM_ORDER))) {
                order = JobConstants.ORDER_ASC;
            } else if (JobConstants.ORDER_DESC.equals(params.get(JobConstants.PARAM_ORDER))) {
                order = JobConstants.ORDER_DESC;
            } else {
                Log.e(TAG, "ORDER-BY '" + params.get(JobConstants.PARAM_ORDER) + "' is not correct!");
            }
        }

        filterSuffix = new HashSet<>();

        if (params.containsKey(JobConstants.PARAM_SUFFIX) && !Util.isNullOrNil(params.get(JobConstants.PARAM_SUFFIX))) {
            String[]  suffix = params.get(JobConstants.PARAM_SUFFIX).split(",");
            for (String suffixStr : suffix) {
                filterSuffix.add(suffixStr.trim());
            }
        }

        entryList = new ArrayList<Pair<String, Long>>();
    }

    @Override
    public TaskResult call() throws TaskExecuteException {
        try {
            TaskResult taskResult = TaskResultFactory.factory(getType(), TASK_RESULT_TYPE_JSON, config);
            if (taskResult == null) {
                return null;
            }

            long startTime = System.currentTimeMillis();

            Map<String, Pair<Long, Long>> entrySizeMap = config.getEntrySizeMap();
            if (!entrySizeMap.isEmpty()) {                                                          //take advantage of the result of UnzipTask.
                for (Map.Entry<String, Pair<Long, Long>> entry : entrySizeMap.entrySet()) {
                    final String suffix = getSuffix(entry.getKey());
                    Pair<Long, Long> size = entry.getValue();
                    if (size.getFirst() >= downLimit * ApkConstants.K1024) {
                        if (filterSuffix.isEmpty() || filterSuffix.contains(suffix)) {
                            entryList.add(Pair.of(entry.getKey(), size.getFirst()));
                        } else {
//                            Log.d(TAG, "file: %s, filter by suffix.", entry.getKey());
                        }
                    } else {
//                        Log.d(TAG, "file:%s, size:%d B, downlimit:%d KB", entry.getKey(), size.getFirst(), downLimit);
                    }
                }
            }

            Collections.sort(entryList, new Comparator<Pair<String, Long>>() {
                @Override
                public int compare(Pair<String, Long> entry1, Pair<String, Long> entry2) {
                    long file1Len = entry1.getSecond();
                    long file2Len = entry2.getSecond();
                    if (file1Len < file2Len) {
                        if (order.equals(JobConstants.ORDER_ASC)) {
                            return -1;
                        } else {
                            return 1;
                        }
                    } else if (file1Len > file2Len) {
                        if (order.equals(JobConstants.ORDER_DESC)) {
                            return -1;
                        } else {
                            return 1;
                        }
                    } else {
                        return 0;
                    }
                }
            });

            JsonArray jsonArray = new JsonArray();
            for (Pair<String, Long> sortFile : entryList) {
                JsonObject fileItem = new JsonObject();
                fileItem.addProperty("entry-name", sortFile.getFirst());
                fileItem.addProperty("entry-size", sortFile.getSecond());
                jsonArray.add(fileItem);
            }
            ((TaskJsonResult) taskResult).add("files", jsonArray);
            taskResult.setStartTime(startTime);
            taskResult.setEndTime(System.currentTimeMillis());
            return taskResult;
        } catch (Exception e) {
            throw new TaskExecuteException(e.getMessage(), e);
        }
    }

    private String getSuffix(String name) {
        int index = name.indexOf('.');
        if (index >= 0 && index < name.length() - 1) {
            return name.substring(index + 1);
        }
        return "";
    }
}
