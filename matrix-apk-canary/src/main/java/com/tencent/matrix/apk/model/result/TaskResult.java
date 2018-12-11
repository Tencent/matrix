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


import java.text.SimpleDateFormat;
import java.util.Calendar;

/**
 * Created by williamjin on 17/5/23.
 */
@SuppressWarnings("PMD")
public abstract class TaskResult {
    private final SimpleDateFormat dateFormat;
    protected String startTime;
    protected String endTime;
    public final int taskType;

    public TaskResult(int taskType) {
        this.taskType = taskType;
        dateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss:SSS");
    }

    public void setStartTime(long startTime) {
        Calendar calendar = Calendar.getInstance();
        calendar.setTimeInMillis(startTime);
        this.startTime = dateFormat.format(calendar.getTime());
    }

    public void setEndTime(long endTime) {
        Calendar calendar = Calendar.getInstance();
        calendar.setTimeInMillis(endTime);
        this.endTime = dateFormat.format(calendar.getTime());
    }

    public abstract Object getResult();

}
