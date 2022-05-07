package sample.tencent.matrix.battery.stats.chart;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.Shader;
import android.util.AttributeSet;
import android.view.View;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

import androidx.annotation.Nullable;
import androidx.core.util.Pair;
import sample.tencent.matrix.R;

public class SimpleLineChart extends View {
    public static final int MODE_ONE = 1;
    public static final int MODE_TWO = 2;
    protected static final DateFormat sTimeFormat = new SimpleDateFormat("HH:mm", Locale.US);
    static final int MAX_COUNT = 7;

    // 画笔
    private Paint mPaint;
    // 背景画笔
    private Paint mBgPaint;
    // Y 轴
    private Float[] mData = {70f, 100f, 95f, 88f, 80f, 60f, 20f};
    // X 轴
    private Long[] mTimeMillis = {0L, 0L, 0L, 0L, 0L, 0L, 0L};

    // 自身宽高
    private float mSelfWidth;
    private float mSelfHeight;
    // 底部文字宽高
    float bottomTextW;
    float bottomTextH;
    // 水平垂直方向边距
    float horizontalSpace = 10F;
    float verticalSpace = 10F;
    // 底部竖条高度
    float bottomVerticalLineHeight = 20F;
    // 底部文字和底部线条间距
    float textWithLineSpace = bottomVerticalLineHeight + 20F;
    // 每个年份的宽度
    float proportionWidth;
    // 底部的高度
    float bottomHeight;
    // 上方文字高度
    float topTextHeight = 120F;
    // 里面圆的半径
    float insideRadius = 6;
    // 外面面圆的半径
    float outsideRadius = 12;
    // 底部横线的宽度
    float lineWidth = 1F;
    // 画折线图的 path
    Path linePath;
    // 画背景的p ath
    Path bgPath;
    // 点的集合
    List<PointF> points = new ArrayList<>();
    // 顶部文字距离折线的高度
    float topTextSpace = 20f;

    private int mode = MODE_ONE;

    public SimpleLineChart(Context context) {
        this(context, null);
    }

