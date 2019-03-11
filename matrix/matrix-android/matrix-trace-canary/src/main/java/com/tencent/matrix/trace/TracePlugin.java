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

package com.tencent.matrix.trace;

import android.app.Application;
import android.os.Build;

import com.tencent.matrix.plugin.Plugin;
import com.tencent.matrix.plugin.PluginListener;
import com.tencent.matrix.trace.config.SharePluginInfo;
import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.trace.core.AppMethodBeat;
import com.tencent.matrix.trace.tracer.AnrTracer;
import com.tencent.matrix.trace.tracer.EvilMethodTracer;
import com.tencent.matrix.trace.tracer.FrameTracer;
import com.tencent.matrix.trace.tracer.StartupTracer;
import com.tencent.matrix.util.MatrixLog;

/**
 * Created by caichongyang on 2017/5/20.
 */
public class TracePlugin extends Plugin {
    private static final String TAG = "Matrix.TracePlugin";

    private final TraceConfig mTraceConfig;
    private EvilMethodTracer mEvilMethodTracer;
    private StartupTracer mStartupTracer;
    private FrameTracer mFrameTracer;
    private AnrTracer mAnrTracer;

    public TracePlugin(TraceConfig config) {
        this.mTraceConfig = config;
    }

    @Override
    public void init(Application app, PluginListener listener) {
        super.init(app, listener);
        MatrixLog.i(TAG, "trace plugin init, trace config: %s", mTraceConfig.toString());
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN) {
            MatrixLog.e(TAG, "[FrameBeat] API is low Build.VERSION_CODES.JELLY_BEAN(16), TracePlugin is not supported");
            unSupportPlugin();
            return;
        }

        mStartupTracer = new StartupTracer(mTraceConfig);

        mFrameTracer = new FrameTracer(mTraceConfig);

        mAnrTracer = new AnrTracer(mTraceConfig);

        mEvilMethodTracer = new EvilMethodTracer(mTraceConfig);

    }

    @Override
    public void start() {
        super.start();
        if (!isSupported()) {
            return;
        }

        AppMethodBeat.getInstance().onStart();

        mAnrTracer.onStartTrace();

        mFrameTracer.onStartTrace();

        mEvilMethodTracer.onStartTrace();

        mStartupTracer.onStartTrace();
    }

    @Override
    public void stop() {
        super.stop();
        if (!isSupported()) {
            return;
        }

        AppMethodBeat.getInstance().onStop();

        mAnrTracer.onCloseTrace();

        mFrameTracer.onCloseTrace();

        mEvilMethodTracer.onCloseTrace();

        mStartupTracer.onCloseTrace();

    }

    @Override
    public void destroy() {
        super.destroy();
    }

    @Override
    public String getTag() {
        return SharePluginInfo.TAG_PLUGIN;
    }

    public FrameTracer getFrameTracer() {
        return mFrameTracer;
    }

}
