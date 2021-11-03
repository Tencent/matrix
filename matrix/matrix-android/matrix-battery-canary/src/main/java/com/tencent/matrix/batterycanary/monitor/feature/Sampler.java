package com.tencent.matrix.batterycanary.monitor.feature;

import android.os.Handler;
import android.os.SystemClock;

import com.tencent.matrix.util.MatrixLog;

import java.util.concurrent.Callable;

import androidx.annotation.Nullable;

import static com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil.ONE_MIN;

/**
 * @author Kaede
 * @since 2021/11/3
 */
class Sampler {
    private static final String TAG = "Matrix.battery.Sampler";

    final Handler mHandler;
    final Callable<? extends Number> mSamplingBlock;

    private final Runnable mSamplingTask = new Runnable() {
        @Override
        public void run() {
            try {
                Number currSample = mSamplingBlock.call();
                mSampleLst = currSample.doubleValue();
                mCount++;
                mSampleAvg = (mSampleAvg * (mCount - 1) + mSampleLst) / mCount;
                if (mSampleFst == Double.MIN_VALUE) {
                    mSampleFst = mSampleLst;
                    mSampleMax = mSampleLst;
                    mSampleMin = mSampleLst;
                } else {
                    if (mSampleLst > mSampleMax) {
                        mSampleMax = mSampleLst;
                    }
                    if (mSampleLst < mSampleMin) {
                        mSampleMin = mSampleLst;
                    }
                }
            } catch (Exception e) {
                MatrixLog.printErrStackTrace(TAG, e, "onSamplingFailed: " + e);
            } finally {
                if (!mPaused) {
                    mHandler.postDelayed(this, mInterval);
                }
            }
        }
    };

    boolean mPaused = true;
    long mInterval = ONE_MIN;
    int mCount = 0;
    long mBgnMillis = 0;
    long mEndMillis = 0;
    double mSampleFst = Double.MIN_VALUE;
    double mSampleLst = Double.MIN_VALUE;
    double mSampleMax = Double.MIN_VALUE;
    double mSampleMin = Double.MIN_VALUE;
    double mSampleAvg = Double.MIN_VALUE;

    Sampler(Handler handler, Callable<? extends Number> onSampling) {
        mHandler = handler;
        mSamplingBlock = onSampling;
    }

    public void setInterval(long interval) {
        if (interval > 0) {
            mInterval = interval;
        }
    }

    public void start() {
        mPaused = false;
        mBgnMillis = SystemClock.uptimeMillis();
        mHandler.postDelayed(mSamplingTask, mInterval);
    }

    public void pause() {
        mPaused = true;
        mEndMillis = SystemClock.uptimeMillis();
        mHandler.removeCallbacks(mSamplingTask);
    }

    @Nullable
    public Result getResult() {
        if (mCount <= 0) {
            MatrixLog.w(TAG, "Sampling count is invalid: " + mCount);
            return null;
        }
        if (mBgnMillis <= 0 || mEndMillis <= 0 || mBgnMillis > mEndMillis) {
            MatrixLog.w(TAG, "Sampling bgn/end millis is invalid: " + mBgnMillis + " - " + mEndMillis);
            return null;
        }
        Result result = new Result();
        result.interval = mInterval;
        result.count = mCount;
        result.duringMillis = mEndMillis - mBgnMillis;
        result.sampleFst = mSampleFst;
        result.sampleLst = mSampleLst;
        result.sampleMax = mSampleMax;
        result.sampleMin = mSampleMin;
        result.sampleAvg = mSampleAvg;
        return result;
    }

    public static final class Result {
        public long interval;
        public int count;
        public long duringMillis;
        public double sampleFst;
        public double sampleLst;
        public double sampleMax;
        public double sampleMin;
        public double sampleAvg;
    }
}
