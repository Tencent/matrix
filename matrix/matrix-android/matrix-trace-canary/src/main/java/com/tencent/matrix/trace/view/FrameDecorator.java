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

package com.tencent.matrix.trace.view;

import android.animation.Animator;
import android.animation.PropertyValuesHolder;
import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.PixelFormat;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.view.animation.AccelerateInterpolator;
import android.widget.TextView;

import androidx.annotation.RequiresApi;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.lifecycle.IStateObserver;
import com.tencent.matrix.lifecycle.owners.ProcessUIResumedStateOwner;
import com.tencent.matrix.trace.R;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.listeners.IActivityFrameListener;
import com.tencent.matrix.trace.tracer.FrameTracer;
import com.tencent.matrix.util.MatrixLog;

import java.util.Arrays;

@RequiresApi(Build.VERSION_CODES.N)
public class FrameDecorator implements IActivityFrameListener {
    private static final String TAG = "Matrix.FrameDecorator";
    private WindowManager windowManager;
    private WindowManager.LayoutParams layoutParam;
    private boolean isShowing;
    private final FloatFrameView view;
    private static final Handler mainHandler = new Handler(Looper.getMainLooper());
    private static FrameDecorator instance;
    private static final Object lock = new Object();
    private View.OnClickListener clickListener;
    private final DisplayMetrics displayMetrics = new DisplayMetrics();
    private boolean isEnable = true;
    private static final int sdkInt = Build.VERSION.SDK_INT;

    private final int bestColor;
    private final int normalColor;
    private final int middleColor;
    private final int highColor;
    private final int frozenColor;
    private int belongColor;

    private IStateObserver mProcessForegroundListener = new IStateObserver() {
        @Override
        public void on() {
            onForeground(true);
        }

        @Override
        public void off() {
            onForeground(false);
        }
    };

