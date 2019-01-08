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
import com.android.dexdeps.MethodRef;
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
import com.tencent.matrix.apk.model.task.util.ApkUtil;
import com.tencent.matrix.javalib.util.Log;
import com.tencent.matrix.javalib.util.Util;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

import static com.tencent.matrix.apk.model.result.TaskResultFactory.TASK_RESULT_TYPE_JSON;
import static com.tencent.matrix.apk.model.task.TaskFactory.TASK_TYPE_COUNT_METHOD;

/**
 * Created by jinqiuchen on 17/6/1.
 */

public class MethodCountTask extends ApkTask {

    private static final String TAG = "Matrix.MethodCountTask";

    private File inputFile;
    private String group = JobConstants.GROUP_PACKAGE;
    private final List<String>           dexFileNameList;
    private final List<RandomAccessFile> dexFileList;
    private final Map<String, Integer> classInternalMethod;
    private final Map<String, Integer> classExternalMethod;
    private final Map<String, Integer> pkgInternalRefMethod;
    private final Map<String, Integer> pkgExternalMethod;

    public MethodCountTask(JobConfig config, Map<String, String> params) {
        super(config, params);
        type = TASK_TYPE_COUNT_METHOD;
        dexFileNameList = new ArrayList<String>();
        dexFileList = new ArrayList<RandomAccessFile>();
        classInternalMethod = new HashMap<String, Integer>();
        classExternalMethod = new HashMap<String, Integer>();
        pkgInternalRefMethod = new HashMap<String, Integer>();
        pkgExternalMethod = new HashMap<String, Integer>();
    }

