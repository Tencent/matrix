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
import com.tencent.matrix.javalib.util.Log;
import com.tencent.matrix.javalib.util.Util;
import com.android.utils.Pair;
import java.awt.image.BufferedImage;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Map;

import javax.imageio.ImageIO;

import static com.tencent.matrix.apk.model.task.TaskFactory.TASK_TYPE_FIND_NON_ALPHA_PNG;

/**
 * Created by jinqiuchen on 17/6/12.
 */

public class FindNonAlphaPngTask extends ApkTask {

    private static final String TAG = "Matrix.FindNonAlphaPngTask";

    private File inputFile;
    private List<Pair<String, Long>> nonAlphaPngList;
    private long downLimitSize;
    private Map<String, Pair<Long, Long>> entrySizeMap;
    private Map<String, String> entryNameMap;

    public FindNonAlphaPngTask(JobConfig config, Map<String, String> params) {
        super(config, params);
        type = TASK_TYPE_FIND_NON_ALPHA_PNG;
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
        if (params.containsKey(JobConstants.PARAM_MIN_SIZE_IN_KB)) {
            try {
                downLimitSize = Long.parseLong(params.get(JobConstants.PARAM_MIN_SIZE_IN_KB));
            } catch (NumberFormatException e) {
                Log.e(TAG, "DOWN-LIMIT-SIZE '" + params.get(JobConstants.PARAM_MIN_SIZE_IN_KB) + "' is not number format!");
            }
        }
        nonAlphaPngList = new ArrayList<Pair<String, Long>>();
        entrySizeMap = config.getEntrySizeMap();
        entryNameMap = config.getEntryNameMap();
    }

    private void findNonAlphaPng(File file) throws IOException {
        if (file != null) {
            if (file.isDirectory()) {
                File[] files = file.listFiles();
                for (File tempFile : files) {
                    findNonAlphaPng(tempFile);
                }
            } else if (file.isFile() && file.getName().endsWith(ApkConstants.PNG_FILE_SUFFIX) && !file.getName().endsWith(ApkConstants.NINE_PNG)) {
                BufferedImage bufferedImage = ImageIO.read(file);
                if (!bufferedImage.getColorModel().hasAlpha()) {
                    String filename = file.getAbsolutePath().substring(inputFile.getAbsolutePath().length() + 1);
                    if (entryNameMap.containsKey(filename)) {
                        filename = entryNameMap.get(filename);
                    }
                    long size = file.length();
                    if (entrySizeMap.containsKey(filename)) {
                        size = entrySizeMap.get(filename).getFirst();
                    }
                    if (size >= downLimitSize * ApkConstants.K1024) {
                        nonAlphaPngList.add(Pair.of(filename, file.length()));
                    }
                }
            }
        }
    }

    @Override
    public TaskResult call() throws TaskExecuteException {
        File resDir = new File(inputFile, ApkConstants.RESOURCE_DIR_PROGUARD_NAME);
        TaskResult taskResult = null;
        try {
            taskResult = TaskResultFactory.factory(getType(), TaskResultFactory.TASK_RESULT_TYPE_JSON, config);
            long startTime = System.currentTimeMillis();
            if (resDir.exists() && resDir.isDirectory()) {
                findNonAlphaPng(resDir);
            } else {
                resDir = new File(inputFile, ApkConstants.RESOURCE_DIR_NAME);
                if (resDir.exists() && resDir.isDirectory()) {
                    findNonAlphaPng(resDir);
                }
            }

            Collections.sort(nonAlphaPngList, new Comparator<Pair<String, Long>>() {
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

            JsonArray jsonArray = new JsonArray();
            for (Pair<String, Long> entry : nonAlphaPngList) {
                if (!Util.isNullOrNil(entry.getFirst())) {
                    JsonObject jsonObject = new JsonObject();
                    jsonObject.addProperty("entry-name", entry.getFirst());
                    jsonObject.addProperty("entry-size", entry.getSecond());
                    jsonArray.add(jsonObject);
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
