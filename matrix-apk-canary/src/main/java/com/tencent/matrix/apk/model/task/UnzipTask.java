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
import com.tencent.matrix.javalib.util.FileUtil;
import com.tencent.matrix.javalib.util.Log;
import com.tencent.matrix.javalib.util.Util;

import org.apache.commons.io.FileUtils;

import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import static com.tencent.matrix.apk.model.result.TaskResultFactory.TASK_RESULT_TYPE_JSON;
import static com.tencent.matrix.apk.model.task.TaskFactory.TASK_TYPE_UNZIP;

/**
 * Created by williamjin on 17/5/23.
 */

public class UnzipTask extends ApkTask {

    private static final String TAG = "Matrix.UnZipTask";

    @SuppressWarnings("PMD")
    private File inputFile;
    private File outputFile;
    private File mappingTxt;
    private File resMappingTxt;
    private final Map<String, String> proguardClassMap;
    private final Map<String, String> resguardMap;
    private final Map<String, String> resDirMap;
    private final Map<String, String> entryNameMap;
    private final Map<String, Pair<Long, Long>> entrySizeMap;

    public UnzipTask(JobConfig config, Map<String, String> params) {
        super(config, params);
        type = TASK_TYPE_UNZIP;
        proguardClassMap = new HashMap<>();
        resguardMap = new HashMap<>();
        resDirMap = new HashMap<>();
        entryNameMap = new HashMap<>();
        entrySizeMap = new HashMap<>();
    }

    @Override
    public void init() throws TaskInitException {
        super.init();
        if (Util.isNullOrNil(config.getApkPath())) {
            throw new TaskInitException(TAG + "---APK-FILE-PATH can not be null!");
        }
        Log.d(TAG, "inputPath:%s", config.getApkPath());
        inputFile = new File(config.getApkPath());
        if (!inputFile.exists()) {
            throw new TaskInitException(TAG + "---'" + config.getApkPath() + "' is not exist!");
        }
        if (Util.isNullOrNil(config.getUnzipPath())) {
            throw new TaskInitException(TAG + "---APK-UNZIP-PATH can not be null!");
        }
        Log.d(TAG, "outputPath:%s", config.getUnzipPath());
        outputFile = new File(config.getUnzipPath());

        if (!Util.isNullOrNil(config.getMappingFilePath())) {
            mappingTxt = new File(config.getMappingFilePath());
            if (!FileUtil.isLegalFile(mappingTxt)) {
                throw new TaskInitException(TAG + "---mapping file " + config.getMappingFilePath() + " is not legal!");
            }
        }

        if (!Util.isNullOrNil(config.getResMappingFilePath())) {
            resMappingTxt = new File(config.getResMappingFilePath());
            if (!FileUtil.isLegalFile(resMappingTxt)) {
                throw new TaskInitException(TAG + "---resguard mapping file " + config.getResMappingFilePath() + " is not legal!");
            }
        }
    }

    private void readMappingTxtFile() throws IOException {
        if (mappingTxt != null) {
            BufferedReader bufferedReader = new BufferedReader(new FileReader(mappingTxt));
            String line = bufferedReader.readLine();
            String beforeClass = "", afterClass = "";
            try {
                while (line != null) {
                    if (!line.startsWith(" ")) {
                        String[] pair = line.split("->");
                        if (pair.length == 2) {
                            beforeClass = pair[0].trim();
                            afterClass = pair[1].trim();
                            afterClass = afterClass.substring(0, afterClass.length() - 1);
                            if (!Util.isNullOrNil(beforeClass) && !Util.isNullOrNil(afterClass)) {
//                                Log.d(TAG, "before:%s,after:%s", beforeClass, afterClass);
                                proguardClassMap.put(afterClass, beforeClass);
                            }
                        }
                    }
                    line = bufferedReader.readLine();
                }
            } finally {
                bufferedReader.close();
            }
        }
    }

    private String parseResourceNameFromResguard(String resName) {
        if (!Util.isNullOrNil(resName)) {
            int index = resName.indexOf('R');
            if (index >= 0) {
                return resName.substring(index);
            }
        }
        return "";
    }


    private void readResMappingTxtFile() throws IOException {
        if (resMappingTxt != null) {
            BufferedReader bufferedReader = new BufferedReader(new FileReader(resMappingTxt));
            try {
                String line = bufferedReader.readLine();
                boolean readResStart = false;
                boolean readPathStart = false;
                while (line != null) {
                    if (line.trim().equals("res path mapping:")) {
                      readPathStart = true;
                    } else if (line.trim().equals("res id mapping:")) {
                        readResStart = true;
                        readPathStart = false;
                    } else if (readPathStart) {
                        String[] columns = line.split("->");
                        if (columns.length == 2) {
                            String before = columns[0].trim();
                            String after = columns[1].trim();
                            if (!Util.isNullOrNil(before) && !Util.isNullOrNil(after)) {
                                //Log.d(TAG, "%s->%s", before, after);
                                resDirMap.put(after, before);
                            }
                        }
                    } else if (readResStart) {
                        String[] columns = line.split("->");
                        if (columns.length == 2) {
                            String before = parseResourceNameFromResguard(columns[0].trim());
                            String after = parseResourceNameFromResguard(columns[1].trim());
                            if (!Util.isNullOrNil(before) && !Util.isNullOrNil(after)) {
//                                Log.d(TAG, "%s->%s", before, after);
                                resguardMap.put(after, before);
                            }
                        }
                    }
                    line = bufferedReader.readLine();
                }
            } finally {
                bufferedReader.close();
            }
        }
    }

