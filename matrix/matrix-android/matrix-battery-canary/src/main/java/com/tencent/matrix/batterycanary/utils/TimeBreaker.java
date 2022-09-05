package com.tencent.matrix.batterycanary.utils;

import android.os.SystemClock;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import androidx.annotation.Nullable;
import androidx.annotation.RestrictTo;
import androidx.annotation.VisibleForTesting;

/**
 * Configure timeline portions & ratio for the given stamps and return split-portions with each weight.
 *
 * @author Kaede
 * @since 2020/12/22
 */
@RestrictTo(RestrictTo.Scope.LIBRARY)
public final class TimeBreaker {

    /**
     * !!Thread Unsafe!! operations
     */
    public static void gcList(List<?> list) {
        if (list == null) {
            return;
        }
        int size = list.size();
        int from = size / 2;
        int to = size - 1;
        if (from < to) {
            list.subList(from, to).clear();
        }
    }

    public static TimePortions configurePortions(List<? extends Stamp> outerStampList, long windowToCurr) {
        return configurePortions(outerStampList, windowToCurr, 10L, new Stamp.Stamper() {
            @Override
            public Stamp stamp(String key) {
                return new Stamp(key);
            }
        });
    }

    public static TimePortions configurePortions(List<? extends Stamp> outerStampList, long windowToCurr, long delta, Stamp.Stamper stamper) {
        List<Stamp> stampList = new ArrayList<>(outerStampList);
        if (!stampList.isEmpty()) {
            Stamp currStamp = stamper.stamp("CURR_STAMP");
            long deltaFromLastStamp = currStamp.upTime - stampList.get(0).upTime;
            if (deltaFromLastStamp > delta) {
                stampList.add(0, currStamp);
            }
        }

        final Map<String, StatRecord> mapper = new HashMap<>();
        long totalMillis = 0L;
        long lastStampMillis = Long.MIN_VALUE;
        long lastStampStatMillis = Long.MIN_VALUE;

        if (windowToCurr <= 0L) {
            // configure for long all app uptime
            for (Stamp item : stampList) {
                if (lastStampMillis != Long.MIN_VALUE) {
                    if (lastStampMillis < item.upTime) {
                        // invalid data
                        break;
                    }

                    long interval = lastStampMillis - item.upTime;
                    totalMillis += interval;
                    StatRecord record = mapper.get(item.key);
                    if (record == null) {
                        record = new StatRecord();
                        mapper.put(item.key, record);
                    }
                    record.weight += interval;
                    record.millis += (lastStampStatMillis - item.statMillis);
                }
                lastStampMillis = item.upTime;
                lastStampStatMillis = item.statMillis;
            }
        } else {
            // just configure for long of the given window
            for (Stamp item : stampList) {
                if (lastStampMillis != Long.MIN_VALUE) {
                    if (lastStampMillis < item.upTime) {
                        // invalid data
                        break;
                    }

                    long interval = lastStampMillis - item.upTime;
                    if (totalMillis + interval >= windowToCurr) {
                        // reach widow edge
                        long lastInterval = windowToCurr - totalMillis;
                        totalMillis += lastInterval;
                        StatRecord record = mapper.get(item.key);
                        if (record == null) {
                            record = new StatRecord();
                            mapper.put(item.key, record);
                        }
                        record.weight += lastInterval;
                        record.millis += (lastStampStatMillis - item.statMillis) * (((float) lastInterval) / interval);
                        break;
                    }

                    totalMillis += interval;
                    StatRecord record = mapper.get(item.key);
                    if (record == null) {
                        record = new StatRecord();
                        mapper.put(item.key, record);
                    }
                    record.weight += interval;
                    record.millis += (lastStampStatMillis - item.statMillis);
                }
                lastStampMillis = item.upTime;
                lastStampStatMillis = item.statMillis;
            }
        }

        TimePortions timePortions = new TimePortions();
        if (totalMillis <= 0L) {
            timePortions.mIsValid = false;

        } else {
            // window > uptime
            if (windowToCurr > totalMillis) {
                timePortions.mIsValid = false;
            }

            timePortions.totalUptime = totalMillis;
            List<TimePortions.Portion> portions = new ArrayList<>();
            for (Map.Entry<String, StatRecord> item : mapper.entrySet()) {
                String key  = item.getKey();
                StatRecord value = item.getValue();
                TimePortions.Portion portion = new TimePortions.Portion(key, configureRatio(value.weight, totalMillis));
                portion.totalStatMillis = value.millis;
                portions.add(portion);
            }
            Collections.sort(portions, new Comparator<TimePortions.Portion>() {
                @Override
                public int compare(TimePortions.Portion o1, TimePortions.Portion o2) {
                    long minus = o1.ratio - o2.ratio;
                    if (minus == 0) return 0;
                    if (minus > 0) return -1;
                    return 1;
                }
            });
            timePortions.portions = portions;
        }
        return timePortions;
    }

    static int configureRatio(long my, long total) {
        long round = Math.round(((double) my / total) * 100);
        if (round >= 100) return 100;
        if (round <= 0) return 0;
        return (int) round;
    }

    static final class StatRecord {
        long weight;
        long millis;
    }

    public static class Stamp {
        public interface Stamper {
            Stamp stamp(String key);
        }

        public final String key;
        public final long upTime;
        public final long statMillis;

        public Stamp(String key) {
            this.key = key;
            this.upTime = SystemClock.uptimeMillis();
            this.statMillis = System.currentTimeMillis();
        }

        public Stamp(String key, long upTime) {
            this.key = key;
            this.upTime = upTime;
            this.statMillis = System.currentTimeMillis();
        }

        @VisibleForTesting
        public Stamp(String key, long upTime, long statMillis) {
            this.key = key;
            this.upTime = upTime;
            this.statMillis = statMillis;
        }

        @Override
        public String toString() {
            return "Stamp{" +
                    "key='" + key + '\'' +
                    ", upTime=" + upTime +
                    ", statMillis=" + statMillis +
                    '}';
        }
    }

    public static final class TimePortions {
        public static final class Portion {
            public final String key;
            public final int ratio;
            public long totalStatMillis = 0;

            public Portion(String key, int ratio) {
                this.key = key;
                this.ratio = ratio;
            }
        }

        public static TimePortions ofInvalid() {
            TimePortions item = new TimePortions();
            item.mIsValid = false;
            return item;
        }

        public long totalUptime;
        public List<Portion> portions = Collections.emptyList();
        private boolean mIsValid = true;

        TimePortions() {
        }

        public boolean isValid() {
            return mIsValid;
        }

        public int getRatio(String key) {
            for (Portion item : portions) {
                if (item.key != null && item.key.equals(key)) {
                    return item.ratio;
                }
            }
            return 0;
        }

        @Nullable
        public Portion top1() {
            if (portions.size() >= 1) {
                return portions.get(0);
            }
            return null;
        }

        @Nullable
        public Portion top2() {
            if (portions.size() >= 2) {
                return portions.get(1);
            }
            return null;
        }
    }
}
