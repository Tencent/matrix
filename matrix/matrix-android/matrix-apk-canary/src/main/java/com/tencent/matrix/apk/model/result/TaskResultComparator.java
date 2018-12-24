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

package com.tencent.matrix.apk.model.result;


import com.tencent.matrix.apk.model.task.TaskFactory;

import java.util.Comparator;

/**
 * Created by jinqiuchen on 17/12/23.
 */

public class TaskResultComparator implements Comparator<TaskResult> {


    private static final int TASK_IMPORT_LEVEL_1 = 1;
    private static final int TASK_IMPORT_LEVEL_2 = 2;
    private static final int TASK_IMPORT_LEVEL_3 = 3;
    private static final int TASK_IMPORT_LEVEL_LOWEST = 0;

    @Override
    public int compare(TaskResult taskResult1, TaskResult taskResult2) {
        return getImportLevel(taskResult1.taskType) - getImportLevel(taskResult2.taskType);
    }

    public static int getImportLevel(int taskType) {
        int level = TASK_IMPORT_LEVEL_LOWEST;
        switch (taskType) {
            case TaskFactory.TASK_TYPE_UNZIP:
            case TaskFactory.TASK_TYPE_MANIFEST:
                level = TASK_IMPORT_LEVEL_1;
                break;
            case TaskFactory.TASK_TYPE_CHECK_RESGUARD:
            case TaskFactory.TASK_TYPE_DUPLICATE_FILE:
            case TaskFactory.TASK_TYPE_FIND_NON_ALPHA_PNG:
            case TaskFactory.TASK_TYPE_UNSTRIPPED_SO:
            case TaskFactory.TASK_TYPE_UNUSED_ASSETS:
            case TaskFactory.TASK_TYPE_UNUSED_RESOURCES:
            case TaskFactory.TASK_TYPE_UNCOMPRESSED_FILE:
                level = TASK_IMPORT_LEVEL_2;
                break;
            case TaskFactory.TASK_TYPE_CHECK_MULTILIB:
            case TaskFactory.TASK_TYPE_CHECK_MULTISTL:
            case TaskFactory.TASK_TYPE_COUNT_METHOD:
            case TaskFactory.TASK_TYPE_COUNT_R_CLASS:
            case TaskFactory.TASK_TYPE_SHOW_FILE_SIZE:
                level = TASK_IMPORT_LEVEL_3;
                break;
            default:
                break;
        }
        return level;
    }
}
