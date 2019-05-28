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

package com.tencent.matrix.plugin;

import android.content.Context;

import com.tencent.matrix.report.Issue;
import com.tencent.matrix.util.MatrixLog;

/**
 * Created by zhangshaowen on 17/5/17.
 */

public class DefaultPluginListener implements PluginListener {
    private static final String TAG = "Matrix.DefaultPluginListener";

    private final Context context;

    public DefaultPluginListener(Context context) {
        this.context = context;
    }

    @Override
    public void onInit(Plugin plugin) {
        MatrixLog.i(TAG, "%s plugin is inited", plugin.getTag());
    }

    @Override
    public void onStart(Plugin plugin) {
        MatrixLog.i(TAG, "%s plugin is started", plugin.getTag());
    }

    @Override
    public void onStop(Plugin plugin) {
        MatrixLog.i(TAG, "%s plugin is stopped", plugin.getTag());
    }

    @Override
    public void onDestroy(Plugin plugin) {
        MatrixLog.i(TAG, "%s plugin is destroyed", plugin.getTag());
    }

    @Override
    public void onReportIssue(Issue issue) {
        MatrixLog.i(TAG, "report issue content: %s", issue == null ? "" : issue);
    }

}
