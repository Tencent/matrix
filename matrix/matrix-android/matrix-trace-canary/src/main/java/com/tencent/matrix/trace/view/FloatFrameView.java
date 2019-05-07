package com.tencent.matrix.trace.view;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.DashPathEffect;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.tencent.matrix.trace.R;

import java.util.LinkedList;

public class FloatFrameView extends LinearLayout {

    public TextView fpsView;
    public TextView fpsView5;
    public TextView fpsView10;

    public FloatFrameView(Context context) {
        super(context);
        initView(context);
    }

    public FloatFrameView(Context context, AttributeSet attrs) {
        super(context, attrs);
        initView(context);
    }


    private void initView(Context context) {
        setLayoutParams(new ViewGroup.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));
        LayoutInflater.from(context).inflate(R.layout.float_frame_view, this);
        fpsView = findViewById(R.id.fps_view);
        fpsView5 = findViewById(R.id.fps_view_5);
        fpsView10 = findViewById(R.id.fps_view_10);
    }


    public static class LineChartView extends View {

        private final Paint paint;
        private final Paint tipPaint;
        private final static int LINE_COUNT = 100;
        private final LinkedList<LineInfo> lines;
        float linePadding;
        float lineStrokeWidth;
        private float[] topLine = new float[4];
        private float[] middleLine = new float[4];
        private int greenColor = getContext().getResources().getColor(android.R.color.holo_green_dark);
        private int orangeColor = getContext().getResources().getColor(android.R.color.holo_orange_dark);
        private int redColor = getContext().getResources().getColor(android.R.color.holo_red_dark);


        public LineChartView(Context context, AttributeSet attrs, int defStyleAttr) {
            super(context, attrs, defStyleAttr);
            paint = new Paint(Paint.ANTI_ALIAS_FLAG);
            tipPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
            tipPaint.setColor(getContext().getResources().getColor(android.R.color.darker_gray));
            tipPaint.setStyle(Paint.Style.STROKE);
            tipPaint.setStrokeWidth(3);
            tipPaint.setPathEffect(new DashPathEffect(new float[]{1, 1}, 0));
            lines = new LinkedList<>();
        }

        public LineChartView(Context context, AttributeSet attrs) {
            this(context, attrs, 0);
        }

        @Override
        protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
            super.onLayout(changed, left, top, right, bottom);
            if (changed) {
                lineStrokeWidth = getMeasuredHeight() / (LINE_COUNT * 2);
                paint.setStrokeWidth(lineStrokeWidth);
                linePadding = lineStrokeWidth * 2;

                float width = getMeasuredWidth();
                float height = getMeasuredHeight();

                topLine[0] = 10 * width / 60; // startX
                topLine[1] = height;// startY
                topLine[2] = topLine[0];// endX
                topLine[3] = 0;// endY

                middleLine[0] = 30 * width / 60; // startX
                middleLine[1] = height;// startY
                middleLine[2] = middleLine[0];// endX
                middleLine[3] = 0;// endY

                for (int i = 0; i < 30; i++) {
                    addFps(60);
                }
                for (int i = 0; i < 30; i++) {
                    addFps(45);
                }
                for (int i = 0; i < 40; i++) {
                    addFps(15);
                }
            }
        }

        public void addFps(int fps) {
            LineInfo linePoint = new LineInfo(fps, 99 - lines.size());
            if (lines.size() >= 100) {
                lines.removeLast();
            }
            lines.addFirst(linePoint);
            invalidate();
        }

        @Override
        public void draw(Canvas canvas) {
            super.draw(canvas);
            for (LineInfo lineInfo : lines) {
                lineInfo.draw(canvas);
            }
            tipPaint.setColor(greenColor);
            canvas.drawLine(topLine[0], topLine[1], topLine[2], topLine[3], tipPaint);
            tipPaint.setColor(orangeColor);
            canvas.drawLine(middleLine[0], middleLine[1], middleLine[2], middleLine[3], tipPaint);
        }

        class LineInfo {
            private float[] linePoint = new float[4];

            public LineInfo(int fps, int index) {
                this.fps = fps;
                if (fps >= 50) {
                    color = greenColor;
                } else if (fps >= 30) {
                    color = orangeColor;
                } else {
                    color = redColor;
                }
                int width = getMeasuredWidth();

                linePoint[0] = width; // startX
                linePoint[1] = (1 + index) * linePadding;// startY
                linePoint[2] = (60 - fps) * width / 60;// endX
                linePoint[3] = linePoint[1];// endY
            }

            void draw(Canvas canvas) {
                paint.setColor(color);
                canvas.drawLine(linePoint[0], linePoint[1], linePoint[2], linePoint[3], paint);
            }

            int fps;
            int color;
        }


    }


}