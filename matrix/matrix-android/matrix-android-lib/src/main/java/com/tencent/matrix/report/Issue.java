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

package com.tencent.matrix.report;

import com.tencent.matrix.plugin.Plugin;

import org.json.JSONObject;

/**
 * Data struct contains the issues
 *
 * Created by zhangshaowen on 2017/8/1.
 */

public class Issue {
    private int        type;
    private String     tag;
    private String     key;
    private JSONObject content;
    private Plugin     plugin;

    public static final String ISSUE_REPORT_TYPE    = "type";
    public static final String ISSUE_REPORT_TAG     = "tag";
    public static final String ISSUE_REPORT_PROCESS = "process";
    public static final String ISSUE_REPORT_TIME = "time";

    public Issue() {
    }

    public Issue(int type) {
        this.type = type;
    }

    public Issue(JSONObject content) {
        this.content = content;
    }

    public JSONObject getContent() {
        return content;
    }

    public void setContent(JSONObject content) {
        this.content = content;
    }


    @Override
    public String toString() {
        String strContent = "";
        if (null != content) strContent = content.toString();
        return String.format("tag[%s]type[%d];key[%s];content[%s]", tag, type, key, strContent);
    }

    public void setKey(String key) {
        this.key = key;
    }

    public void setTag(String tag) {
        this.tag = tag;
    }

    public int getType() {
        return type;
    }

    public String getKey() {
        return key;
    }

    public String getTag() {
        return tag;
    }

    public void setType(int type) {
        this.type = type;
    }

    public Plugin getPlugin() {
        return plugin;
    }

    public void setPlugin(Plugin plugin) {
        this.plugin = plugin;
    }
}
