package com.tencent.matrix.batterycanary.monitor.feature;


import android.os.SystemClock;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;

public interface MonitorFeature {
    void configure(BatteryMonitorCore monitor);
    void onTurnOn();
    void onTurnOff();
    void onForeground(boolean isForeground);
    int weight();

    @SuppressWarnings("rawtypes")
    abstract class Snapshot<RECORD extends Snapshot> {
        public final long time;
        public boolean isDelta = false;
        public boolean isValid = true;

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
            public boolean isValid = true;

            public Delta(RECORD bgn, RECORD end) {
                this.bgn = bgn;
                this.end = end;
                this.during = end.time - bgn.time;
                this.dlt = computeDelta();
                dlt.isDelta = true;
            }

            protected abstract RECORD computeDelta();
        }

        interface Differ<VALUE> {
            int UNSPECIFIED_DIGIT = Integer.MAX_VALUE;
            LongDiffer sDigitDiffer = new LongDiffer();
            IntArrayDiffer sDigitArrayDiffer = new IntArrayDiffer();

            VALUE diff(VALUE bgn, VALUE end);

            class LongDiffer implements Differ<Long> {

                @Override
                public Long diff(Long bgn, Long end) {
                    long diff = end - bgn;
                    return diff >= 0 ? diff : 0;
                }

                public Integer diffInt(Integer bgn, Integer end) {
                    return diff((long) bgn, (long) end).intValue();
                }
            }

            class IntArrayDiffer implements Differ<int[]> {
                @Override
                public int[] diff(int[] bgn, int[] end) {
                    int[] dlt = new int[0];
                    if (end != null) {
                        dlt = new int[end.length];
                        for (int i = 0; i < end.length; i++) {
                            dlt[i] = sDigitDiffer.diffInt(((bgn == null || bgn.length < i) ? 0 : bgn[i]), end[i]);
                        }
                    }
                    return dlt;
                }
            }
        }
    }

}
