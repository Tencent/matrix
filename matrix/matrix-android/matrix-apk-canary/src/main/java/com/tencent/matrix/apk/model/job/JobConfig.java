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

package com.tencent.matrix.apk.model.job;

import com.google.gson.JsonArray;

import com.android.utils.Pair;

import java.util.List;
import java.util.Map;

/**
 * Created by jinqiuchen on 17/6/15.
 */

public final class JobConfig {

    private String inputDir;
    private String apkPath;
    private String unzipPath;
    private String outputPath;
    private String mappingFilePath;
    private String resMappingFilePath;
    private JsonArray outputConfig;

    private List<String> outputFormatList;
    private Map<String, String> proguardClassMap;
    private Map<String, String> resguardMap;
    private Map<String, Pair<Long, Long>> entrySizeMap;
    private Map<String, String> entryNameMap;
    private Map<String, Long> entryNameCrcMap;

    public String getInputDir() {
        return inputDir;
    }

    public void setInputDir(String inputDir) {
        this.inputDir = inputDir;
    }

    public String getApkPath() {
        return apkPath;
    }

    public void setApkPath(String apkPath) {
        this.apkPath = apkPath;
    }

    public List<String> getOutputFormatList() {
        return outputFormatList;
    }

    public void setOutputFormatList(List<String> outputFormatList) {
        this.outputFormatList = outputFormatList;
    }

    public String getUnzipPath() {
        return unzipPath;
    }

    public void setUnzipPath(String unzipPath) {
        this.unzipPath = unzipPath;
    }

    public String getOutputPath() {
        return outputPath;
    }

    public void setOutputPath(String outputPath) {
        this.outputPath = outputPath;
    }

    public String getMappingFilePath() {
        return mappingFilePath;
    }

    public void setMappingFilePath(String mappingFilePath) {
        this.mappingFilePath = mappingFilePath;
    }

    public String getResMappingFilePath() {
        return resMappingFilePath;
    }

    public void setResMappingFilePath(String resMappingFilePath) {
        this.resMappingFilePath = resMappingFilePath;
    }

    public Map<String, String> getProguardClassMap() {
        return proguardClassMap;
    }

    public void setProguardClassMap(Map<String, String> proguardClassMap) {
        this.proguardClassMap = proguardClassMap;
    }

    public Map<String, String> getResguardMap() {
        return resguardMap;
    }

    public void setResguardMap(Map<String, String> resguardMap) {
        this.resguardMap = resguardMap;
    }

    public Map<String, Pair<Long, Long>> getEntrySizeMap() {
        return entrySizeMap;
    }

    public void setEntrySizeMap(Map<String, Pair<Long, Long>> entrySizeMap) {
        this.entrySizeMap = entrySizeMap;
    }

    public Map<String, String> getEntryNameMap() {
        return entryNameMap;
    }

    public void setEntryNameMap(Map<String, String> entryNameMap) {
        this.entryNameMap = entryNameMap;
    }

    public Map<String, Long> getEntryNameCrcMap() {
        return entryNameCrcMap;
    }

    public void setEntryNameCrcMap(Map<String, Long> entryNameCrcMap) {
        this.entryNameCrcMap = entryNameCrcMap;
    }

    public JsonArray getOutputConfig() {
        return outputConfig;
    }

    public void setOutputConfig(JsonArray outputConfig) {
        this.outputConfig = outputConfig;
    }

}