    private String parseResourceNameFromPath(String dir, String filename) {
        if (Util.isNullOrNil(dir) || Util.isNullOrNil(filename)) {
            return "";
        }

        String type = dir.substring(dir.indexOf('/') + 1);
        int index = type.indexOf('-');
        if (index >= 0) {
            type = type.substring(0, index);
        }
        index = filename.indexOf('.');
        if (index >= 0) {
            filename = filename.substring(0, index);
        }
        return "R." + type + "." + filename;
    }

    private String reverseResguard(String dirName, String filename) {
        String outEntryName = "";
        if (resDirMap.containsKey(dirName)) {
            String newDirName = resDirMap.get(dirName);
            final String resource = parseResourceNameFromPath(newDirName, filename);
            int suffixIndex = filename.indexOf('.');
            String suffix = "";
            if (suffixIndex >= 0) {
                suffix = filename.substring(suffixIndex);
            }
            if (resguardMap.containsKey(resource)) {
                int lastIndex =  resguardMap.get(resource).lastIndexOf('.');
                if (lastIndex >= 0) {
                    filename = resguardMap.get(resource).substring(lastIndex + 1) + suffix;
                }
            }
            outEntryName = newDirName + "/" + filename;
        }
        return outEntryName;
    }

    private String writeEntry(ZipFile zipFile, ZipEntry entry) throws IOException {

        int readSize;
        byte[] readBuffer = new byte[4096];
        BufferedOutputStream bufferedOutput = null;
        InputStream zipInputStream = null;
        String entryName = entry.getName();
        String outEntryName = null;
        String filename;
        File dir;
        File file = null;
        int index = entryName.lastIndexOf('/');
        if (index >= 0) {
            filename = entryName.substring(index + 1);
            String dirName = entryName.substring(0, index);
            dir = new File(outputFile, dirName);
            if (!dir.exists() && !dir.mkdirs()) {
                Log.e(TAG, "%s mkdirs failed!", dir.getAbsolutePath());
                return null;
            }
            if (!Util.isNullOrNil(filename)) {
                file = new File(dir, filename);
                outEntryName = reverseResguard(dirName, filename);
                if (Util.isNullOrNil(outEntryName)) {
                    outEntryName = entryName;
                }
            }
        } else {
            file = new File(outputFile, entryName);
            outEntryName = entryName;
        }
        try {
            if (file != null) {
                if (!file.createNewFile()) {
                    Log.e(TAG, "create file %s failed!", file.getAbsolutePath());
                    return null;
                }
                bufferedOutput = new BufferedOutputStream(new FileOutputStream(file));
                zipInputStream = zipFile.getInputStream(entry);
                while ((readSize = zipInputStream.read(readBuffer)) != -1) {
                    bufferedOutput.write(readBuffer, 0, readSize);
                }
            } else {
                return null;
            }
        } finally {
            if (zipInputStream != null) {
                zipInputStream.close();
            }
            if (bufferedOutput != null) {
                bufferedOutput.close();
            }
        }
        return outEntryName;
    }

    @Override
    public TaskResult call() throws TaskExecuteException {

        try {
            ZipFile zipFile = new ZipFile(inputFile);
            if (outputFile.isDirectory() && outputFile.exists()) {
                Log.d(TAG, "%s exists, delete it.", outputFile.getAbsolutePath());
                FileUtils.deleteDirectory(outputFile);
            } else if (outputFile.isFile()) {
                throw new TaskExecuteException(TAG + "---File '" + outputFile.getAbsolutePath() + "' is already exists!");
            }
            TaskResult taskResult = TaskResultFactory.factory(getType(), TASK_RESULT_TYPE_JSON, config);
            if (taskResult == null) {
                return null;
            }
            long startTime = System.currentTimeMillis();
            if (!outputFile.mkdir()) {
                throw new TaskExecuteException(TAG + "---Create directory '" + outputFile.getAbsolutePath() + "' failed!");
            }

            ((TaskJsonResult) taskResult).add("total-size", inputFile.length());

            readMappingTxtFile();
            config.setProguardClassMap(proguardClassMap);
            readResMappingTxtFile();
            config.setResguardMap(resguardMap);

            Enumeration entries = zipFile.entries();
            JsonArray jsonArray = new JsonArray();
            String outEntryName = "";
            while (entries.hasMoreElements()) {
                ZipEntry entry = (ZipEntry) entries.nextElement();
                outEntryName = writeEntry(zipFile, entry);
                if (!Util.isNullOrNil(outEntryName)) {
                    JsonObject fileItem = new JsonObject();
                    fileItem.addProperty("entry-name", outEntryName);
                    fileItem.addProperty("entry-size", entry.getCompressedSize());
                    jsonArray.add(fileItem);
                    entrySizeMap.put(outEntryName, Pair.of(entry.getSize(), entry.getCompressedSize()));
                    entryNameMap.put(entry.getName(), outEntryName);
                }
            }

            config.setEntrySizeMap(entrySizeMap);
            config.setEntryNameMap(entryNameMap);
            ((TaskJsonResult) taskResult).add("entries", jsonArray);
            taskResult.setStartTime(startTime);
            taskResult.setEndTime(System.currentTimeMillis());
            return taskResult;
        } catch (Exception e) {
            throw new TaskExecuteException(e.getMessage(), e);
        }
    }
}
