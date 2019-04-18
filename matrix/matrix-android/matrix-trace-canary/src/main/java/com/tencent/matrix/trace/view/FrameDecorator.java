package com.tencent.matrix.trace.view;

import android.content.Context;
import android.graphics.PixelFormat;
import android.os.Build;
import android.os.Handler;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.listeners.IDoFrameListener;
import com.tencent.matrix.trace.tracer.FrameTracer;
import com.tencent.matrix.util.MatrixHandlerThread;

public class FrameDecorator extends IDoFrameListener {

    private WindowManager windowManager;
    private WindowManager.LayoutParams layoutParam;
    private boolean isShowing;
    private FloatFrameView view;
    private Handler mainHandler;

    private FrameDecorator(Context context, FloatFrameView view) {
        this.mainHandler = new Handler();
        this.view = view;

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
    }

    long sumFrameCost;
    long sumFrames;
    Runnable updateDefaultRunnable = new Runnable() {
        @Override
        public void run() {
            view.fpsView.setText("60.00 FPS");
        }
    };

    @Override
    public void doFrameAsync(String focusedActivityName, long frameCost, int droppedFrames) {
        super.doFrameAsync(focusedActivityName, frameCost, droppedFrames);
        sumFrameCost += frameCost;
        sumFrames += 1;
        if (sumFrameCost >= 100) {
            final float fps = Math.min(60.f, 1000.f * sumFrames / sumFrameCost);
            mainHandler.post(new Runnable() {
                @Override
                public void run() {
                    view.fpsView.setText(String.format("%.2f FPS", fps));
                    view.fpsView.removeCallbacks(updateDefaultRunnable);
                    view.fpsView.postDelayed(updateDefaultRunnable, 100);
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

    public static FrameDecorator create(Context context) {
        return new FrameDecorator(context, new FloatFrameView(context));
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

}
