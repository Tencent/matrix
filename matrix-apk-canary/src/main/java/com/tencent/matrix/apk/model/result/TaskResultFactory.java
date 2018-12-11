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


import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import com.tencent.matrix.apk.model.job.JobConfig;
import com.tencent.matrix.apk.model.task.TaskFactory;
import com.tencent.matrix.javalib.util.Log;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.util.HashMap;
import java.util.Map;

import javax.xml.parsers.ParserConfigurationException;

/**
 * Created by zhangshaowen on 17/6/12.
 */

public final class TaskResultFactory {
    private static final String TAG = "Matrix.TaskResultFactory";

    public static final String TASK_RESULT_TYPE_JSON = "json";
    public static final String TASK_RESULT_TYPE_HTML = "html";

    private static Map<String, Class<? extends TaskHtmlResult>> customHtmlResultMap = new HashMap<>();
    private static Map<String, Class<? extends TaskJsonResult>> customJsonResultMap = new HashMap<>();

    public static TaskResult factory(int taskType, String resultType, JobConfig config) throws ParserConfigurationException, IllegalAccessException, InstantiationException, NoSuchMethodException, InvocationTargetException {
        TaskResult result = null;
        JsonObject taskConfig = null;
        JsonArray taskConfigs = config.getOutputConfig();
        if (taskConfigs != null) {
            for (JsonElement element : taskConfigs) {
                JsonObject obj = element.getAsJsonObject();
                if (obj.get("name").getAsString().equals(TaskFactory.TaskOptionName.get(taskType))) {
                    taskConfig = obj;
                    break;
                }
            }
        }
        if (TASK_RESULT_TYPE_JSON.equals(resultType)) {
            result = new TaskJsonResult(taskType, taskConfig);
        } else if (TASK_RESULT_TYPE_HTML.equals(resultType)) {
            result = new TaskHtmlResult(taskType, taskConfig);
        } else if (customHtmlResultMap.containsKey(resultType)) {
            Class class1 = customHtmlResultMap.get(resultType);
            Constructor constructor = class1.getDeclaredConstructor(int.class, JsonObject.class);
            result = (TaskHtmlResult) constructor.newInstance(taskType, taskConfig);
        } else if (customJsonResultMap.containsKey(resultType)) {
            Class class1 = customJsonResultMap.get(resultType);
            Constructor constructor = class1.getDeclaredConstructor(int.class, JsonObject.class);
            result = (TaskJsonResult) constructor.newInstance(taskType, taskConfig);
        } else {
            result = new TaskHtmlResult(taskType, taskConfig);
        }
        return result;
    }

    public static TaskResult transferTaskResult(int taskType, TaskResult source, String destResultType, JobConfig config) {
        TaskResult result = null;
        try {
            if (source instanceof TaskJsonResult) {
                if (TaskResultFactory.customHtmlResultMap.containsKey(destResultType) || destResultType.equals(TaskResultFactory.TASK_RESULT_TYPE_HTML)) {
                    result = TaskResultFactory.factory(taskType, destResultType, config);
                    transferJsonToHtml((TaskJsonResult) source, (TaskHtmlResult) result);
                } else if (TaskResultFactory.customJsonResultMap.containsKey(destResultType)) {
                    result = TaskResultFactory.factory(taskType, destResultType, config);
                    formatJson((TaskJsonResult) source, (TaskJsonResult) result);
                } else {
                    result = source;
                }
            }
        } catch (ParserConfigurationException | InstantiationException |  IllegalAccessException |  NoSuchMethodException | InvocationTargetException e) {
            Log.e(TAG, "transfer task result failed! " + e.getMessage());
            result = null;
        }
        return result;
    }

    private static void transferJsonToHtml(TaskJsonResult source, TaskHtmlResult dest) throws ParserConfigurationException {
        JsonObject jsonObject = (JsonObject) new JsonParser().parse(source.toString());
        dest.format(jsonObject);
    }

    private static void formatJson(TaskJsonResult source, TaskJsonResult dest) {
        dest.format(source.jsonObject);
    }

    public static void addCustomTaskHtmlResult(Map<String, Class<? extends TaskHtmlResult>> customTaskHtmlResult) {
        customHtmlResultMap.putAll(customTaskHtmlResult);
    }

    public static void addCustomTaskJsonResult(Map<String, Class<? extends TaskJsonResult>> customTaskJsonResult) {
        customJsonResultMap.putAll(customTaskJsonResult);
    }

    public static boolean isJsonResult(String resultType) {
        if (resultType.equals(TASK_RESULT_TYPE_JSON)) {
            return true;
        } else if (customJsonResultMap.containsKey(resultType)) {
            return true;
        }
        return false;
    }

    public static boolean isHtmlResult(String resultType) {
        if (resultType.equals(TASK_RESULT_TYPE_HTML)) {
            return true;
        } else if (customHtmlResultMap.containsKey(resultType)) {
            return true;
        }
        return false;
    }


}
