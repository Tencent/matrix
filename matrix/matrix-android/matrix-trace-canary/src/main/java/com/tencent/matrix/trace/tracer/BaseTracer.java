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

package com.tencent.matrix.trace.tracer;

import android.app.Activity;
import android.os.Process;
import android.support.v4.app.Fragment;

import com.tencent.matrix.report.Issue;
import com.tencent.matrix.report.IssuePublisher;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.trace.core.ApplicationLifeObserver;
import com.tencent.matrix.trace.core.FrameBeat;
import com.tencent.matrix.trace.core.OldMethodBeat;
import com.tencent.matrix.trace.hacker.ActivityThreadHacker;
import com.tencent.matrix.trace.listeners.IFrameBeatListener;
import com.tencent.matrix.trace.listeners.IMethodBeatListener;
import com.tencent.matrix.util.MatrixLog;

import org.json.JSONObject;

import java.util.HashMap;

/**
 * Created by caichongyang on 2017/6/5.
 * <p>
 * The tracer base class.
 * </p>
 */

public abstract class BaseTracer extends IssuePublisher implements ApplicationLifeObserver.IObserver, IFrameBeatListener, IMethodBeatListener {

    private static final String TAG = "Matrix.BaseTracer";
    private final TracePlugin mPlugin;
    private boolean isBackground = true;
    private String mScene;
    private boolean isCreated = false;
    private static final OldMethodBeat S_OLD_METHOD_BEAT = new OldMethodBeat();
    private static final HashMap<Class<BaseTracer>, BaseTracer> sTracerMap = new HashMap<>();

    BaseTracer(TracePlugin plugin) {
        super(plugin);
        this.mPlugin = plugin;
        sTracerMap.put((Class<BaseTracer>) this.getClass(), this);
    }

    public <T extends BaseTracer> T getTracer(Class<T> clazz) {
        return (T) sTracerMap.get(clazz);
    }

    public TracePlugin getPlugin() {
        return mPlugin;
    }

    public static OldMethodBeat getMethodBeat() {
        return S_OLD_METHOD_BEAT;
    }

    protected boolean isBackground() {
        return isBackground;
    }

    protected boolean isEnterAnimationComplete() {
        return ActivityThreadHacker.isEnterAnimationComplete;
    }

    protected String getScene() {
        return mScene;
    }

    protected abstract String getTag();

    @Override
    public void onChange(final Activity activity, final Fragment fragment) {
        this.mScene = TraceConfig.getSceneForString(activity, fragment);
    }

    @Override
    public void doFrame(long lastFrameNanos, long frameNanos) {

    }

    @Override
    public void cancelFrame() {

    }

    @Override
    public void onFront(Activity activity) {
        isBackground = false;
    }

    @Override
    public void onBackground(Activity activity) {
        isBackground = true;
    }

    @Override
    public void onActivityCreated(Activity activity) {
    }

    @Override
    public void onActivityPause(Activity activity) {

    }

    @Override
    public void onActivityStarted(Activity activity) {

    }

    @Override
    public void onActivityResume(Activity activity) {

    }

    @Override
    public void onApplicationCreated(long startTime, long endTime) {

    }

    protected boolean isEnableMethodBeat() {
        return false;
    }

    public void onCreate() {
        MatrixLog.i(TAG, "[onCreate] name:%s process:%s", this.getClass().getCanonicalName(), Process.myPid());
        if (isEnableMethodBeat()) {
            if (!getMethodBeat().isHasListeners()) {
                getMethodBeat().onCreate();
            }
            getMethodBeat().registerListener(this);
        }
        ApplicationLifeObserver.getInstance().register(this);
        FrameBeat.getInstance().addListener(this);
        isCreated = true;
    }

    public void onDestroy() {
        MatrixLog.i(TAG, "[onDestroy] name:%s  process:%s", this.getClass().getCanonicalName(), Process.myPid());
        if (isEnableMethodBeat()) {
            getMethodBeat().unregisterListener(this);
            if (!getMethodBeat().isHasListeners()) {
                getMethodBeat().onDestroy();
            }
        }
        ApplicationLifeObserver.getInstance().unregister(this);
        FrameBeat.getInstance().removeListener(this);
        isCreated = false;
    }

    public boolean isCreated() {
        return isCreated;
    }

    protected void sendReport(JSONObject jsonObject) {
        Issue issue = new Issue();
        issue.setTag(getTag());
        issue.setContent(jsonObject);
        mPlugin.onDetectIssue(issue);
    }

    protected void sendReport(JSONObject jsonObject, String tag) {
        Issue issue = new Issue();
        issue.setTag(tag);
        issue.setContent(jsonObject);
        mPlugin.onDetectIssue(issue);
    }

    @Override
    public void pushFullBuffer(int start, int end, long[] buffer) {

    }

    @Override
    public void onActivityEntered(Activity activity, boolean isFocus, int nowIndex, long[] buffer) {

    }
}
