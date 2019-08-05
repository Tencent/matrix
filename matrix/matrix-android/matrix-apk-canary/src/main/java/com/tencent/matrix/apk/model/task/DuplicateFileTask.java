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
import com.tencent.matrix.apk.model.result.TaskJsonResult;
import com.tencent.matrix.apk.model.result.TaskResult;
import com.tencent.matrix.apk.model.result.TaskResultFactory;
import com.tencent.matrix.javalib.util.Util;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Created by jinqiuchen on 17/6/27.
 */

public class DuplicateFileTask extends ApkTask {

    private static final String TAG = "Matrix.DuplicateFileTask";

    private File inputFile;
    private Map<Long, List<String>> crcMap;
    private List<Pair<Long, Long>> fileSizeList;
    private Map<String, Pair<Long, Long>> entrySizeMap;
    private Map<String, String> entryNameMap;
    private Map<String, Long> entryNameCrcMap;

    public DuplicateFileTask(JobConfig config, Map<String, String> params) {
        super(config, params);
        type = TaskFactory.TASK_TYPE_DUPLICATE_FILE;
    }

    @Override
    public void init() throws TaskInitException {
        super.init();
        String inputPath = config.getUnzipPath();
        if (Util.isNullOrNil(inputPath)) {
            throw new TaskInitException(TAG + "---APK-UNZIP-PATH can not be null!");
        }
        inputFile = new File(inputPath);
        if (!inputFile.exists()) {
            throw new TaskInitException(TAG + "---APK-UNZIP-PATH '" + inputPath + "'is not exists!");
        } else if (!inputFile.isDirectory()) {
            throw new TaskInitException(TAG + "---APK-UNZIP-PATH '" + inputPath + "' is not directory!");
        }
        crcMap = new HashMap<>();
        fileSizeList = new ArrayList<>();
        entrySizeMap = config.getEntrySizeMap();
        entryNameMap = config.getEntryNameMap();
        entryNameCrcMap = config.getEntryNameCrcMap();
    }

    private void computeCrc(File file) throws IOException {
        if (file != null) {
            if (file.isDirectory()) {
                File[] files = file.listFiles();
                for (File resFile : files) {
                    computeCrc(resFile);
                }
            } else {
                String filename = file.getAbsolutePath().substring(inputFile.getAbsolutePath().length() + 1);
                String entryName = entryNameMap.get(filename);
                filename = entryName != null ? entryName : filename;
                Long crcValue = entryNameCrcMap.get(filename);

                if (!crcMap.containsKey(crcValue)) {
                    crcMap.put(crcValue, new ArrayList<String>());
                    if (entrySizeMap.containsKey(filename)) {
                        fileSizeList.add(Pair.of(crcValue, entrySizeMap.get(filename).getFirst()));
                    } else {
                        BufferedInputStream inputStream = new BufferedInputStream(new FileInputStream(file));
                        byte[] buffer = new byte[512];
                        int readSize;
                        long totalRead = 0;
                        while ((readSize = inputStream.read(buffer)) > 0) {
                            totalRead += readSize;
                        }
                        inputStream.close();
                        fileSizeList.add(Pair.of(crcValue, totalRead));
                    }
                }
                crcMap.get(crcValue).add(filename);
            }
        }
    }

    @Override
    public TaskResult call() throws TaskExecuteException {
        TaskResult taskResult = null;
        try {
            taskResult = TaskResultFactory.factory(getType(), TaskResultFactory.TASK_RESULT_TYPE_JSON, config);
            long startTime = System.currentTimeMillis();
            JsonArray jsonArray = new JsonArray();

            computeCrc(inputFile);

            Collections.sort(fileSizeList, new Comparator<Pair<Long, Long>>() {
                @Override
                public int compare(Pair<Long, Long> entry1, Pair<Long, Long> entry2) {
                    return Long.compare(entry1.getSecond(), entry2.getSecond());
                }
            });

            for (Pair<Long, Long> entry : fileSizeList) {
                if (crcMap.get(entry.getFirst()).size() > 1) {
                    JsonObject jsonObject = new JsonObject();
                    jsonObject.addProperty("crc", entry.getFirst());
                    jsonObject.addProperty("size", entry.getSecond());
                    JsonArray jsonFiles = new JsonArray();
                    for (String filename : crcMap.get(entry.getFirst())) {
                        jsonFiles.add(filename);
                    }
                    jsonObject.add("files", jsonFiles);
                    jsonArray.add(jsonObject);
                }
            }
            ((TaskJsonResult) taskResult).add("files", jsonArray);
            taskResult.setStartTime(startTime);
            taskResult.setEndTime(System.currentTimeMillis());
        } catch (Exception e) {
            throw new TaskExecuteException(e.getMessage(), e);
        }
        return taskResult;
    }
}
