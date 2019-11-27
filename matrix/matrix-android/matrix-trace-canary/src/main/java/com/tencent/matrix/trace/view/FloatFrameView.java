package com.tencent.matrix.trace.view;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.DashPathEffect;
import android.graphics.Paint;
import android.graphics.Path;
import android.text.TextPaint;
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
    public LineChartView chartView;
    public TextView levelFrozenView;
    public TextView levelHighView;
    public TextView levelMiddleView;
    public TextView levelNormalView;
    public TextView levelBestView;
    public TextView extraInfoView;
    public TextView sceneView;

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
        extraInfoView = findViewById(R.id.extra_info);
        sceneView = findViewById(R.id.scene_view);
        extraInfoView.setText("{other info}");
        levelFrozenView = findViewById(R.id.level_frozen);
        levelHighView = findViewById(R.id.level_high);
        levelMiddleView = findViewById(R.id.level_middle);
        levelNormalView = findViewById(R.id.level_normal);
        levelBestView = findViewById(R.id.level_best);
        chartView = findViewById(R.id.chart);
    }


    public static class LineChartView extends View {

        private final Paint paint;
        private final TextPaint tipPaint;
        private final Paint levelLinePaint;
        private final Paint tipLinePaint;
        private final static int LINE_COUNT = 50;
        private final LinkedList<LineInfo> lines;
        float linePadding;
        float lineStrokeWidth;
        private Path topPath = new Path();
        private float[] topTip = new float[2];

        private int bestColor = getContext().getResources().getColor(R.color.level_best_color);
        private int normalColor = getContext().getResources().getColor(R.color.level_normal_color);
        private int middleColor = getContext().getResources().getColor(R.color.level_middle_color);
        private int highColor = getContext().getResources().getColor(R.color.level_high_color);
        private int frozenColor = getContext().getResources().getColor(R.color.level_frozen_color);

        private int grayColor = getContext().getResources().getColor(R.color.dark_text);
        float padding = 10 * getContext().getResources().getDisplayMetrics().density;
        float width;
        float lineContentWidth;
        float lineContentHeight;
        float height;
        float textSize;

        public LineChartView(Context context, AttributeSet attrs, int defStyleAttr) {
            super(context, attrs, defStyleAttr);
            paint = new Paint();
            tipPaint = new TextPaint();
            tipPaint.setTextSize(textSize = 8 * getContext().getResources().getDisplayMetrics().density);
            tipPaint.setStrokeWidth(3);
            tipPaint.setColor(grayColor);

            levelLinePaint = new TextPaint();
            levelLinePaint.setStrokeWidth(2);
            levelLinePaint.setStyle(Paint.Style.STROKE);

            tipLinePaint = new Paint(tipPaint);
            tipLinePaint.setStrokeWidth(2);
            tipLinePaint.setColor(grayColor);
            tipLinePaint.setStyle(Paint.Style.STROKE);
            tipLinePaint.setPathEffect(new DashPathEffect(new float[]{6, 6}, 0));
            lines = new LinkedList<>();
        }

        public LineChartView(Context context, AttributeSet attrs) {
            this(context, attrs, 0);
        }

        @Override
        protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
            super.onLayout(changed, left, top, right, bottom);
            if (changed) {
                width = getMeasuredWidth();
                height = getMeasuredHeight();

                lineContentWidth = width - padding;
                lineContentHeight = height - 2 * padding;
//                lineStrokeWidth = (height - 2 * padding) / (LINE_COUNT * 2);

                lineStrokeWidth = dip2px(getContext(), 1);
                paint.setStrokeWidth(lineStrokeWidth);
                linePadding = lineStrokeWidth * 2;

                float rate = lineContentWidth / 60;
                topTip[0] = 20 * rate + padding;
                topTip[1] = lineContentHeight;
                topPath.moveTo(topTip[0], lineContentHeight);
                topPath.lineTo(topTip[0], 0);
            }
        }

        public void addFps(int fps) {
            LineInfo linePoint = new LineInfo(fps);
            synchronized (lines) {
                if (lines.size() >= LINE_COUNT) {
                    lines.removeLast();
                }
                lines.addFirst(linePoint);
            }
            postInvalidate();
        }

        @SuppressLint("DefaultLocale")
        @Override
        public void draw(Canvas canvas) {
            super.draw(canvas);
            int index = 1;
            int sumFps = 0;
            synchronized (lines) {
                for (LineInfo lineInfo : lines) {
                    sumFps += lineInfo.fps;
                    lineInfo.draw(canvas, index);
                    if (index % 25 == 0) {
                        Path path = new Path();
                        float pathY = lineInfo.linePoint[1];
                        path.moveTo(0, pathY);
                        path.lineTo(getMeasuredHeight(), pathY);
                        canvas.drawPath(path, tipLinePaint);
                        tipPaint.setColor(grayColor);
                        canvas.drawText(index / 5 + "s", 0, pathY + textSize, tipPaint);
                        if (index > 0) {
                            int aver = sumFps / index;
                            tipPaint.setColor(getColor(aver));
                            canvas.drawText(aver + " FPS", 0, pathY - textSize / 2, tipPaint);
                        }
                    }
                    index++;
                }
            }
            tipPaint.setColor(grayColor);
            levelLinePaint.setColor(middleColor);
            canvas.drawPath(topPath, levelLinePaint);
//            canvas.drawText(String.format("%d FPS", (60 - Constants.DEFAULT_DROPPED_MIDDLE)), topTip[0] - textSize / 2, topTip[1], tipPaint);

        }

        public static int dip2px(Context context, float dpValue) {
            final float scale = context.getResources().getDisplayMetrics().density;
            return (int) (dpValue * scale + 0.5f);
        }

        class LineInfo {
            private float[] linePoint = new float[4];

            LineInfo(int fps) {
                this.fps = fps;

                color = getColor(fps);
                linePoint[0] = width; // startX
                linePoint[2] = (60 - fps) * (lineContentWidth) / 60 + padding; // endX

            }

            void draw(Canvas canvas, int index) {
                if (paint.getColor() != color) {
                    paint.setColor(color);
                }
                linePoint[1] = (1 + index) * linePadding; // startY
                linePoint[3] = linePoint[1]; // endY
                canvas.drawLine(linePoint[0], linePoint[1], linePoint[2], linePoint[3], paint);
            }

            int fps;
            int color;
        }

        private int getColor(int fps) {
            int color;
            if (fps > 56) {
                color = bestColor;
            } else if (fps > 40) {
                color = middleColor;
            } else {
                color = highColor;
            }
            return color;
        }
    }


}