    public SimpleLineChart(Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public SimpleLineChart(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    private void init() {
        mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mBgPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        linePath = new Path();
        bgPath = new Path();
        Rect rect = new Rect();
        String time = getTimeString(mTimeMillis[0]);
        mPaint.getTextBounds(time, 0, time.length(), rect);
        bottomTextW = rect.width();
        bottomTextH = rect.height();
        // Y 轴底部文字的宽度
        proportionWidth = (mSelfWidth - 2 * horizontalSpace) / mTimeMillis.length;
    }

    private String getDataString(float value) {
        if (value < 0) {
            return "";
        }
        return ((int) value) + "%";
    }

    protected String getTimeString(long millis) {
        if (millis <= 0) {
            return "NULL";
        }
        return sTimeFormat.format(new Date(millis));
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        mSelfWidth = w;
        mSelfHeight = h;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        init();
        // 将所有的坐标点转成PointF,并放入list
        initPoint();
        // 绘制底部文字
        drawBottomText(canvas);
        // 绘制底部横线
        drawBottomLine(canvas);
        // 绘制折线图背景
        drawBg(canvas);
        // 绘制折线图
        drawBrokenLine(canvas);
        // 绘制折线图上的文字
        drawTopText(canvas);
    }

    private void initPoint() {
        // 折线图的高度 = view的高度 - 上面文字高度 - 下面文字高度
        float brokenLineHeight = mSelfHeight - topTextHeight - bottomHeight;
        // 单位折线图高度 = 折线图的高度 / 数据集中最大的值
        float proportionHeight = brokenLineHeight / getMaxData();
        // 初始x点坐标 = 水平方向间距 + 每个点的宽度/2
        float circleCenterX = horizontalSpace + proportionWidth / 2;
        // 初始y点坐标 = 0
        float circleCenterY;
        // 使用之前clear一下，有可能执行多次onDraw
        points.clear();
        // 将点放入到集合
        for (Float datum : mData) {
            float currentProportionHeight = datum * proportionHeight;
            circleCenterY = mSelfHeight - bottomHeight - currentProportionHeight;
            points.add(new PointF(circleCenterX, circleCenterY));
            circleCenterX += proportionWidth;
        }
    }

    /**
     * 画底部文字
     */
    private void drawBottomText(Canvas canvas) {
        // 文字初始点 x 坐标
        float currentTextX = 0;
        // 文字初始点 y 坐标
        float currentTextY = 0;
        currentTextX = proportionWidth / 2 + horizontalSpace;
        currentTextY = mSelfHeight - verticalSpace;
        mPaint.setTextSize(DisplayUtils.sp2px(getContext(), 13));
        mPaint.setColor(getResources().getColor(R.color.FG_2));
        mPaint.setTextAlign(Paint.Align.CENTER);
        for (Long timeMilli : mTimeMillis) {
            // currentTextX 左边起始点，currentTextY 文字基线坐标
            canvas.drawText(getTimeString(timeMilli), currentTextX, currentTextY, mPaint);
            currentTextX += proportionWidth;
        }
    }

    /**
     * 画底部线条
     */
    private void drawBottomLine(Canvas canvas) {
        float bottomLineStartX = horizontalSpace;
        float bottomLineStopX = mSelfWidth - horizontalSpace;
        float bottomLineStartY = mSelfHeight - bottomTextH - textWithLineSpace - verticalSpace;
        float bottomLineStopY = bottomLineStartY;

        mPaint.setColor(getResources().getColor(R.color.Red));
        mPaint.setStrokeWidth(lineWidth);

        canvas.drawLine(bottomLineStartX, bottomLineStartY, bottomLineStopX, bottomLineStopY, mPaint);
        // 起点X： 水平间距
        float verticalLineStartX = horizontalSpace;
        // 起点Y：高度 - 文字高度 - 文字和横线间距 - 垂直间距
        float verticalLineStartY = mSelfHeight - bottomTextH - textWithLineSpace - verticalSpace;
        // 终点X：不变
        float verticalLineStopX = verticalLineStartX;
        // 终点Y：起点Y + 线长
        float verticalLineStopY = verticalLineStartY + bottomVerticalLineHeight;

        if (mode == MODE_ONE) {
            for (int i = 0; i < mTimeMillis.length + 1; i++) {
                canvas.drawLine(verticalLineStartX, verticalLineStartY, verticalLineStopX, verticalLineStopY, mPaint);
                verticalLineStartX = verticalLineStartX + proportionWidth;
                verticalLineStopX = verticalLineStartX;
            }
        }

        // 底部的高度就是总体高度 - 竖线的Y起始点
        bottomHeight = mSelfHeight - verticalLineStartY;
    }

    /**
     * 画折线
     */
    private void drawBrokenLine(Canvas canvas) {
        // 画点
        for (int i = 0; i < points.size(); i++) {
            if (mode == MODE_ONE) {
                mPaint.setColor(getResources().getColor(R.color.Red_170));
                canvas.drawCircle(points.get(i).x, points.get(i).y, outsideRadius, mPaint);
            }
            mPaint.setColor(getResources().getColor(R.color.Red));
            canvas.drawCircle(points.get(i).x, points.get(i).y, insideRadius, mPaint);
        }

        // 连线
        linePath.reset();
        for (int i = 0; i < points.size(); i++) {
            if (i == 0) {
                linePath.moveTo(points.get(i).x, points.get(i).y);
            } else {
                linePath.lineTo(points.get(i).x, points.get(i).y);
            }
        }
        mPaint.setStyle(Paint.Style.STROKE);
        mPaint.setStrokeWidth(lineWidth);
        mPaint.setColor(getResources().getColor(R.color.Red));
        canvas.drawPath(linePath, mPaint);
    }

    /**
     * 画渐变背景
     */
    private void drawBg(Canvas canvas) {
        // 折线图的高度
        float brokenLineHeight = mSelfHeight - topTextHeight - bottomHeight;
        // 单位折线图高度
        float proportionHeight = brokenLineHeight / getMaxData();
        bgPath.reset();
        bgPath.moveTo(horizontalSpace + proportionWidth / 2, mSelfHeight - bottomHeight);
        for (int i = 0; i < points.size(); i++) {
            bgPath.lineTo(points.get(i).x, points.get(i).y);
        }
        float bgPathEndX = points.get(points.size() - 1).x;
        bgPath.lineTo(bgPathEndX, mSelfHeight - bottomHeight);
        bgPath.close();
        mBgPaint.setStyle(Paint.Style.FILL);


        float shaderStartX = horizontalSpace + proportionWidth / 2;
        float shaderStartY = mSelfHeight - bottomHeight - getMaxData() * proportionHeight;

        float shaderStopX = shaderStartX;
        float shaderStopY = mSelfHeight - bottomHeight - topTextHeight;

        Shader shader = new LinearGradient(
                shaderStartX,
                shaderStartY,
                shaderStopX,
                shaderStopY,
                Color.parseColor("#4DFFB59C"),
                Color.parseColor("#4DFFF5F1"),
                Shader.TileMode.CLAMP);

        mBgPaint.setShader(shader);
        canvas.drawPath(bgPath, mBgPaint);
    }

    private void drawTopText(Canvas canvas) {
        Rect rect = new Rect();
        mPaint.setStyle(Paint.Style.FILL);
        mPaint.setColor(getResources().getColor(R.color.FG_0));
        mPaint.setTextAlign(Paint.Align.CENTER);
        for (int i = 0; i < mData.length; i++) {
            float textX = points.get(i).x;
            float textY = points.get(i).y - topTextSpace;
            canvas.drawText(getDataString(mData[i]), textX, textY, mPaint);
        }
    }


    private float getMaxData() {
        float max = 0;
        for (Float datum : mData) {
            max = Math.max(max, datum);
        }
        return max;
    }

    public void setData(List<Pair<Float, Long>> dataSet) {
        dataSet = polishDataSet(dataSet);
        List<Float> data = new ArrayList<>();
        List<Long> millis = new ArrayList<>();
        for (Pair<Float, Long> item : dataSet) {
            data.add(item.first);
            millis.add(item.second);
        }
        mData = data.toArray(new Float[0]);
        mTimeMillis = millis.toArray(new Long[0]);
        invalidate();
    }

    public void setMode(int mode) {
        this.mode = mode;
    }

    static List<Pair<Float, Long>> polishDataSet(List<Pair<Float, Long>> input) {
        if (input.size() <= MAX_COUNT) {
            return input;
        }

        float avg = 0;
        for (Pair<Float, Long> item : input) {
            avg += item.first;
        }
        avg = avg / input.size();

        Set<Integer> targetIdxSet = new HashSet<>();
        targetIdxSet.add(0);
        targetIdxSet.add(input.size() - 1);
        Pair<Integer, Integer> minMaxIdx = figureOutMinMaxIdx(input);
        targetIdxSet.add(minMaxIdx.first);
        targetIdxSet.add(minMaxIdx.second);
        List<Integer> targetIdxList = new ArrayList<>(targetIdxSet);

        figureOutIdxList(input, targetIdxList, avg, MAX_COUNT);
        Collections.sort(targetIdxList);

        List<Pair<Float, Long>> output = new ArrayList<>(MAX_COUNT);
        for (Integer idx : targetIdxList) {
            output.add(input.get(idx));
        }
        return output;
    }

    private static Pair<Integer, Integer> figureOutMinMaxIdx(List<Pair<Float, Long>> input) {
        if (input.isEmpty()) {
            throw new IllegalStateException("Input list in empty");
        }
        int idxMin = 0, idxMax = 0;
        float lastMin = input.get(idxMin).first, lastMax = input.get(idxMax).first;
        for (int i = 1; i < input.size(); i++) {
            Pair<Float, Long> curr = input.get(i);
            if (curr.first < lastMin) {
                lastMin = curr.first;
                idxMin = i;
            }
            if (curr.first > lastMax) {
                lastMax = curr.first;
                idxMax = i;
            }
        }
        return new Pair<>(idxMin, idxMax);
    }

    private static void figureOutIdxList(List<Pair<Float, Long>> input, List<Integer> targetIdxList, float avg, int findCount) {
        if (input.size() >= findCount && targetIdxList.size() < findCount) {
            int idx = -1;
            float lastDelta = -1;
            for (int i = 0; i < input.size(); i++) {
                if (!targetIdxList.contains(i)) {
                    float delta = input.get(i).first - avg;
                    if (Math.abs(delta) > lastDelta) {
                        idx = i;
                        lastDelta = delta;
                    }
                }
            }
            targetIdxList.add(idx);
            figureOutIdxList(input, targetIdxList, avg, findCount);
        }
    }
}
