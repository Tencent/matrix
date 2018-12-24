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


import com.tencent.matrix.apk.model.job.JobConfig;
import com.tencent.matrix.apk.model.job.JobConstants;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * Created by zhangshaowen on 17/6/12.
 */

public final class TaskFactory {
    /*
            The value of TASK_TYPE_XXX is also the index of TaskDescription.
      */
    public static final int TASK_TYPE_UNZIP = 1;
    public static final int TASK_TYPE_MANIFEST = 2;
    public static final int TASK_TYPE_SHOW_FILE_SIZE = 3;
    public static final int TASK_TYPE_COUNT_METHOD = 4;
    public static final int TASK_TYPE_CHECK_RESGUARD = 5;
    public static final int TASK_TYPE_FIND_NON_ALPHA_PNG = 6;
    public static final int TASK_TYPE_CHECK_MULTILIB = 7;
    public static final int TASK_TYPE_UNCOMPRESSED_FILE = 8;
    public static final int TASK_TYPE_COUNT_R_CLASS = 9;
    public static final int TASK_TYPE_DUPLICATE_FILE = 10;
    public static final int TASK_TYPE_CHECK_MULTISTL = 11;
    public static final int TASK_TYPE_UNUSED_RESOURCES = 12;
    public static final int TASK_TYPE_UNUSED_ASSETS = 13;
    public static final int TASK_TYPE_UNSTRIPPED_SO = 14;
    public static final int TASK_TYPE_COUNT_CLASS = 15;

    public static final List<String> TaskDescription = Collections.unmodifiableList(Arrays.asList(
            "Useless Task for default task type.",
            "Unzip the apk file to dest path.",
            "Read package info from the AndroidManifest.xml.",
            "Show files whose size exceed limit size in order.",
            "Count methods in dex file, output results group by class name or package name.",
            "Check if the apk handled by resguard.",
            "Find out the non-alpha png-format files whose size exceed limit size in desc order.",
            "Check if there are more than one library dir in the 'lib'.",
            "Show uncompressed file types.",
            "Count the R class.",
            "Find out the duplicated files.",
            "Check if there are more than one shared library statically linked the STL.",
            "Find out the unused resources.",
            "Find out the unused assets.",
            "Find out the unstripped shared library files.",
            "Count classes in dex file, output results group by package name."));



    public static final List<String> TaskOptionName = Collections.unmodifiableList(Arrays.asList(
            "", "",
            JobConstants.OPTION_MANIFEST,
            JobConstants.OPTION_FILE_SIZE,
            JobConstants.OPTION_COUNT_METHOD,
            JobConstants.OPTION_CHECK_RES_PROGUARD,
            JobConstants.OPTION_FIND_NON_ALPHA_PNG,
            JobConstants.OPTION_CHECK_MULTILIB,
            JobConstants.OPTION_UNCOMPRESSED_FILE,
            JobConstants.OPTION_COUNT_R_CLASS,
            JobConstants.OPTION_DUPLICATE_RESOURCES,
            JobConstants.OPTION_CHECK_MULTISTL,
            JobConstants.OPTION_UNUSED_RESOURCES,
            JobConstants.OPTION_UNUSED_ASSETS,
            JobConstants.OPTION_UNSTRIPPED_SO,
            JobConstants.OPTION_COUNT_CLASS
    ));


    public static ApkTask factory(int taskType, JobConfig config, Map<String, String> params) {
        ApkTask task = null;
        switch (taskType) {
            case TASK_TYPE_UNZIP:
                task = new UnzipTask(config, params);
                break;
            case TASK_TYPE_MANIFEST:
                task = new ManifestAnalyzeTask(config, params);
                break;
            case TASK_TYPE_SHOW_FILE_SIZE:
                task = new ShowFileSizeTask(config, params);
                break;
            case TASK_TYPE_COUNT_METHOD:
                task = new MethodCountTask(config, params);
                break;
            case TASK_TYPE_CHECK_RESGUARD:
                task = new ResProguardCheckTask(config, params);
                break;
            case TASK_TYPE_FIND_NON_ALPHA_PNG:
                task = new FindNonAlphaPngTask(config, params);
                break;
            case TASK_TYPE_CHECK_MULTILIB:
                task = new MultiLibCheckTask(config, params);
                break;
            case TASK_TYPE_UNCOMPRESSED_FILE:
                task = new UncompressedFileTask(config, params);
                break;
            case TASK_TYPE_COUNT_R_CLASS:
                task = new CountRTask(config, params);
                break;
            case TASK_TYPE_DUPLICATE_FILE:
                task = new DuplicateFileTask(config, params);
                break;
            case TASK_TYPE_CHECK_MULTISTL:
                task = new MultiSTLCheckTask(config, params);
                break;
            case TASK_TYPE_UNUSED_RESOURCES:
                task = new UnusedResourcesTask(config, params);
                break;
            case TASK_TYPE_UNUSED_ASSETS:
                task = new UnusedAssetsTask(config, params);
                break;
            case TASK_TYPE_UNSTRIPPED_SO:
                task = new UnStrippedSoCheckTask(config, params);
                break;
            case TASK_TYPE_COUNT_CLASS:
                task = new CountClassTask(config, params);
                break;
            default:
                break;
        }
        return task;
    }

}