    @SuppressLint("ClickableViewAccessibility")
    private FrameDecorator(Context context, final FloatFrameView view) {
        this.view = view;
        this.bestColor = context.getResources().getColor(R.color.level_best_color);
        this.normalColor = context.getResources().getColor(R.color.level_normal_color);
        this.middleColor = context.getResources().getColor(R.color.level_middle_color);
        this.highColor = context.getResources().getColor(R.color.level_high_color);
        this.frozenColor = context.getResources().getColor(R.color.level_frozen_color);
        belongColor = bestColor;

        ProcessUIResumedStateOwner.INSTANCE.observeForever(mProcessForegroundListener);

        view.addOnAttachStateChangeListener(new View.OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View v) {
                MatrixLog.i(TAG, "onViewAttachedToWindow");
                if (Matrix.isInstalled()) {
                    TracePlugin tracePlugin = Matrix.with().getPluginByClass(TracePlugin.class);
                    if (null != tracePlugin) {
                        FrameTracer tracer = tracePlugin.getFrameTracer();
                        tracer.register(FrameDecorator.this);
                    }
                }
            }

            @Override
            public void onViewDetachedFromWindow(View v) {
                MatrixLog.i(TAG, "onViewDetachedFromWindow");
                if (Matrix.isInstalled()) {
                    TracePlugin tracePlugin = Matrix.with().getPluginByClass(TracePlugin.class);
                    if (null != tracePlugin) {
                        FrameTracer tracer = tracePlugin.getFrameTracer();
                        tracer.unregister(FrameDecorator.this);
                    }
                }
            }
        });
        initLayoutParams(context);

        view.setOnTouchListener(new View.OnTouchListener() {
            float downX = 0;
            float downY = 0;
            int downOffsetX = 0;
            int downOffsetY = 0;

            @Override
            public boolean onTouch(final View v, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        downX = event.getX();
                        downY = event.getY();
                        downOffsetX = layoutParam.x;
                        downOffsetY = layoutParam.y;
                        break;
                    case MotionEvent.ACTION_MOVE:
                        float moveX = event.getX();
                        float moveY = event.getY();
                        layoutParam.x += (moveX - downX) / 3;
                        layoutParam.y += (moveY - downY) / 3;
                        if (v != null) {
                            windowManager.updateViewLayout(v, layoutParam);
                        }
                        break;
                    case MotionEvent.ACTION_UP:

                        PropertyValuesHolder holder = PropertyValuesHolder.ofInt("trans", layoutParam.x,
                                layoutParam.x > displayMetrics.widthPixels / 2 ? displayMetrics.widthPixels - view.getWidth() : 0);

                        Animator animator = ValueAnimator.ofPropertyValuesHolder(holder);
                        ((ValueAnimator) animator).addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
                            @Override
                            public void onAnimationUpdate(ValueAnimator animation) {
                                if (!isShowing) {
                                    return;
                                }
                                layoutParam.x = (int) animation.getAnimatedValue("trans");
                                windowManager.updateViewLayout(v, layoutParam);
                            }
                        });
                        animator.setInterpolator(new AccelerateInterpolator());
                        animator.setDuration(180).start();

                        int upOffsetX = layoutParam.x;
                        int upOffsetY = layoutParam.y;
                        if (Math.abs(upOffsetX - downOffsetX) <= 20 && Math.abs(upOffsetY - downOffsetY) <= 20) {
                            if (null != clickListener) {
                                clickListener.onClick(v);
                            }
                        }
                        break;
                }
                return true;
            }

        });
    }

    public void setClickListener(View.OnClickListener clickListener) {
        this.clickListener = clickListener;
    }

    public void setExtraInfo(String info) {
        if (getView() != null) {
            TextView textView = getView().findViewById(R.id.extra_info);
            if (null != textView) {
                textView.setText(info);
            }
        }
    }


    public static FrameDecorator get() {
        return instance;
    }

    public static FrameDecorator getInstance(final Context context) {
        if (instance == null) {
            if (Thread.currentThread() == Looper.getMainLooper().getThread()) {
                instance = new FrameDecorator(context, new FloatFrameView(context));
            } else {
                try {
                    synchronized (lock) {
                        mainHandler.post(new Runnable() {
                            @Override
                            public void run() {
                                instance = new FrameDecorator(context, new FloatFrameView(context));
                                synchronized (lock) {
                                    lock.notifyAll();
                                }
                            }
                        });
                        lock.wait();
                    }
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
        }
        return instance;
    }

    public FloatFrameView getView() {
        return view;
    }


    private void initLayoutParams(Context context) {
        windowManager = (WindowManager) context.getApplicationContext().getSystemService(Context.WINDOW_SERVICE);
        try {
            DisplayMetrics metrics = new DisplayMetrics();
            if (null != windowManager.getDefaultDisplay()) {
                windowManager.getDefaultDisplay().getMetrics(displayMetrics);
                windowManager.getDefaultDisplay().getMetrics(metrics);
            }

            layoutParam = new WindowManager.LayoutParams();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                layoutParam.type = WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY;
            } else {
                layoutParam.type = WindowManager.LayoutParams.TYPE_PHONE;
            }
            layoutParam.flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                    | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL;
            layoutParam.gravity = Gravity.START | Gravity.TOP;
            if (null != view) {
                layoutParam.x = metrics.widthPixels - view.getLayoutParams().width * 2;
            }
            layoutParam.y = 0;
            layoutParam.width = WindowManager.LayoutParams.WRAP_CONTENT;
            layoutParam.height = WindowManager.LayoutParams.WRAP_CONTENT;
            layoutParam.format = PixelFormat.TRANSPARENT;
        } catch (Exception e) {
        }
    }

    public void show() {
        if (!isEnable) {
            return;
        }
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                if (!isShowing) {
                    isShowing = true;
                    windowManager.addView(view, layoutParam);
                }
            }
        });

    }

    public boolean isEnable() {
        return isEnable;
    }

    public void setEnable(boolean enable) {
        isEnable = enable;
    }

    public void dismiss() {
        if (!isEnable) {
            return;
        }
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                if (isShowing) {
                    isShowing = false;
                    windowManager.removeView(view);
                }
            }
        });
    }

    public boolean isShowing() {
        return isShowing;
    }

    private void onForeground(final boolean isForeground) {
        MatrixLog.i(TAG, "[onForeground] isForeground:%s", isForeground);
        if (!isEnable) {
            return;
        }
        if (mainHandler != null) {
            mainHandler.post(new Runnable() {
                @Override
                public void run() {
                    if (isForeground) {
                        show();
                    } else {
                        dismiss();
                    }
                }
            });
        }
    }

    @Override
    public int getSize() {
        return 3;
    }

    @Override
    public String getName() {
        return null;
    }

    @Override
    public boolean skipFirstFrame() {
        return false;
    }

    @SuppressLint("DefaultLocale")
    @Override
    public void onFrameMetricsAvailable(String scene, long[] durations, int[] dropLevel, int[] dropSum, int avgDroppedFrame, float avgRefreshRate) {
        final String unknownDelay = String.format("unknown delay: %.1fms", (double) durations[FrameTracer.FrameDuration.UNKNOWN_DELAY_DURATION.ordinal()] / Constants.TIME_MILLIS_TO_NANO);
        final String inputHandling = String.format("input handling: %.1fms", (double) durations[FrameTracer.FrameDuration.INPUT_HANDLING_DURATION.ordinal()] / Constants.TIME_MILLIS_TO_NANO);
        final String animation = String.format("animation: %.1fms", (double) durations[FrameTracer.FrameDuration.ANIMATION_DURATION.ordinal()] / Constants.TIME_MILLIS_TO_NANO);
        final String layoutMeasure = String.format("layout measure: %.1fms", (double) durations[FrameTracer.FrameDuration.LAYOUT_MEASURE_DURATION.ordinal()] / Constants.TIME_MILLIS_TO_NANO);
        final String draw = String.format("draw: %.1fms", (double) durations[FrameTracer.FrameDuration.DRAW_DURATION.ordinal()] / Constants.TIME_MILLIS_TO_NANO);
        final String sync = String.format("sync: %.1fms", (double) durations[FrameTracer.FrameDuration.SYNC_DURATION.ordinal()] / Constants.TIME_MILLIS_TO_NANO);
        final String commandIssue = String.format("command issue: %.1fms", (double) durations[FrameTracer.FrameDuration.COMMAND_ISSUE_DURATION.ordinal()] / Constants.TIME_MILLIS_TO_NANO);
        final String swapBuffers = String.format("swap buffers: %.1fms", (double) durations[FrameTracer.FrameDuration.SWAP_BUFFERS_DURATION.ordinal()] / Constants.TIME_MILLIS_TO_NANO);
        final String gpu = String.format("gpu: %.1fms", (double) durations[FrameTracer.FrameDuration.GPU_DURATION.ordinal()] / Constants.TIME_MILLIS_TO_NANO);
        final String total = String.format("total: %.1fms", (double) durations[FrameTracer.FrameDuration.TOTAL_DURATION.ordinal()] / Constants.TIME_MILLIS_TO_NANO);
        final float fps = Math.min(avgRefreshRate, Constants.TIME_SECOND_TO_NANO / durations[FrameTracer.FrameDuration.TOTAL_DURATION.ordinal()]);

        if (avgDroppedFrame >= Constants.DEFAULT_DROPPED_FROZEN) {
            belongColor = frozenColor;
        } else if (avgDroppedFrame >= Constants.DEFAULT_DROPPED_HIGH) {
            belongColor = highColor;
        } else if (avgDroppedFrame >= Constants.DEFAULT_DROPPED_MIDDLE) {
            belongColor = middleColor;
        } else if (avgDroppedFrame >= Constants.DEFAULT_DROPPED_NORMAL) {
            belongColor = normalColor;
        } else {
            belongColor = bestColor;
        }

        final int[] level = Arrays.copyOf(dropLevel, dropLevel.length);
        final int[] sum = Arrays.copyOf(dropSum, dropSum.length);

        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                view.chartView.addFps((int) fps, belongColor);
                view.fpsView.setText(String.format("%.2f FPS", fps));
                view.fpsView.setTextColor(belongColor);

                view.unknownDelayDurationView.setText(unknownDelay);
                view.inputHandlingDurationView.setText(inputHandling);
                view.animationDurationView.setText(animation);
                view.layoutMeasureDurationView.setText(layoutMeasure);
                view.drawDurationView.setText(draw);
                view.syncDurationView.setText(sync);
                view.commandIssueDurationView.setText(commandIssue);
                view.swapBuffersDurationView.setText(swapBuffers);
                if (sdkInt >= Build.VERSION_CODES.S) {
                    view.gpuDurationView.setText(gpu);
                } else {
                    view.gpuDurationView.setText("gpu: unusable");
                }
                view.totalDurationView.setText(total);

                view.sumNormalView.setText(String.valueOf(sum[FrameTracer.DropStatus.DROPPED_NORMAL.ordinal()]));
                view.sumMiddleView.setText(String.valueOf(sum[FrameTracer.DropStatus.DROPPED_MIDDLE.ordinal()]));
                view.sumHighView.setText(String.valueOf(sum[FrameTracer.DropStatus.DROPPED_HIGH.ordinal()]));
                view.sumFrozenView.setText(String.valueOf(sum[FrameTracer.DropStatus.DROPPED_FROZEN.ordinal()]));
                view.levelNormalView.setText(String.valueOf(level[FrameTracer.DropStatus.DROPPED_NORMAL.ordinal()]));
                view.levelMiddleView.setText(String.valueOf(level[FrameTracer.DropStatus.DROPPED_MIDDLE.ordinal()]));
                view.levelHighView.setText(String.valueOf(level[FrameTracer.DropStatus.DROPPED_HIGH.ordinal()]));
                view.levelFrozenView.setText(String.valueOf(level[FrameTracer.DropStatus.DROPPED_FROZEN.ordinal()]));
            }
        });
    }
}
