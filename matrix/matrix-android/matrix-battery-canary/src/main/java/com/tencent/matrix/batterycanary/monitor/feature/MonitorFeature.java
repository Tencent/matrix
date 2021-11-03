package com.tencent.matrix.batterycanary.monitor.feature;


import android.os.Handler;
import android.os.SystemClock;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.Callable;

import static com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil.ONE_MIN;

public interface MonitorFeature {
    void configure(BatteryMonitorCore monitor);
    void onTurnOn();
    void onTurnOff();
    void onForeground(boolean isForeground);
    void onBackgroundCheck(long duringMillis);
    int weight();


    @SuppressWarnings("rawtypes")
    abstract class Snapshot<RECORD extends Snapshot> {
        public final long time;
        public boolean isDelta = false;
        private boolean mIsValid = true;

        @SuppressWarnings("UnusedReturnValue")
        public Snapshot<RECORD> setValid(boolean bool) {
            mIsValid = bool;
            return this;
        }

        public boolean isValid() {
            return mIsValid;
        }

        public Snapshot() {
            time = getTimeStamps();
        }

        public abstract Delta<RECORD> diff(RECORD bgn);

        protected long getTimeStamps() {
            return SystemClock.uptimeMillis();
        }

        public abstract static class Delta<RECORD extends Snapshot> {
            public final RECORD bgn;
            public final RECORD end;
            public final RECORD dlt;
            public final long during;

            public Delta(RECORD bgn, RECORD end) {
                this.bgn = bgn;
                this.end = end;
                this.during = end.time - bgn.time;
                this.dlt = computeDelta();
                dlt.isDelta = true;
            }

            public boolean isValid() {
                return bgn.isValid() && end.isValid();
            }

            protected abstract RECORD computeDelta();
        }

        public abstract static class Entry<ENTRY> {
            private boolean mIsValid = true;

            public ENTRY setValid(boolean bool) {
                mIsValid = bool;
                //noinspection unchecked
                return (ENTRY) this;
            }

            public boolean isValid() {
                return mIsValid;
            }

            public static abstract class DigitEntry<DIGIT extends Number> extends Entry<DigitEntry> {

                @SuppressWarnings("unchecked")
                public static <DIGIT extends Number> DigitEntry<DIGIT> of(DIGIT digit) {
                    if (digit instanceof Integer) {
                        return (DigitEntry<DIGIT>) new IntDigit((Integer) digit);
                    } else if (digit instanceof Long) {
                        return (DigitEntry<DIGIT>) new LongDigit((Long) digit);
                    } else if (digit instanceof Float) {
                        return (DigitEntry<DIGIT>) new FloatDigit((Float) digit);
                    } else if (digit instanceof Double) {
                        return (DigitEntry<DIGIT>) new DoubleDigit((Double) digit);
                    }

                    throw new RuntimeException("unsupported digit: " + digit.getClass());
                }

                DIGIT value;

                public DigitEntry(DIGIT value) {
                    this.value = value;
                }

                public DIGIT get() {
                    return value;
                }

                @Override
                public boolean equals(Object o) {
                    if (this == o) return true;
                    if (o == null || getClass() != o.getClass()) return false;
                    DigitEntry<?> that = (DigitEntry<?>) o;
                    return value.equals(that.value);
                }

                @Override
                public int hashCode() {
                    return Objects.hash(value);
                }

                @Override
                @NonNull
                public String toString() {
                    return String.valueOf(value);
                }

                public abstract DIGIT diff(DIGIT right);

                static class IntDigit extends DigitEntry<Integer> {
                    IntDigit(Integer value) {
                        super(value);
                    }

                    @Override
                    public Integer diff(Integer right) {
                        return value - right;
                    }
                }

                static class LongDigit extends DigitEntry<Long> {
                    LongDigit(Long value) {
                        super(value);
                    }

                    @Override
                    public Long diff(Long right) {
                        return value - right;
                    }
                }

                static class FloatDigit extends DigitEntry<Float> {
                    FloatDigit(Float value) {
                        super(value);
                    }

                    @Override
                    public Float diff(Float right) {
                        return value - right;
                    }
                }

                static class DoubleDigit extends DigitEntry<Double> {
                    DoubleDigit(Double value) {
                        super(value);
                    }

                    @Override
                    public Double diff(Double right) {
                        return value - right;
                    }
                }
            }

