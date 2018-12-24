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

/**
 * Created by jinqiuchen on 17/6/14.
 */

public final class JobConstants {


    public static final String PARAM_CONFIG = "--config";
    public static final String PARAM_INPUT = "--input";
    public static final String PARAM_APK = "--apk";
    public static final String PARAM_UNZIP = "--unzip";
    public static final String PARAM_OUTPUT = "--output";
    public static final String PARAM_FORMAT = "--format";
    public static final String PARAM_FORMAT_JAR = "--formatJar";
    public static final String PARAM_FORMAT_CONFIG = "--formatConfig";
    public static final String PARAM_TOOL_NM = "--toolnm";
    public static final String PARAM_MIN_SIZE_IN_KB = "--min";
    public static final String PARAM_ORDER = "--order";
    public static final String PARAM_GROUP = "--group";
    public static final String PARAM_SUFFIX = "--suffix";
    public static final String PARAM_R_TXT = "--rTxt";
    public static final String PARAM_IGNORE_RESOURCES_LIST = "--ignoreResources";
    public static final String PARAM_MAPPING_TXT = "--mappingTxt";
    public static final String PARAM_RES_MAPPING_TXT = "--resMappingTxt";
    public static final String PARAM_IGNORE_ASSETS_LIST = "--ignoreAssets";

    public static final String OPTION_MANIFEST = "-manifest";
    public static final String OPTION_FILE_SIZE = "-fileSize";
    public static final String OPTION_COUNT_METHOD = "-countMethod";
    public static final String OPTION_CHECK_RES_PROGUARD = "-checkResProguard";
    public static final String OPTION_FIND_NON_ALPHA_PNG = "-findNonAlphaPng";
    public static final String OPTION_CHECK_MULTILIB = "-checkMultiLibrary";
    public static final String OPTION_UNCOMPRESSED_FILE = "-uncompressedFile";
    public static final String OPTION_COUNT_R_CLASS = "-countR";
    public static final String OPTION_DUPLICATE_RESOURCES = "-duplicatedFile";
    public static final String OPTION_CHECK_MULTISTL = "-checkMultiSTL";
    public static final String OPTION_UNUSED_RESOURCES = "-unusedResources";
    public static final String OPTION_UNUSED_ASSETS = "-unusedAssets";
    public static final String OPTION_UNSTRIPPED_SO = "-unstrippedSo";
    public static final String OPTION_COUNT_CLASS = "-countClass";

    public static final String ORDER_ASC = "asc";
    public static final String ORDER_DESC = "desc";
    public static final String GROUP_PACKAGE = "package";
    public static final String GROUP_CLASS = "class";


    public static final String TASK_RESULT_REGISTRY = "TaskResult-Registry";
    public static final String TASK_RESULT_REGISTERY_CLASS = "TaskResult-Registry-Class";

}
