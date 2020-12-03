package com.tencent.matrix.threadcanary;

import java.util.HashSet;

public final class ThreadMonitorConfig {
    public static final String TAG = "Matrix.ThreadMonitorConfig";

    private static final long DEFAULT_CHECK_TIME = 10 * 60 * 1000L;
    private static final long DEFAULT_CHECK_TIME_IN_BACKGROUND = DEFAULT_CHECK_TIME * 2;
    private static final int DEFAULT_LIMIT_THREAD_COUNT = 40;
    private static final long DEFAULT_REPORT_TIME = 30 * 60 * 1000L;
    private static final String DEFAULT_FILTER_SET = "";

    private long checkTime = DEFAULT_CHECK_TIME;
    private long checkBgTime = DEFAULT_CHECK_TIME_IN_BACKGROUND;
    private int threadLimitCount = DEFAULT_LIMIT_THREAD_COUNT;
    private long reportTime = DEFAULT_REPORT_TIME;
    private String filterThreadSet = DEFAULT_FILTER_SET;


    public long getCheckTime() {
        return checkTime;
    }

    public long getCheckBgTime() {
        return checkBgTime;
    }

    public int getThreadLimitCount() {
        return threadLimitCount;
    }

    public long getReportTime() {
        return reportTime;
    }


    public HashSet<String> getFilterThreadSet() {
        final String filterSets = filterThreadSet;
        if ("".equals(filterSets)) {
            return null;
        }

        final String[] vecFilterSets = filterSets.split(";");
        HashSet<String> result = new HashSet<>();
        for (String filterThread : vecFilterSets) {
            if ("".equals(filterThread)) {
                continue;
            }

            result.add(filterThread);
        }

        return result;
    }

    public static final class Builder {

        ThreadMonitorConfig config = new ThreadMonitorConfig();

        public Builder() {

        }

        public Builder setCheckTime(long checkTime) {
            config.checkTime = checkTime;
            return this;
        }

        public Builder setCheckBgTime(long checkBgTime) {
            config.checkBgTime = checkBgTime;
            return this;
        }

        public Builder setReportTime(long reportTime) {
            config.reportTime = reportTime;
            return this;
        }

        public Builder setThreadLimitCount(int threadLimitCount) {
            config.threadLimitCount = threadLimitCount;
            return this;
        }

        public Builder setFilterThreadSet(String filterThreadSet) {
            config.filterThreadSet = filterThreadSet;
            return this;
        }

        public ThreadMonitorConfig build() {
            return config;
        }
    }
}
