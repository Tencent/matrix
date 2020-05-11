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

import com.tencent.matrix.AppActiveMatrixDelegate;
import com.tencent.matrix.Matrix;
import com.tencent.matrix.listeners.IAppForeground;
import com.tencent.matrix.trace.R;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.core.UIThreadMonitor;
import com.tencent.matrix.trace.listeners.IDoFrameListener;
import com.tencent.matrix.trace.tracer.FrameTracer;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import java.util.concurrent.Executor;

public class FrameDecorator extends IDoFrameListener implements IAppForeground {
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

    @SuppressLint("ClickableViewAccessibility")
    private FrameDecorator(Context context, final FloatFrameView view) {
        this.view = view;
        AppActiveMatrixDelegate.INSTANCE.addListener(this);
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
    private long[] lastFrames = new long[1];

    private Runnable updateDefaultRunnable = new Runnable() {
        @Override
        public void run() {
            view.fpsView.setText("60.00 FPS");
            view.fpsView.setTextColor(view.getResources().getColor(android.R.color.holo_green_dark));
        }
    };

    @Override
    public void doFrameAsync(String visibleScene, long taskCost, long frameCostMs, int droppedFrames, boolean isContainsFrame) {
        super.doFrameAsync(visibleScene, taskCost, frameCostMs, droppedFrames, isContainsFrame);
        sumFrameCost += (droppedFrames + 1) * UIThreadMonitor.getMonitor().getFrameIntervalNanos() / Constants.TIME_MILLIS_TO_NANO;
        sumFrames += 1;
        long duration = sumFrameCost - lastCost[0];

        long collectFrame = sumFrames - lastFrames[0];
        if (duration >= 200) {
            final float fps = Math.min(60.f, 1000.f * collectFrame / duration);
            updateView(view.fpsView, fps);
            view.chartView.addFps((int) fps);
            lastCost[0] = sumFrameCost;
            lastFrames[0] = sumFrames;
            mainHandler.removeCallbacks(updateDefaultRunnable);
            mainHandler.postDelayed(updateDefaultRunnable, 130);
        }
    }

    private void updateView(final TextView view, final float fps) {
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                view.setText(String.format("%.2f FPS", fps));
                if (fps >= 50) {
                    view.setTextColor(view.getResources().getColor(android.R.color.holo_green_dark));
                } else if (fps >= 30) {
                    view.setTextColor(view.getResources().getColor(android.R.color.holo_orange_dark));
                } else {
                    view.setTextColor(view.getResources().getColor(android.R.color.holo_red_dark));
                }
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
            handler = new Handler(MatrixHandlerThread.getDefaultHandlerThread().getLooper());
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
                    mainHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            instance = new FrameDecorator(context, new FloatFrameView(context));
                            synchronized (lock) {
                                lock.notifyAll();
                            }
                        }
                    });
                    synchronized (lock) {
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
            windowManager.getDefaultDisplay().getMetrics(displayMetrics);
            DisplayMetrics metrics = new DisplayMetrics();
            windowManager.getDefaultDisplay().getMetrics(metrics);
            layoutParam = new WindowManager.LayoutParams();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                layoutParam.type = WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY;
            } else {
                layoutParam.type = WindowManager.LayoutParams.TYPE_PHONE;
            }
            layoutParam.flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                    | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL;
            layoutParam.gravity = Gravity.START | Gravity.TOP;
            layoutParam.x = metrics.widthPixels - view.getLayoutParams().width * 2;
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

    @Override
    public void onForeground(final boolean isForeground) {
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
