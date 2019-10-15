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

import android.app.Application;

import com.tencent.matrix.AppActiveMatrixDelegate;
import com.tencent.matrix.listeners.IAppForeground;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.report.IssuePublisher;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by zhangshaowen on 17/5/17.
 */

public abstract class Plugin implements IPlugin, IssuePublisher.OnIssueDetectListener, IAppForeground {
    private static final String TAG = "Matrix.Plugin";

    public static final int PLUGIN_CREATE = 0x00;
    public static final int PLUGIN_INITED = 0x01;
    public static final int PLUGIN_STARTED = 0x02;
    public static final int PLUGIN_STOPPED = 0x04;
    public static final int PLUGIN_DESTROYED = 0x08;

    private PluginListener pluginListener;
    private Application application;

    private boolean isSupported = true;

    private int status = PLUGIN_CREATE;

    @Override
    public void init(Application app, PluginListener listener) {
        if (application != null || pluginListener != null) {
            throw new RuntimeException("plugin duplicate init, application or plugin listener is not null");
        }
        status = PLUGIN_INITED;
        this.application = app;
        this.pluginListener = listener;
        AppActiveMatrixDelegate.INSTANCE.addListener(this);
    }

    @Override
    public void onDetectIssue(Issue issue) {
        if (issue.getTag() == null) {
            // set default tag
            issue.setTag(getTag());
        }
        issue.setPlugin(this);
        JSONObject content = issue.getContent();
        // add tag and type for default
        try {
            if (issue.getTag() != null) {
                content.put(Issue.ISSUE_REPORT_TAG, issue.getTag());
            }
            if (issue.getType() != 0) {
                content.put(Issue.ISSUE_REPORT_TYPE, issue.getType());
            }
            content.put(Issue.ISSUE_REPORT_PROCESS, MatrixUtil.getProcessName(application));
            content.put(Issue.ISSUE_REPORT_TIME, System.currentTimeMillis());

        } catch (JSONException e) {
            MatrixLog.e(TAG, "json error", e);
        }

        //MatrixLog.e(TAG, "detect issue:%s", issue);
        pluginListener.onReportIssue(issue);
    }

    @Override
    public Application getApplication() {
        return application;
    }

    @Override
    public void start() {
        if (isPluginDestroyed()) {
            throw new RuntimeException("plugin start, but plugin has been already destroyed");
        }

        if (isPluginStarted()) {
            throw new RuntimeException("plugin start, but plugin has been already started");
        }

        status = PLUGIN_STARTED;

        if (pluginListener == null) {
            throw new RuntimeException("plugin start, plugin listener is null");
        }
        pluginListener.onStart(this);
    }

    @Override
    public void stop() {
        if (isPluginDestroyed()) {
            throw new RuntimeException("plugin stop, but plugin has been already destroyed");
        }

        if (!isPluginStarted()) {
            throw new RuntimeException("plugin stop, but plugin is never started");
        }

        status = PLUGIN_STOPPED;

        if (pluginListener == null) {
            throw new RuntimeException("plugin stop, plugin listener is null");
        }
        pluginListener.onStop(this);
    }

    @Override
    public void destroy() {
        // stop first
        if (isPluginStarted()) {
            stop();
        }
        if (isPluginDestroyed()) {
            throw new RuntimeException("plugin destroy, but plugin has been already destroyed");
        }
        status = PLUGIN_DESTROYED;

        if (pluginListener == null) {
            throw new RuntimeException("plugin destroy, plugin listener is null");
        }
        pluginListener.onDestroy(this);
    }

    @Override
    public String getTag() {
        return getClass().getName();
    }

    @Override
    public void onForeground(boolean isForeground) {

    }

    public boolean isForeground() {
        return AppActiveMatrixDelegate.INSTANCE.isAppForeground();
    }


    public int getStatus() {
        return status;
    }

    public boolean isPluginStarted() {
        return (status == PLUGIN_STARTED);
    }

    public boolean isPluginStopped() {
        return (status == PLUGIN_STOPPED);
    }

    public boolean isPluginDestroyed() {
        return (status == PLUGIN_DESTROYED);
    }

    public boolean isSupported() {
        return isSupported;
    }

    public void unSupportPlugin() {
        isSupported = false;
    }

    public JSONObject getJsonInfo() {
        return new JSONObject();
    }

}
