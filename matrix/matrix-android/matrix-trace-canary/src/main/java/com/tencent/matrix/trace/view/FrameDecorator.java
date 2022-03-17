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

import com.tencent.matrix.Matrix;
import com.tencent.matrix.lifecycle.IStateObserver;
import com.tencent.matrix.lifecycle.owners.ProcessUIResumedStateOwner;
import com.tencent.matrix.trace.R;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.core.UIThreadMonitor;
import com.tencent.matrix.trace.listeners.IDoFrameListener;
import com.tencent.matrix.trace.tracer.FrameTracer;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import java.util.Objects;
import java.util.concurrent.Executor;

public class FrameDecorator extends IDoFrameListener {
    private static final String TAG = "Matrix.FrameDecorator";
    private WindowManager windowManager;
    private WindowManager.LayoutParams layoutParam;
    private boolean isShowing;
    private FloatFrameView view;
    private static Handler mainHandler = new Handler(Looper.getMainLooper());
    private Handler handler;
    private static FrameDecorator instance;
    private static final Object lock = new Object();
    private View.OnClickListener clickListener;
    private DisplayMetrics displayMetrics = new DisplayMetrics();
    private boolean isEnable = true;
    private float frameIntervalMs;
    private float maxFps;


    private int bestColor;
    private int normalColor;
    private int middleColor;
    private int highColor;
    private int frozenColor;

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
        this.frameIntervalMs = 1f * UIThreadMonitor.getMonitor().getFrameIntervalNanos() / Constants.TIME_MILLIS_TO_NANO;
        this.maxFps = Math.round(1000f / frameIntervalMs);
        this.view = view;
        view.fpsView.setText(String.format("%.2f FPS", maxFps));
        this.bestColor = context.getResources().getColor(R.color.level_best_color);
        this.normalColor = context.getResources().getColor(R.color.level_normal_color);
        this.middleColor = context.getResources().getColor(R.color.level_middle_color);
        this.highColor = context.getResources().getColor(R.color.level_high_color);
        this.frozenColor = context.getResources().getColor(R.color.level_frozen_color);

        ProcessUIResumedStateOwner.INSTANCE.observeForever(mProcessForegroundListener);

