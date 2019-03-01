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
import android.os.Handler;
import android.os.Looper;

import com.tencent.matrix.plugin.Plugin;
import com.tencent.matrix.plugin.PluginListener;
import com.tencent.matrix.trace.config.SharePluginInfo;
import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.trace.core.ApplicationLifeObserver;
import com.tencent.matrix.trace.core.FrameBeat;
import com.tencent.matrix.trace.tracer.AnrTracer;
import com.tencent.matrix.trace.tracer.EvilMethodTracer;
import com.tencent.matrix.trace.tracer.FPSTracer;
import com.tencent.matrix.trace.tracer.FrameTracer;
import com.tencent.matrix.trace.tracer.StartUpTracer;
import com.tencent.matrix.trace.util.TraceDataUtils;
import com.tencent.matrix.util.MatrixLog;

/**
 * Created by caichongyang on 2017/5/20.
 * <p>The TracePlugin includes two parts,
 * {@link FPSTracer}\{@link EvilMethodTracer}
 * </p>
 */
public class TracePlugin extends Plugin {
    private static final String TAG = "Matrix.TracePlugin";

    private final TraceConfig mTraceConfig;
    private FPSTracer mFPSTracer;
    private EvilMethodTracer mEvilMethodTracer;
    private FrameTracer mFrameTracer;
    private StartUpTracer mStartUpTracer;

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

        ApplicationLifeObserver.init(app);
        mFrameTracer = new FrameTracer(this);
        if (mTraceConfig.isMethodTraceEnable()) {
            mStartUpTracer = new StartUpTracer(this, mTraceConfig);
        }
        if (mTraceConfig.isFPSEnable()) {
            mFPSTracer = new FPSTracer(this, mTraceConfig);
        }

        if (mTraceConfig.isMethodTraceEnable()) {
            mEvilMethodTracer = new EvilMethodTracer(this, mTraceConfig);
        }

//        UIThreadMonitor.getMonitor().onStart();

        TraceDataUtils.testStructuredDataToTree();

    }

    @Override
    public void start() {
        super.start();
        if (!isSupported()) {
            return;
        }

        new Handler(Looper.getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
//                FrameBeat.getMonitor().onCreate();
            }
        });

        if (null != mFPSTracer) {
            mFPSTracer.onCreate();
        }
        if (null != mEvilMethodTracer) {
            mEvilMethodTracer.onCreate();
        }
        if (null != mFrameTracer) {
            mFrameTracer.onCreate();
        }
        if (null != mStartUpTracer) {
            mStartUpTracer.onCreate();
        }

        new AnrTracer(mTraceConfig).onStartTrace();



    }

    @Override
    public void stop() {
        super.stop();
        if (!isSupported()) {
            return;
        }
        FrameBeat.getInstance().onDestroy();
        if (null != mFPSTracer) {
            mFPSTracer.onDestroy();
        }
        if (null != mEvilMethodTracer) {
            mEvilMethodTracer.onDestroy();
        }
        if (null != mFrameTracer) {
            mFrameTracer.onDestroy();
        }
        if (null != mStartUpTracer) {
            mStartUpTracer.onDestroy();
        }
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

    public EvilMethodTracer getEvilMethodTracer() {
        return mEvilMethodTracer;
    }

    public StartUpTracer getStartUpTracer() {
        return mStartUpTracer;
    }

    public FPSTracer getFPSTracer() {
        return mFPSTracer;
    }
}