    @Override
    public void init() throws TaskInitException {
        super.init();
        String inputPath = config.getUnzipPath();
        if (Util.isNullOrNil(inputPath)) {
            throw new TaskInitException(TAG + "---APK-UNZIP-PATH can not be null!");
        }
        Log.i(TAG, "input path:%s", inputPath);
        inputFile = new File(inputPath);
        if (!inputFile.exists()) {
            throw new TaskInitException(TAG + "---APK-UNZIP-PATH '" + inputPath + "' is not exist!");
        } else if (!inputFile.isDirectory()) {
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
            } else if (JobConstants.GROUP_CLASS.equals(params.get(JobConstants.PARAM_GROUP))) {
                group = JobConstants.GROUP_CLASS;
            } else {
                Log.e(TAG, "GROUP-BY '" + params.get(JobConstants.PARAM_GROUP) + "' is not correct!");
            }
        }
    }

    private void countDex(RandomAccessFile dexFile) throws IOException {
        classInternalMethod.clear();
        classExternalMethod.clear();
        pkgInternalRefMethod.clear();
        pkgExternalMethod.clear();
        DexData dexData = new DexData(dexFile);
        dexData.load();
        MethodRef[] methodRefs = dexData.getMethodRefs();
        ClassRef[] externalClassRefs = dexData.getExternalReferences();
        Map<String, String> proguardClassMap = config.getProguardClassMap();
        String className = null;
        for (ClassRef classRef : externalClassRefs) {
            className = ApkUtil.getNormalClassName(classRef.getName());
            if (proguardClassMap.containsKey(className)) {
                className = proguardClassMap.get(className);
            }
            if (className.indexOf('.') == -1) {
                continue;
            }
            classExternalMethod.put(className, 0);
        }
        for (MethodRef methodRef : methodRefs) {
            className = ApkUtil.getNormalClassName(methodRef.getDeclClassName());
            if (proguardClassMap.containsKey(className)) {
                className = proguardClassMap.get(className);
            }
            if (!Util.isNullOrNil(className)) {
                if (className.indexOf('.') == -1) {
                    continue;
                }
                if (classExternalMethod.containsKey(className)) {
                    classExternalMethod.put(className, classExternalMethod.get(className) + 1);
                } else if (classInternalMethod.containsKey(className)) {
                    classInternalMethod.put(className, classInternalMethod.get(className) + 1);
                } else {
                    classInternalMethod.put(className, 1);
                }
            }
        }

        //remove 0-method referenced class
        Iterator<String> iterator = classExternalMethod.keySet().iterator();
        while (iterator.hasNext()) {
            if (classExternalMethod.get(iterator.next()) == 0) {
                iterator.remove();
            }
        }
    }

    private List<String> sortKeyByValue(final Map<String, Integer> map) {
        List<String> list = new LinkedList<>();
        list.addAll(map.keySet());
        Collections.sort(list, new Comparator<String>() {
            @Override
            public int compare(String class1, String class2) {
                if (map.get(class1) > map.get(class2)) {
                    return -1;
                } else if (map.get(class1) < map.get(class2)) {
                    return 1;
                } else {
                    return 0;
                }
            }
        });
        return list;
    }

    private int sumOfValue(final Map<String, Integer> map) {
        Iterator<Integer> iterator = map.values().iterator();
        int sum = 0;
        while (iterator.hasNext()) {
            sum += iterator.next();
        }
        return sum;
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
            for (int i = 0; i < dexFileList.size(); i++) {
                RandomAccessFile dexFile = dexFileList.get(i);
                countDex(dexFile);
                int totalInternalMethods = sumOfValue(classInternalMethod);
                int totalExternalMethods = sumOfValue(classExternalMethod);
                JsonObject jsonObject = new JsonObject();
                jsonObject.addProperty("dex-file", dexFileNameList.get(i));

                if (JobConstants.GROUP_CLASS.equals(group)) {
                    List<String> sortList = sortKeyByValue(classInternalMethod);
                    JsonArray classes = new JsonArray();
                    for (String className : sortList) {
                        JsonObject classObj = new JsonObject();
                        classObj.addProperty("name", className);
                        classObj.addProperty("methods", classInternalMethod.get(className));
                        classes.add(classObj);
                    }
                    jsonObject.add("internal-classes", classes);
                } else if (JobConstants.GROUP_PACKAGE.equals(group)) {
                    String packageName;
                    for (Map.Entry<String, Integer> entry : classInternalMethod.entrySet()) {
                        packageName = ApkUtil.getPackageName(entry.getKey());
                        if (!Util.isNullOrNil(packageName)) {
                            if (!pkgInternalRefMethod.containsKey(packageName)) {
                                pkgInternalRefMethod.put(packageName, entry.getValue());
                            } else {
                                pkgInternalRefMethod.put(packageName, pkgInternalRefMethod.get(packageName) + entry.getValue());
                            }
                        }
                    }
                    List<String> sortList = sortKeyByValue(pkgInternalRefMethod);
                    JsonArray packages = new JsonArray();
                    for (String pkgName : sortList) {
                        JsonObject pkgObj = new JsonObject();
                        pkgObj.addProperty("name", pkgName);
                        pkgObj.addProperty("methods", pkgInternalRefMethod.get(pkgName));
                        packages.add(pkgObj);
                    }
                    jsonObject.add("internal-packages", packages);
                }
                jsonObject.addProperty("total-internal-classes", classInternalMethod.size());
                jsonObject.addProperty("total-internal-methods", totalInternalMethods);

                if (JobConstants.GROUP_CLASS.equals(group)) {
                    List<String> sortList = sortKeyByValue(classExternalMethod);
                    JsonArray classes = new JsonArray();
                    for (String className : sortList) {
                        JsonObject classObj = new JsonObject();
                        classObj.addProperty("name", className);
                        classObj.addProperty("methods", classExternalMethod.get(className));
                        classes.add(classObj);
                    }
                    jsonObject.add("external-classes", classes);

                } else if (JobConstants.GROUP_PACKAGE.equals(group)) {
                    String packageName = "";
                    for (Map.Entry<String, Integer> entry : classExternalMethod.entrySet()) {
                        packageName = ApkUtil.getPackageName(entry.getKey());
                        if (!Util.isNullOrNil(packageName)) {
                            if (!pkgExternalMethod.containsKey(packageName)) {
                                pkgExternalMethod.put(packageName, entry.getValue());
                            } else {
                                pkgExternalMethod.put(packageName, pkgExternalMethod.get(packageName) + entry.getValue());
                            }
                        }
                    }
                    List<String> sortList = sortKeyByValue(pkgExternalMethod);
                    JsonArray packages = new JsonArray();
                    for (String pkgName : sortList) {
                        JsonObject pkgObj = new JsonObject();
                        pkgObj.addProperty("name", pkgName);
                        pkgObj.addProperty("methods", pkgExternalMethod.get(pkgName));
                        packages.add(pkgObj);
                    }
                    jsonObject.add("external-packages", packages);

                }
                jsonObject.addProperty("total-external-classes", classExternalMethod.size());
                jsonObject.addProperty("total-external-methods", totalExternalMethods);
                jsonArray.add(jsonObject);
            }
            ((TaskJsonResult) taskResult).add("dex-files", jsonArray);
            taskResult.setStartTime(startTime);
            taskResult.setEndTime(System.currentTimeMillis());
            return taskResult;
        } catch (Exception e) {
            throw new TaskExecuteException(e.getMessage(), e);
        }
    }
}
