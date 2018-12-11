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
import com.tencent.matrix.apk.model.exception.TaskExecuteException;
import com.tencent.matrix.apk.model.exception.TaskInitException;
import com.tencent.matrix.apk.model.job.JobConfig;
import com.tencent.matrix.apk.model.result.TaskJsonResult;
import com.tencent.matrix.apk.model.result.TaskResult;
import com.tencent.matrix.apk.model.result.TaskResultFactory;
import com.android.utils.Pair;
import com.tencent.matrix.javalib.util.Util;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Created by williamjin on 17/6/27.
 */

public class DuplicateFileTask extends ApkTask {

    private static final String TAG = "Matrix.DuplicateFileTask";

    private File inputFile;
    private Map<String, List<String>> md5Map;
    private List<Pair<String, Long>> fileSizeList;
    private Map<String, Pair<Long, Long>> entrySizeMap;
    private Map<String, String> entryNameMap;

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
        md5Map = new HashMap<>();
        fileSizeList = new ArrayList<>();
        entrySizeMap = config.getEntrySizeMap();
        entryNameMap = config.getEntryNameMap();
    }

    private void computeMD5(File file) throws NoSuchAlgorithmException, IOException {
        if (file != null) {
            if (file.isDirectory()) {
                File[] files = file.listFiles();
                for (File resFile : files) {
                    computeMD5(resFile);
                }
            } else {
                MessageDigest msgDigest = MessageDigest.getInstance("MD5");
                BufferedInputStream inputStream = new BufferedInputStream(new FileInputStream(file));
                byte[] buffer = new byte[512];
                int readSize = 0;
                long totalRead = 0;
                while ((readSize = inputStream.read(buffer)) > 0) {
                    msgDigest.update(buffer, 0, readSize);
                    totalRead += readSize;
                }
                inputStream.close();
                if (totalRead > 0) {
                    final String md5 = Util.byteArrayToHex(msgDigest.digest());
                    String filename = file.getAbsolutePath().substring(inputFile.getAbsolutePath().length() + 1);
                    if (entryNameMap.containsKey(filename)) {
                        filename = entryNameMap.get(filename);
                    }
                    if (!md5Map.containsKey(md5)) {
                        md5Map.put(md5, new ArrayList<String>());
                        if (entrySizeMap.containsKey(filename)) {
                            fileSizeList.add(Pair.of(md5, entrySizeMap.get(filename).getFirst()));
                        } else {
                            fileSizeList.add(Pair.of(md5, totalRead));
                        }
                    }
                    md5Map.get(md5).add(filename);
                }
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

            computeMD5(inputFile);

            Collections.sort(fileSizeList, new Comparator<Pair<String, Long>>() {
                @Override
                public int compare(Pair<String, Long> entry1, Pair<String, Long> entry2) {
                    long file1Len = entry1.getSecond();
                    long file2Len = entry2.getSecond();
                    if (file1Len < file2Len) {
                        return 1;
                    } else if (file1Len > file2Len) {
                        return -1;
                    } else {
                        return 0;
                    }
                }
            });

            for (Pair<String, Long> entry : fileSizeList) {
                if (md5Map.get(entry.getFirst()).size() > 1) {
                    JsonObject jsonObject = new JsonObject();
                    jsonObject.addProperty("md5", entry.getFirst());
                    jsonObject.addProperty("size", entry.getSecond());
                    JsonArray jsonFiles = new JsonArray();
                    for (String filename : md5Map.get(entry.getFirst())) {
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
