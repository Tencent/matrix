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
import android.support.v4.app.Fragment;
import android.view.ViewTreeObserver;

import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.core.FrameBeat;
import com.tencent.matrix.trace.listeners.IDoFrameListener;
import com.tencent.matrix.util.MatrixLog;

import java.util.LinkedList;

/**
 * Created by caichongyang on 2017/5/26.
 *
 **/

public class FrameTracer extends BaseTracer implements ViewTreeObserver.OnDrawListener {
    private static final String TAG = "Matrix.FrameTracer";
    private boolean isDrawing;
    private final LinkedList<IDoFrameListener> mDoFrameListenerList = new LinkedList<>();
    private static final long REFRESH_RATE_MS = (int) (Constants.DEFAULT_DEVICE_REFRESH_RATE * Constants.TIME_MILLIS_TO_NANO);

    public FrameTracer(TracePlugin plugin) {
        super(plugin);
    }

    @Override
    protected String getTag() {
        return null;
    }

    @Override
    public void doFrame(final long lastFrameNanos, final long frameNanos) {
        if (!isDrawing) {
            return;
        }
        isDrawing = false;
        final int droppedCount = (int) ((frameNanos - lastFrameNanos) / REFRESH_RATE_MS);
        for (final IDoFrameListener listener : mDoFrameListenerList) {
            listener.doFrameSync(lastFrameNanos, frameNanos, getScene(), droppedCount);
            if (null != listener.getHandler()) {
                listener.getHandler().post(new AsyncDoFrameTask(listener,
                        lastFrameNanos, frameNanos, getScene(), droppedCount));
            }

        }
    }

    @Override
    public void onChange(final Activity activity, final Fragment fragment) {
        if (null == activity || null == fragment) {
            MatrixLog.e(TAG, "Empty Parameters");
            return;
        }

        super.onChange(activity, fragment);
        MatrixLog.i(TAG, "[onChange] activity:%s", activity.getClass().getName());
        if (null != activity.getWindow() && null != activity.getWindow().getDecorView()) {
            activity.getWindow().getDecorView().post(new Runnable() {
                @Override
                public void run() {
                    activity.getWindow().getDecorView().getViewTreeObserver().removeOnDrawListener(FrameTracer.this);
                    activity.getWindow().getDecorView().getViewTreeObserver().addOnDrawListener(FrameTracer.this);
                }
            });
        }

    }

    @Override
    public void onDraw() {
        isDrawing = true;
    }

    private final static class AsyncDoFrameTask implements Runnable {
        IDoFrameListener listener;
        String scene;
        int droppedCount;
        long lastFrameNanos;
        long frameNanos;

        AsyncDoFrameTask(IDoFrameListener listener, long lastFrameNanos, long frameNanos, String scene, int droppedCount) {
            this.listener = listener;
            this.scene = scene;
            this.droppedCount = droppedCount;
            this.lastFrameNanos = lastFrameNanos;
            this.frameNanos = frameNanos;
        }

        @Override
        public void run() {
            listener.doFrameAsync(lastFrameNanos, frameNanos, scene, droppedCount);
        }
    }

    public void register(IDoFrameListener listener) {
        if (FrameBeat.getInstance().isPause()) {
            FrameBeat.getInstance().resume();
        }
        if (!mDoFrameListenerList.contains(listener)) {
            mDoFrameListenerList.add(listener);
        }
    }

    public void unregister(IDoFrameListener listener) {
        mDoFrameListenerList.remove(listener);
        if (!FrameBeat.getInstance().isPause() && mDoFrameListenerList.isEmpty()) {
            FrameBeat.getInstance().removeListener(this);
        }
    }

}