            public static class BeanEntry<BEAN> extends Entry<BeanEntry> {
                public static final BeanEntry<?> sEmpty = new BeanEntry<Void>(null) {
                    @Override
                    public boolean isEmpty() {
                        return true;
                    }
                };

                public static <BEAN> BeanEntry<BEAN> of(BEAN bean) {
                    return new BeanEntry<>(bean);
                }

                BEAN value;

                public BeanEntry(BEAN value) {
                    this.value = value;
                }

                public boolean isEmpty() {
                    return false;
                }

                public BEAN get() {
                    return value;
                }

                @Override
                public boolean equals(Object o) {
                    if (this == o) return true;
                    if (o == null || getClass() != o.getClass()) return false;
                    BeanEntry<?> beanEntry = (BeanEntry<?>) o;
                    return Objects.equals(String.valueOf(value), String.valueOf(beanEntry.value));
                }

                @Override
                public int hashCode() {
                    return Objects.hash(value);
                }

                @Override
                @NonNull
                public String toString() {
                    return String.valueOf(value);
                }
            }

            public static class ListEntry<ITEM extends Entry> extends Entry<ListEntry> {

                public static <ITEM extends Entry> ListEntry<ITEM> of(List<ITEM> items) {
                    ListEntry<ITEM> listEntry = new ListEntry<>();
                    listEntry.list = items;
                    return listEntry;
                }

                public static <BEAN> ListEntry<BeanEntry<BEAN>> ofBeans(List<BEAN> items) {
                    List<BeanEntry<BEAN>> list = new ArrayList<>();
                    for (BEAN item : items) {
                        list.add(BeanEntry.of(item));
                    }
                    return of(list);
                }

                public static <DIGIT extends Number> ListEntry<DigitEntry<DIGIT>> ofDigits(List<DIGIT> items) {
                    List<DigitEntry<DIGIT>> list = new ArrayList<>();
                    for (DIGIT item : items) {
                        list.add(DigitEntry.of(item));
                    }
                    ListEntry<DigitEntry<DIGIT>> listEntry = new ListEntry<>();
                    listEntry.list = list;
                    return listEntry;
                }

                public static <DIGIT extends Number> ListEntry<DigitEntry<DIGIT>> ofDigits(DIGIT[] items) {
                    return ofDigits(Arrays.asList(items));
                }

                public static ListEntry<DigitEntry<Integer>> ofDigits(int[] items) {
                    List<DigitEntry<Integer>> list = new ArrayList<>();
                    for (int item : items) {
                        list.add(DigitEntry.of(item));
                    }
                    ListEntry<DigitEntry<Integer>> listEntry = new ListEntry<>();
                    listEntry.list = list;
                    return listEntry;
                }

                public static ListEntry<DigitEntry<Long>> ofDigits(long[] items) {
                    List<DigitEntry<Long>> list = new ArrayList<>();
                    for (long item : items) {
                        list.add(DigitEntry.of(item));
                    }
                    ListEntry<DigitEntry<Long>> listEntry = new ListEntry<>();
                    listEntry.list = list;
                    return listEntry;
                }

                public static ListEntry<DigitEntry<Float>> ofDigits(float[] items) {
                    List<DigitEntry<Float>> list = new ArrayList<>();
                    for (float item : items) {
                        list.add(DigitEntry.of(item));
                    }
                    ListEntry<DigitEntry<Float>> listEntry = new ListEntry<>();
                    listEntry.list = list;
                    return listEntry;
                }

                public static ListEntry<DigitEntry<Double>> ofDigits(double[] items) {
                    List<DigitEntry<Double>> list = new ArrayList<>();
                    for (double item : items) {
                        list.add(DigitEntry.of(item));
                    }
                    ListEntry<DigitEntry<Double>> listEntry = new ListEntry<>();
                    listEntry.list = list;
                    return listEntry;
                }

                public static <ITEM extends Entry> ListEntry<ITEM> ofEmpty() {
                    ListEntry<ITEM> listEntry = new ListEntry<>();
                    listEntry.list = new ArrayList<>();
                    return listEntry;
                }


                List<ITEM> list;

                private ListEntry() {
                }

                @Override
                public boolean isValid() {
                    for (ITEM item : list) {
                        if (!item.isValid()) return false;
                    }
                    return super.isValid();
                }

                public List<ITEM> getList() {
                    return list;
                }
            }
        }

