package com.tencent.matrix.batterycanary.utils;

import android.arch.core.util.Function;
import android.os.SystemClock;
import android.support.annotation.Nullable;
import android.support.annotation.RestrictTo;
import android.support.v4.util.Pair;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Configure timeline portions & ratio for the given stamps as splits.
 *
 * @author Kaede
 * @since 2020/12/22
 */
@RestrictTo(RestrictTo.Scope.LIBRARY)
public final class TimeBreaker {

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

    public static TimePortions configurePortions(List<? extends Stamp> stampList, long windowMillis) {
        final Map<String, Long> mapper = new HashMap<>();
        long totalMillis = 0L;
        long lastStampMillis = Long.MIN_VALUE;

        if (windowMillis <= 0L) {
            // configure for long all app uptime
            for (Stamp item : stampList) {
                if (lastStampMillis != Long.MIN_VALUE) {
                    if (lastStampMillis < item.upTime) {
                        // invalid data
                        break;
                    }

                    long interval = lastStampMillis - item.upTime;
                    totalMillis += interval;
                    Long record = mapper.get(item.key);
                    mapper.put(item.key, interval + (record == null ? 0 : record));
                }
                lastStampMillis = item.upTime;
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
                    if (totalMillis + interval >= windowMillis) {
                        // reach widow edge
                        long lastInterval = windowMillis - totalMillis;
                        totalMillis += lastInterval;
                        Long record = mapper.get(item.key);
                        mapper.put(item.key, lastInterval + (record == null ? 0 : record));
                        break;
                    }

                    totalMillis += interval;
                    Long record = mapper.get(item.key);
                    mapper.put(item.key, interval + (record == null ? 0 : record));
                }
                lastStampMillis = item.upTime;
            }
        }

        TimePortions timePortions = new TimePortions();
        if (totalMillis <= 0L) {
            timePortions.mIsValid = false;

        } else {
            // window > uptime
            if (windowMillis > totalMillis) {
                timePortions.mIsValid = false;
            }

            timePortions.totalUptime = totalMillis;
            Function<String, Long> block = new Function<String, Long>() {
                @Override
                public Long apply(String input) {
                    Long millis = mapper.get(input);
                    return millis == null ? 0 : millis;
                }
            };

            List<Pair<String, Integer>> portions = new ArrayList<>();
            for (Map.Entry<String, Long> item : mapper.entrySet()) {
                String key  = item.getKey();
                long value = item.getValue();
                portions.add(new Pair<>(key, configureRatio(value, totalMillis)));
            }
            Collections.sort(portions, new Comparator<Pair<String, Integer>>() {
                @Override
                public int compare(Pair<String, Integer> o1, Pair<String, Integer> o2) {
                    long minus = (o1.second == null ? 0 : o1.second) - (o2.second == null ? 0 : o2.second);
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

    public static class Stamp {
        public final String key;
        public final long upTime;

        public Stamp(String key) {
            this.key = key;
            this.upTime = SystemClock.uptimeMillis();
        }

        public Stamp(String key, long upTime) {
            this.key = key;
            this.upTime = upTime;
        }
    }

    public static final class TimePortions {
        public static TimePortions ofInvalid() {
            TimePortions item = new TimePortions();
            item.mIsValid = false;
            return item;
        }

        public long totalUptime;
        public List<Pair<String, Integer>> portions = Collections.emptyList();
        private boolean mIsValid = true;

        TimePortions() {}

        public boolean isValid() {
            return mIsValid;
        }

        public int getRatio(String key) {
            for (Pair<String, Integer> item : portions) {
                if (item.first != null && item.first.equals(key)) {
                    return item.second == null ? 0 : item.second;
                }
            }
            return 0;
        }

        @Nullable
        public Pair<String, Integer> top1() {
            if (portions.size() >= 1) {
                return portions.get(0);
            }
            return null;
        }
    }
}
