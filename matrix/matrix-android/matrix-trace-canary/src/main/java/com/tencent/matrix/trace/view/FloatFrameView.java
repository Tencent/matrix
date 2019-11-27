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
import com.tencent.matrix.trace.constants.Constants;

import java.util.LinkedList;

public class FloatFrameView extends LinearLayout {

    public TextView fpsView;
    public LineChartView chartView;
    public TextView levelFrozenView;
    public TextView levelHighView;
    public TextView levelMiddleView;
    public TextView levelNormalView;
    public TextView extraInfoView;
    public TextView sceneView;
    public TextView qiWangView;

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
        qiWangView = findViewById(R.id.qi_wang);
        levelFrozenView = findViewById(R.id.level_frozen);
        levelHighView = findViewById(R.id.level_high);
        levelMiddleView = findViewById(R.id.level_middle);
        levelNormalView = findViewById(R.id.level_normal);
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
        private Path middlePath = new Path();
        private float[] topTip = new float[2];
        private float[] middleTip = new float[2];

        private int bestColor = getContext().getResources().getColor(R.color.level_best_color);
        private int normalColor = getContext().getResources().getColor(R.color.level_normal_color);
        private int middleColor = getContext().getResources().getColor(R.color.level_middle_color);
        private int highColor = getContext().getResources().getColor(R.color.level_high_color);
        private int frozenColor = getContext().getResources().getColor(R.color.level_frozen_color);

        private int grayColor = getContext().getResources().getColor(R.color.dark_text);
        float padding = dip2px(getContext(), 10);
        float width;
        float lineContentWidth;
        float lineContentHeight;
        float height;
        float textSize;

        public LineChartView(Context context, AttributeSet attrs, int defStyleAttr) {
            super(context, attrs, defStyleAttr);
            paint = new Paint();
            tipPaint = new TextPaint();
            tipPaint.setTextSize(textSize = dip2px(getContext(), 8));
            tipPaint.setStrokeWidth(dip2px(getContext(), 1));
            tipPaint.setColor(grayColor);

            levelLinePaint = new TextPaint();
            levelLinePaint.setStrokeWidth(dip2px(getContext(), 1));
            levelLinePaint.setStyle(Paint.Style.STROKE);

            levelLinePaint.setPathEffect(new DashPathEffect(new float[]{6, 6}, 0));

            tipLinePaint = new Paint(tipPaint);
            tipLinePaint.setStrokeWidth(dip2px(getContext(), 1));
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
                topTip[0] = 10 * rate + padding;
                topTip[1] = lineContentHeight;
                topPath.moveTo(topTip[0], lineContentHeight);
                topPath.lineTo(topTip[0], 0);

                middleTip[0] = 30 * rate + padding;
                middleTip[1] = lineContentHeight;
                middlePath.moveTo(middleTip[0], lineContentHeight);
                middlePath.lineTo(middleTip[0], 0);
            }
        }

        public void addFps(int fps, int color) {
            LineInfo linePoint = new LineInfo(fps, color);
            if (lines.size() >= LINE_COUNT) {
                lines.removeLast();
            }
            lines.addFirst(linePoint);
            invalidate();
        }

        @SuppressLint("DefaultLocale")
        @Override
        public void draw(Canvas canvas) {
            super.draw(canvas);
            int index = 1;
            int sumFps = 0;
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
            tipPaint.setColor(grayColor);
            levelLinePaint.setColor(normalColor);
            canvas.drawPath(topPath, levelLinePaint);
            canvas.drawText("50", topTip[0] - textSize / 2, topTip[1] + textSize, tipPaint);
            levelLinePaint.setColor(middleColor);
            canvas.drawPath(middlePath, levelLinePaint);
            canvas.drawText("30", middleTip[0] - textSize / 2, middleTip[1] + textSize, tipPaint);

        }

        public static int dip2px(Context context, float dpValue) {
            final float scale = context.getResources().getDisplayMetrics().density;
            return (int) (dpValue * scale + 0.5f);
        }

        class LineInfo {
            private float[] linePoint = new float[4];

            LineInfo(int fps, int color) {
                this.fps = fps;
                this.color = color;

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
            if (fps > 60 - Constants.DEFAULT_DROPPED_NORMAL) {
                color = bestColor;
            } else if (fps > 60 - Constants.DEFAULT_DROPPED_MIDDLE) {
                color = normalColor;
            } else if (fps > 60 - Constants.DEFAULT_DROPPED_HIGH) {
                color = middleColor;
            } else if (fps > 60 - Constants.DEFAULT_DROPPED_FROZEN) {
                color = highColor;
            } else {
                color = frozenColor;
            }
            return color;
        }
    }


}