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

import com.android.dexdeps.ClassRef;
import com.android.dexdeps.DexData;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import com.tencent.matrix.apk.model.exception.TaskExecuteException;
import com.tencent.matrix.apk.model.exception.TaskInitException;
import com.tencent.matrix.apk.model.job.JobConfig;
import com.tencent.matrix.apk.model.job.JobConstants;
import com.tencent.matrix.apk.model.result.TaskJsonResult;
import com.tencent.matrix.apk.model.result.TaskResult;
import com.tencent.matrix.apk.model.result.TaskResultFactory;
import com.tencent.matrix.apk.model.task.util.ApkConstants;
import com.tencent.matrix.apk.model.task.util.ApkUtil;
import com.tencent.matrix.javalib.util.Log;
import com.tencent.matrix.javalib.util.Util;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.RandomAccessFile;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import static com.tencent.matrix.apk.model.task.TaskFactory.TASK_TYPE_COUNT_CLASS;

/**
 * Created by jinqiuchen on 17/11/13.
 */

public class CountClassTask extends ApkTask {

    private static final String TAG = "Matrix.CountClassTask";

    private File inputFile;
    private String group = JobConstants.GROUP_PACKAGE;
    private final List<String> dexFileNameList;
    private final List<RandomAccessFile> dexFileList;

    public CountClassTask(JobConfig config, Map<String, String> params) {
        super(config, params);
        type = TASK_TYPE_COUNT_CLASS;
        dexFileNameList = new ArrayList<>();
        dexFileList = new ArrayList<>();
    }

    @Override
    public void init() throws TaskInitException {
        super.init();
        String inputPath = config.getUnzipPath();

        if (Util.isNullOrNil(inputPath)) {
            throw new TaskInitException(TAG + "---APK-UNZIP-PATH can not be null!");
        }

        Log.d(TAG, "input path:%s", inputPath);

        inputFile = new File(inputPath);
        if (!inputFile.exists()) {
            throw new TaskInitException(TAG + "---APK-UNZIP-PATH '" + inputPath + "' is not exist!");
        }
        if (!inputFile.isDirectory()) {
            throw new TaskInitException(TAG + "---APK-UNZIP-PATH '" + inputPath + "' is not directory!");
        }

        File[] files = inputFile.listFiles();
        try {
            if (files != null) {
                for (File file : files) {
                    if (file.isFile() && file.getName().endsWith(ApkConstants.DEX_FILE_SUFFIX)) {
                        dexFileNameList.add(file.getName());
                        RandomAccessFile randomAccessFile = new RandomAccessFile(file, "rw");
                        dexFileList.add(randomAccessFile);
                    }
                }
            }
        } catch (FileNotFoundException e) {
            throw new TaskInitException(e.getMessage(), e);
        }

        if (params.containsKey(JobConstants.PARAM_GROUP)) {
            if (JobConstants.GROUP_PACKAGE.equals(params.get(JobConstants.PARAM_GROUP))) {
                group = JobConstants.GROUP_PACKAGE;
            } else {
                Log.e(TAG, "GROUP-BY '" + params.get(JobConstants.PARAM_GROUP) + "' is not correct!");
            }
        }

    }

    @Override
    public TaskResult call() throws TaskExecuteException {
        try {
            TaskResult taskResult = TaskResultFactory.factory(type, TaskResultFactory.TASK_RESULT_TYPE_JSON, config);
            long startTime = System.currentTimeMillis();
            Map<String, String> classProguardMap = config.getProguardClassMap();
            JsonArray dexFiles = new JsonArray();

            for (int i = 0; i < dexFileList.size(); i++) {
                RandomAccessFile dexFile = dexFileList.get(i);
                DexData dexData = new DexData(dexFile);
                dexData.load();
                ClassRef[] defClassRefs = dexData.getInternalReferences();
                Set<String> classNameSet = new HashSet<>();
                for (ClassRef classRef : defClassRefs) {
                    String className = ApkUtil.getNormalClassName(classRef.getName());
                    if (classProguardMap.containsKey(className)) {
                        className = classProguardMap.get(className);
                    }
                    if (className.indexOf('.') == -1) {
                        continue;
                    }
                    classNameSet.add(className);
                }
                JsonObject jsonObject = new JsonObject();
                jsonObject.addProperty("dex-file", dexFileNameList.get(i));
                //Log.d(TAG, "dex %s, classes %s", dexFileNameList.get(i), classNameSet.toString());

                Map<String, Set<String>> packageClass = new HashMap<>();
                if (JobConstants.GROUP_PACKAGE.equals(group)) {
                    String packageName = "";
                    for (String clazzName : classNameSet) {
                        packageName = ApkUtil.getPackageName(clazzName);
                        if (!Util.isNullOrNil(packageName)) {
                            if (!packageClass.containsKey(packageName)) {
                                packageClass.put(packageName, new HashSet<String>());
                            }
                            packageClass.get(packageName).add(clazzName);
                        }
                    }
                    JsonArray packages = new JsonArray();
                    for (Map.Entry<String, Set<String>> pkg : packageClass.entrySet()) {
                        JsonObject pkgObj = new JsonObject();
                        pkgObj.addProperty("package", pkg.getKey());
                        JsonArray classArray = new JsonArray();
                        for (String clazz : pkg.getValue()) {
                            classArray.add(clazz);
                        }
                        pkgObj.add("classes", classArray);
                        packages.add(pkgObj);
                    }
                    jsonObject.add("packages", packages);
                }
                dexFiles.add(jsonObject);
            }

            ((TaskJsonResult) taskResult).add("dex-files", dexFiles);
            taskResult.setStartTime(startTime);
            taskResult.setEndTime(System.currentTimeMillis());
            return taskResult;
        } catch (Exception e) {
            throw new TaskExecuteException(e.getMessage(), e);
        }
    }

    @Override
    public int getType() {
        return super.getType();
    }
}
