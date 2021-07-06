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
import android.os.Looper;

import com.tencent.matrix.plugin.Plugin;
import com.tencent.matrix.plugin.PluginListener;
import com.tencent.matrix.trace.config.SharePluginInfo;
import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.trace.core.AppMethodBeat;
import com.tencent.matrix.trace.core.UIThreadMonitor;
import com.tencent.matrix.trace.tracer.EvilMethodTracer;
import com.tencent.matrix.trace.tracer.FrameTracer;
import com.tencent.matrix.trace.tracer.IdleHandlerLagTracer;
import com.tencent.matrix.trace.tracer.LooperAnrTracer;
import com.tencent.matrix.trace.tracer.SignalAnrTracer;
import com.tencent.matrix.trace.tracer.StartupTracer;
import com.tencent.matrix.trace.tracer.ThreadPriorityTracer;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

/**
 * Created by caichongyang on 2017/5/20.
 */
public class TracePlugin extends Plugin {
    private static final String TAG = "Matrix.TracePlugin";

    private final TraceConfig traceConfig;
    private EvilMethodTracer evilMethodTracer;
    private StartupTracer startupTracer;
    private FrameTracer frameTracer;
    private LooperAnrTracer looperAnrTracer;
    private SignalAnrTracer signalAnrTracer;
    private IdleHandlerLagTracer idleHandlerLagTracer;
    private ThreadPriorityTracer threadPriorityTracer;

    public TracePlugin(TraceConfig config) {
        this.traceConfig = config;
    }

    @Override
    public void init(Application app, PluginListener listener) {
        super.init(app, listener);
        MatrixLog.i(TAG, "trace plugin init, trace config: %s", traceConfig.toString());
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN) {
            MatrixLog.e(TAG, "[FrameBeat] API is low Build.VERSION_CODES.JELLY_BEAN(16), TracePlugin is not supported");
            unSupportPlugin();
            return;
        }

        looperAnrTracer = new LooperAnrTracer(traceConfig);

        frameTracer = new FrameTracer(traceConfig);

        evilMethodTracer = new EvilMethodTracer(traceConfig);

        startupTracer = new StartupTracer(traceConfig);
    }

    @Override
    public void start() {
        super.start();
        if (!isSupported()) {
            MatrixLog.w(TAG, "[start] Plugin is unSupported!");
            return;
        }
        MatrixLog.w(TAG, "start!");
        Runnable runnable = new Runnable() {
            @Override
            public void run() {

                if (!UIThreadMonitor.getMonitor().isInit()) {
                    try {
                        UIThreadMonitor.getMonitor().init(traceConfig);
                    } catch (java.lang.RuntimeException e) {
                        MatrixLog.e(TAG, "[start] RuntimeException:%s", e);
                        return;
                    }
                }

                if (traceConfig.isAppMethodBeatEnable()) {
                    AppMethodBeat.getInstance().onStart();
                } else {
                    AppMethodBeat.getInstance().forceStop();
                }

                UIThreadMonitor.getMonitor().onStart();

                if (traceConfig.isAnrTraceEnable()) {
                    looperAnrTracer.onStartTrace();
                }

                if (traceConfig.isIdleHandlerEnable()) {
                    idleHandlerLagTracer = new IdleHandlerLagTracer(traceConfig);
                    idleHandlerLagTracer.onStartTrace();
                }

                if (traceConfig.isSignalAnrTraceEnable()) {
                    if (!SignalAnrTracer.hasInstance) {
                        signalAnrTracer = new SignalAnrTracer(traceConfig);
                        signalAnrTracer.onStartTrace();
                    }
                }

                if (traceConfig.isMainThreadPriorityTraceEnable()) {
                    threadPriorityTracer = new ThreadPriorityTracer();
                    threadPriorityTracer.onStartTrace();
                }

                if (traceConfig.isFPSEnable()) {
                    frameTracer.onStartTrace();
                }

                if (traceConfig.isEvilMethodTraceEnable()) {
                    evilMethodTracer.onStartTrace();
                }

                if (traceConfig.isStartupEnable()) {
                    startupTracer.onStartTrace();
                }


            }
        };

        if (Thread.currentThread() == Looper.getMainLooper().getThread()) {
            runnable.run();
        } else {
            MatrixLog.w(TAG, "start TracePlugin in Thread[%s] but not in mainThread!", Thread.currentThread().getId());
            MatrixHandlerThread.getDefaultMainHandler().post(runnable);
        }

    }

    @Override
    public void stop() {
        super.stop();
        if (!isSupported()) {
            MatrixLog.w(TAG, "[stop] Plugin is unSupported!");
            return;
        }
        MatrixLog.w(TAG, "stop!");
        Runnable runnable = new Runnable() {
            @Override
            public void run() {

                AppMethodBeat.getInstance().onStop();

                UIThreadMonitor.getMonitor().onStop();

                looperAnrTracer.onCloseTrace();

                frameTracer.onCloseTrace();

                evilMethodTracer.onCloseTrace();

                startupTracer.onCloseTrace();

                if (signalAnrTracer != null) {
                    signalAnrTracer.onCloseTrace();
                }

                if (idleHandlerLagTracer != null) {
                    idleHandlerLagTracer.onCloseTrace();
                }

                if (threadPriorityTracer != null) {
                    threadPriorityTracer.onCloseTrace();
                }

            }
        };

        if (Thread.currentThread() == Looper.getMainLooper().getThread()) {
            runnable.run();
        } else {
            MatrixLog.w(TAG, "stop TracePlugin in Thread[%s] but not in mainThread!", Thread.currentThread().getId());
            MatrixHandlerThread.getDefaultMainHandler().post(runnable);
        }

    }

    @Override
    public void onForeground(boolean isForeground) {
        super.onForeground(isForeground);
        if (!isSupported()) {
            return;
        }

        if (frameTracer != null) {
            frameTracer.onForeground(isForeground);
        }

        if (looperAnrTracer != null) {
            looperAnrTracer.onForeground(isForeground);
        }

        if (evilMethodTracer != null) {
            evilMethodTracer.onForeground(isForeground);
        }

        if (startupTracer != null) {
            startupTracer.onForeground(isForeground);
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
        return frameTracer;
    }

    public AppMethodBeat getAppMethodBeat() {
        return AppMethodBeat.getInstance();
    }

    public LooperAnrTracer getLooperAnrTracer() {
        return looperAnrTracer;
    }

    public EvilMethodTracer getEvilMethodTracer() {
        return evilMethodTracer;
    }

    public StartupTracer getStartupTracer() {
        return startupTracer;
    }

    public UIThreadMonitor getUIThreadMonitor() {
        if (UIThreadMonitor.getMonitor().isInit()) {
            return UIThreadMonitor.getMonitor();
        } else {
            return null;
        }
    }

    public TraceConfig getTraceConfig() {
        return traceConfig;
    }
}
