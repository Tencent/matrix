package com.tencent.matrix.trace.view;

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

import com.tencent.matrix.AppForegroundDelegate;
import com.tencent.matrix.Matrix;
import com.tencent.matrix.listeners.IAppForeground;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.core.UIThreadMonitor;
import com.tencent.matrix.trace.listeners.IDoFrameListener;
import com.tencent.matrix.trace.tracer.FrameTracer;
import com.tencent.matrix.util.MatrixHandlerThread;

public class FrameDecorator extends IDoFrameListener implements IAppForeground {

    private WindowManager windowManager;
    private WindowManager.LayoutParams layoutParam;
    private boolean isShowing;
    private FloatFrameView view;
    private static Handler mainHandler = new Handler(Looper.getMainLooper());
    private static FrameDecorator instance;
    private static Object lock = new Object();

    private FrameDecorator(Context context, FloatFrameView view) {
        this.view = view;
        AppForegroundDelegate.INSTANCE.addListener(this);
        view.addOnAttachStateChangeListener(new View.OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View v) {
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
            int oddOffsetX = 0;
            int oddOffsetY = 0;

            @Override
            public boolean onTouch(View v, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        downX = event.getX();
                        downY = event.getY();
                        oddOffsetX = layoutParam.x;
                        oddOffsetY = layoutParam.y;
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
                        int newOffsetX = layoutParam.x;
                        int newOffsetY = layoutParam.y;
                        if (Math.abs(newOffsetX - oddOffsetX) <= 20 && Math.abs(newOffsetY - oddOffsetY) <= 20) {
                            v.performClick();
                        }
                        break;
                }
                return true;
            }

        });

    }

    long sumFrameCost;
    long sumFrames;
    Runnable updateDefaultRunnable = new Runnable() {
        @Override
        public void run() {
            view.fpsView.setText("60.00 FPS");
            view.fpsView.setTextColor(view.getResources().getColor(android.R.color.holo_green_dark));
        }
    };

    @Override
    public void doFrameAsync(String focusedActivityName, long frameCost, int droppedFrames) {
        super.doFrameAsync(focusedActivityName, frameCost, droppedFrames);
        sumFrameCost += (droppedFrames + 1) * UIThreadMonitor.getMonitor().getFrameIntervalNanos() / Constants.TIME_MILLIS_TO_NANO;
        sumFrames += 1;
        if (sumFrameCost >= 200) {
            final float fps = Math.min(60.f, 1000.f * sumFrames / sumFrameCost);
            mainHandler.post(new Runnable() {
                @Override
                public void run() {
                    view.fpsView.setText(String.format("%.2f FPS", fps));
                    if (fps >= 50) {
                        view.fpsView.setTextColor(view.getResources().getColor(android.R.color.holo_green_dark));
                    } else if (fps >= 30) {
                        view.fpsView.setTextColor(view.getResources().getColor(android.R.color.holo_orange_dark));
                    } else {
                        view.fpsView.setTextColor(view.getResources().getColor(android.R.color.holo_red_dark));
                    }
                    mainHandler.removeCallbacks(updateDefaultRunnable);
                    mainHandler.postDelayed(updateDefaultRunnable, 120);
                }
            });
            sumFrames = 0;
            sumFrameCost = 0;
        }

    }

    @Override
    public Handler getHandler() {
        return MatrixHandlerThread.getDefaultHandler();
    }

    public static FrameDecorator get(final Context context) {
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
        if (!isShowing) {
            isShowing = true;
            windowManager.addView(view, layoutParam);

        }
    }

    public void dismiss() {
        if (isShowing) {
            isShowing = false;
            windowManager.removeView(view);
        }
    }

    public boolean isShowing() {
        return isShowing;
    }

    @Override
    public void onForeground(final boolean isForeground) {
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