        public interface Differ<ENTRY extends Entry> {

            @NonNull
            ENTRY diff(@NonNull ENTRY bgn, @NonNull ENTRY end);

            class DigitDiffer<DIGIT extends Number> implements Differ<Entry.DigitEntry<DIGIT>> {
                static final DigitDiffer sGlobal = new DigitDiffer();

                @NonNull
                public static <DIGIT extends Number> Entry.DigitEntry<DIGIT> globalDiff(@NonNull Entry.DigitEntry<DIGIT> bgn, @NonNull Entry.DigitEntry<DIGIT> end) {
                    //noinspection unchecked
                    return sGlobal.diff(bgn, end);
                }

                @Override
                @NonNull
                public Entry.DigitEntry<DIGIT> diff(@NonNull Entry.DigitEntry<DIGIT> bgn, @NonNull Entry.DigitEntry<DIGIT> end) {
                    DIGIT diff = end.diff(bgn.value);
                    return Entry.DigitEntry.of(diff);
                }
            }

            class BeanDiffer<BEAN> implements Differ<Entry.BeanEntry<BEAN>> {
                static final BeanDiffer sGlobal = new BeanDiffer<>();

                @NonNull
                public static <BEAN> Entry.BeanEntry<BEAN> globalDiff(@NonNull Entry.BeanEntry<BEAN> bgn, @NonNull Entry.BeanEntry<BEAN> end) {
                    //noinspection unchecked
                    return sGlobal.diff(bgn, end);
                }

                @Override
                @NonNull
                public Entry.BeanEntry<BEAN> diff(@NonNull Entry.BeanEntry<BEAN> bgn, @NonNull Entry.BeanEntry<BEAN> end) {
                    if (end == bgn || end.equals(bgn)) {
                        //noinspection unchecked
                        return (Entry.BeanEntry<BEAN>) Entry.BeanEntry.sEmpty;
                    }
                    return end;
                }
            }

            class ListDiffer<ENTRY extends Entry> implements Differ<Entry.ListEntry<ENTRY>> {
                static final ListDiffer sGlobal = new ListDiffer<>();

                @NonNull
                public static <ENTRY extends Entry> Entry.ListEntry<ENTRY> globalDiff(@NonNull Entry.ListEntry<ENTRY> bgn, @NonNull Entry.ListEntry<ENTRY> end) {
                    //noinspection unchecked
                    return sGlobal.diff(bgn, end);
                }

                @NonNull
                @Override
                public Entry.ListEntry<ENTRY> diff(@NonNull Entry.ListEntry<ENTRY> bgn, @NonNull Entry.ListEntry<ENTRY> end) {
                    Entry.ListEntry<ENTRY> diff = Entry.ListEntry.ofEmpty();
                    for (int i = 0; i < end.list.size(); i++) {
                        ENTRY endEntry = end.list.get(i);
                        if (endEntry instanceof Entry.DigitEntry) {
                            // digit
                            if (bgn.list.size() > i) {
                                ENTRY bgnEntry = bgn.list.get(i);
                                if (bgnEntry instanceof Entry.DigitEntry) {
                                    //noinspection unchecked
                                    diff.list.add((ENTRY) DigitDiffer.globalDiff((Entry.DigitEntry) bgnEntry, (Entry.DigitEntry) endEntry));
                                    continue;
                                }
                            }
                            //noinspection unchecked
                            diff.list.add((ENTRY) Entry.DigitEntry.of(((Entry.DigitEntry) endEntry).value).setValid(false));

                        } else if (endEntry instanceof Entry.BeanEntry) {
                            // bean
                            if (bgn.list.contains(endEntry)) {
                                continue;
                            }
                            boolean find = false;
                            for (ENTRY bgnEntry : bgn.list) {
                                if (!(bgnEntry instanceof Entry.BeanEntry)) {
                                    continue;
                                }
                                if (BeanDiffer.globalDiff((Entry.BeanEntry) bgnEntry, (Entry.BeanEntry) endEntry) == Entry.BeanEntry.sEmpty) {
                                    find = true;
                                    break;
                                }
                            }
                            if (!find) {
                                diff.list.add(endEntry);
                            }
                        }
                    }
                    return diff;
                }
            }
        }

        public static class Sampler {
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

            public Sampler(Handler handler, Callable<? extends Number> onSampling) {
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
    }
}
