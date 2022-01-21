/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

package com.tencent.matrix.trace.tracer;

import androidx.annotation.Keep;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.config.SharePluginInfo;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.util.Utils;
import com.tencent.matrix.util.DeviceUtil;
import com.tencent.matrix.util.MatrixLog;

import org.json.JSONObject;

public class ThreadPriorityTracer extends Tracer {

    private static final String TAG = "ThreadPriorityTracer";
    private static MainThreadPriorityModifiedListener sMainThreadPriorityModifiedListener;

    static {
        System.loadLibrary("trace-canary");
    }

    @Override
    protected void onAlive() {
        super.onAlive();
        nativeInitMainThreadPriorityDetective();
    }

    @Override
    protected void onDead() {
        super.onDead();
    }

    public void setMainThreadPriorityModifiedListener(MainThreadPriorityModifiedListener mainThreadPriorityModifiedListener) {
        this.sMainThreadPriorityModifiedListener = mainThreadPriorityModifiedListener;
    }

    private static native void nativeInitMainThreadPriorityDetective();

    @Keep
    private static void onMainThreadPriorityModified(int priorityBefore, int priorityAfter) {
        if (sMainThreadPriorityModifiedListener != null) {
            sMainThreadPriorityModifiedListener.onMainThreadPriorityModified(priorityBefore, priorityAfter);
            return;
        }
        try {
            TracePlugin plugin = Matrix.with().getPluginByClass(TracePlugin.class);
            if (null == plugin) {
                return;
            }

            String stackTrace = Utils.getMainThreadJavaStackTrace();

            JSONObject jsonObject = new JSONObject();
            jsonObject = DeviceUtil.getDeviceInfo(jsonObject, Matrix.with().getApplication());
            jsonObject.put(SharePluginInfo.ISSUE_STACK_TYPE, Constants.Type.PRIORITY_MODIFIED);
            jsonObject.put(SharePluginInfo.ISSUE_THREAD_STACK, stackTrace);
            jsonObject.put(SharePluginInfo.ISSUE_PROCESS_PRIORITY, priorityAfter);

            Issue issue = new Issue();
            issue.setTag(SharePluginInfo.TAG_PLUGIN_EVIL_METHOD);
            issue.setContent(jsonObject);
            plugin.onDetectIssue(issue);
            MatrixLog.e(TAG, "happens MainThreadPriorityModified : %s ", jsonObject.toString());
        } catch (Throwable t) {
            MatrixLog.e(TAG, "onMainThreadPriorityModified error: %s", t.getMessage());
        }
    }

    @Keep
    private static void onMainThreadTimerSlackModified(long timerSlack) {
        try {

            if (sMainThreadPriorityModifiedListener != null) {
                sMainThreadPriorityModifiedListener.onMainThreadTimerSlackModified(timerSlack);
                return;
            }

            TracePlugin plugin = Matrix.with().getPluginByClass(TracePlugin.class);
            if (null == plugin) {
                return;
            }

            String stackTrace = Utils.getMainThreadJavaStackTrace();

            JSONObject jsonObject = new JSONObject();
            jsonObject = DeviceUtil.getDeviceInfo(jsonObject, Matrix.with().getApplication());
            jsonObject.put(SharePluginInfo.ISSUE_STACK_TYPE, Constants.Type.TIMERSLACK_MODIFIED);
            jsonObject.put(SharePluginInfo.ISSUE_THREAD_STACK, stackTrace);
            jsonObject.put(SharePluginInfo.ISSUE_PROCESS_TIMER_SLACK, timerSlack);

            Issue issue = new Issue();
            issue.setTag(SharePluginInfo.TAG_PLUGIN_EVIL_METHOD);
            issue.setContent(jsonObject);
            plugin.onDetectIssue(issue);
            MatrixLog.e(TAG, "happens MainThreadPriorityModified : %s ", jsonObject.toString());
        } catch (Throwable t) {
            MatrixLog.e(TAG, "onMainThreadPriorityModified error: %s", t.getMessage());
        }

    }

    public interface MainThreadPriorityModifiedListener {
        void onMainThreadPriorityModified(int priorityBefore, int priorityAfter);

        void onMainThreadTimerSlackModified(long timerSlack);
    }
}