        view.addOnAttachStateChangeListener(new View.OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View v) {
                MatrixLog.i(TAG, "onViewAttachedToWindow");
                if (Matrix.isInstalled()) {
                    TracePlugin tracePlugin = Matrix.with().getPluginByClass(TracePlugin.class);
                    if (null != tracePlugin) {
                        FrameTracer tracer = tracePlugin.getFrameTracer();
                        tracer.addListener(FrameDecorator.this);
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
                        tracer.removeListener(FrameDecorator.this);
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
                                int value = (int) animation.getAnimatedValue("trans");
                                layoutParam.x = value;
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


    private long sumFrameCost;
    private long[] lastCost = new long[1];
    private long sumFrames;
    private int belongColor = bestColor;
    private long[] lastFrames = new long[1];
    private int[] dropLevel = new int[FrameTracer.DropStatus.values().length];
    private int[] sumDropLevel = new int[FrameTracer.DropStatus.values().length];
    private String lastVisibleScene = "default";

    private Runnable updateDefaultRunnable = new Runnable() {
        @Override
        public void run() {
            view.fpsView.setText(String.format("%.2f FPS", maxFps));
            view.fpsView.setTextColor(view.getResources().getColor(R.color.level_best_color));
        }
    };


    @Override
    public void doFrameAsync(String focusedActivity, long startNs, long endNs, int dropFrame, boolean isVsyncFrame, long intendedFrameTimeNs, long inputCostNs, long animationCostNs, long traversalCostNs) {
        super.doFrameAsync(focusedActivity, startNs, endNs, dropFrame, isVsyncFrame, intendedFrameTimeNs, inputCostNs, animationCostNs, traversalCostNs);

        if (!Objects.equals(focusedActivity, lastVisibleScene)) {
            dropLevel = new int[FrameTracer.DropStatus.values().length];
            lastVisibleScene = focusedActivity;
            lastCost[0] = 0;
            lastFrames[0] = 0;
        }

        sumFrameCost += (dropFrame + 1) * frameIntervalMs;
        sumFrames += 1;
        float duration = sumFrameCost - lastCost[0];

        if (dropFrame >= Constants.DEFAULT_DROPPED_FROZEN) {
            dropLevel[FrameTracer.DropStatus.DROPPED_FROZEN.index]++;
            sumDropLevel[FrameTracer.DropStatus.DROPPED_FROZEN.index]++;
            belongColor = frozenColor;
        } else if (dropFrame >= Constants.DEFAULT_DROPPED_HIGH) {
            dropLevel[FrameTracer.DropStatus.DROPPED_HIGH.index]++;
            sumDropLevel[FrameTracer.DropStatus.DROPPED_HIGH.index]++;
            if (belongColor != frozenColor) {
                belongColor = highColor;
            }
        } else if (dropFrame >= Constants.DEFAULT_DROPPED_MIDDLE) {
            dropLevel[FrameTracer.DropStatus.DROPPED_MIDDLE.index]++;
            sumDropLevel[FrameTracer.DropStatus.DROPPED_MIDDLE.index]++;
            if (belongColor != frozenColor && belongColor != highColor) {
                belongColor = middleColor;
            }
        } else if (dropFrame >= Constants.DEFAULT_DROPPED_NORMAL) {
            dropLevel[FrameTracer.DropStatus.DROPPED_NORMAL.index]++;
            sumDropLevel[FrameTracer.DropStatus.DROPPED_NORMAL.index]++;
            if (belongColor != frozenColor && belongColor != highColor && belongColor != middleColor) {
                belongColor = normalColor;
            }
        } else {
            dropLevel[FrameTracer.DropStatus.DROPPED_BEST.index]++;
            sumDropLevel[FrameTracer.DropStatus.DROPPED_BEST.index]++;
            if (belongColor != frozenColor && belongColor != highColor && belongColor != middleColor && belongColor != normalColor) {
                belongColor = bestColor;
            }
        }


        long collectFrame = sumFrames - lastFrames[0];
        if (duration >= 200) {
            final float fps = Math.min(maxFps, 1000.f * collectFrame / duration);
            updateView(view, fps, belongColor,
                    dropLevel[FrameTracer.DropStatus.DROPPED_NORMAL.index],
                    dropLevel[FrameTracer.DropStatus.DROPPED_MIDDLE.index],
                    dropLevel[FrameTracer.DropStatus.DROPPED_HIGH.index],
                    dropLevel[FrameTracer.DropStatus.DROPPED_FROZEN.index],
                    sumDropLevel[FrameTracer.DropStatus.DROPPED_NORMAL.index],
                    sumDropLevel[FrameTracer.DropStatus.DROPPED_MIDDLE.index],
                    sumDropLevel[FrameTracer.DropStatus.DROPPED_HIGH.index],
                    sumDropLevel[FrameTracer.DropStatus.DROPPED_FROZEN.index]);
            belongColor = bestColor;
            lastCost[0] = sumFrameCost;
            lastFrames[0] = sumFrames;
            mainHandler.removeCallbacks(updateDefaultRunnable);
            mainHandler.postDelayed(updateDefaultRunnable, 250);
        }
    }

    private void updateView(final FloatFrameView view, final float fps, final int belongColor,
                            final int normal, final int middle, final int high, final int frozen,
                            final int sumNormal, final int sumMiddle, final int sumHigh, final int sumFrozen) {
        int all = normal + middle + high + frozen;
        float frozenValue = all <= 0 ? 0 : 1.f * frozen / all * 60;
        float highValue = all <= 0 ? 0 : 1.f * high / all * 25;
        float middleValue = all <= 0 ? 0 : 1.f * middle / all * 14;
        float normaValue = all <= 0 ? 0 : 1.f * normal / all * 1;
        float qiWang = frozenValue + highValue + middleValue + normaValue;

        int sumAll = sumNormal + sumMiddle + sumHigh + sumFrozen;
        float sumFrozenValue = sumAll <= 0 ? 0 : 1.f * sumFrozen / sumAll * 60;
        float sumHighValue = sumAll <= 0 ? 0 : 1.f * sumHigh / sumAll * 25;
        float sumMiddleValue = sumAll <= 0 ? 0 : 1.f * sumMiddle / sumAll * 14;
        float sumNormaValue = sumAll <= 0 ? 0 : 1.f * sumNormal / sumAll * 1;
        float sumQiWang = sumFrozenValue + sumHighValue + sumMiddleValue + sumNormaValue;

        final String radioFrozen = String.format("%.1f", frozenValue);
        final String radioHigh = String.format("%.1f", highValue);
        final String radioMiddle = String.format("%.1f", middleValue);
        final String radioNormal = String.format("%.1f", normaValue);
        final String qiWangStr = String.format("current: %.1f", qiWang);

        final String sumRadioFrozen = String.format("%.1f", sumFrozenValue);
        final String sumRadioHigh = String.format("%.1f", sumHighValue);
        final String sumRadioMiddle = String.format("%.1f", sumMiddleValue);
        final String sumRadioNormal = String.format("%.1f", sumNormaValue);
        final String sumQiWangStr = String.format("sum: %.1f", sumQiWang);

        final String fpsStr = String.format("%.2f FPS", fps);

        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                view.chartView.addFps((int) fps, belongColor);
                view.fpsView.setText(fpsStr);
                view.fpsView.setTextColor(belongColor);

                view.qiWangView.setText(qiWangStr);
                view.levelFrozenView.setText(radioFrozen);
                view.levelHighView.setText(radioHigh);
                view.levelMiddleView.setText(radioMiddle);
                view.levelNormalView.setText(radioNormal);

                view.sumQiWangView.setText(sumQiWangStr);
                view.sumLevelFrozenView.setText(sumRadioFrozen);
                view.sumLevelHighView.setText(sumRadioHigh);
                view.sumLevelMiddleView.setText(sumRadioMiddle);
                view.sumLevelNormalView.setText(sumRadioNormal);

            }
        });
    }

    @Override
    public Executor getExecutor() {
        return executor;
    }

    private Executor executor = new Executor() {
        @Override
        public void execute(Runnable command) {
            getHandler().post(command);
        }
    };

    private Handler getHandler() {
        if (handler == null || !handler.getLooper().getThread().isAlive()) {
            if (null != MatrixHandlerThread.getDefaultHandlerThread()) {
                handler = new Handler(MatrixHandlerThread.getDefaultHandlerThread().getLooper());
            }
        }
        return handler;
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

}
