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


import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.tencent.matrix.apk.model.task.TaskFactory;
import com.tencent.matrix.javalib.util.Util;

import javax.xml.parsers.ParserConfigurationException;

/**
 * Created by williamjin on 17/5/23.
 */

public class TaskJsonResult extends TaskResult {

    protected final JsonObject jsonObject;

    protected final JsonObject config;

    public TaskJsonResult(int taskType, JsonObject config) throws ParserConfigurationException {
        super(taskType);
        this.config = config;
        jsonObject = new JsonObject();
        jsonObject.addProperty("taskType", taskType);
        jsonObject.addProperty("taskDescription", TaskFactory.TaskDescription.get(taskType));
    }

    public void add(String name, String value) {
        if (!Util.isNullOrNil(name)) {
            jsonObject.addProperty(name, value);
        }
    }

    public void add(String name, boolean value) {
        if (!Util.isNullOrNil(name)) {
            jsonObject.addProperty(name, value);
        }
    }

    public void add(String name, Number value) {
        if (!Util.isNullOrNil(name)) {
            jsonObject.addProperty(name, value);
        }
    }

    public void add(String name, JsonElement jsonElement) {
        if (!Util.isNullOrNil(name)) {
            jsonObject.add(name, jsonElement);
        }
    }

    @Override
    public void setStartTime(long startTime) {
        super.setStartTime(startTime);
        jsonObject.addProperty("start-time", this.startTime);
    }

    @Override
    public void setEndTime(long endTime) {
        super.setEndTime(endTime);
        jsonObject.addProperty("end-time", this.endTime);
    }

    public void format(JsonObject jsonObject) {
        //do nothing
    }

    @Override
    public String toString() {
        return jsonObject.toString();
    }

    @Override
    public JsonObject getResult() {
        return jsonObject;
    }
}
