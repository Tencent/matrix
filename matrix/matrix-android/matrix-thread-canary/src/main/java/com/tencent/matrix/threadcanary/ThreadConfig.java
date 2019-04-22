package com.tencent.matrix.threadcanary;

import com.tencent.mrs.plugin.IDynamicConfig;

import java.util.HashSet;

public final class ThreadConfig {
    public static final String TAG = "Matrix.ThreadConfig";

    private static final long DEFAULT_CHECK_TIME = 20 * 60 * 1000L;
    private static final long DEFAULT_CHECK_TIME_IN_BACKGROUND = DEFAULT_CHECK_TIME * 2;
    private static final int DEFAULT_LIMIT_THREAD_COUNT = 40;
    private static final long DEFAULT_REPORT_TIME = 2 * 60 * 60 * 1000L;
    private static final String DEFAULT_FILTER_SET = "";

    private final IDynamicConfig mDynamicConfig;

    private ThreadConfig(IDynamicConfig dynamicConfig) {
        this.mDynamicConfig = dynamicConfig;
    }

    public long getCheckTime() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_thread_check_time.name(), DEFAULT_CHECK_TIME);
    }

    public long getCheckBgTime() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_thread_check_bg_time.name(), DEFAULT_CHECK_TIME_IN_BACKGROUND);
    }

    public int getThreadLimitCount() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_thread_limit_count.name(), DEFAULT_LIMIT_THREAD_COUNT);
    }

    public long getReportTime() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_thread_report_time.name(), DEFAULT_REPORT_TIME);
    }


    public HashSet<String> getFilterThreadSet() {
        final String filterSets = mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_thread_filter_thread_set.name(), DEFAULT_FILTER_SET);
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
        private IDynamicConfig mDynamicConfig;

        public Builder() {

        }

        public Builder dynamicConfig(IDynamicConfig config) {
            this.mDynamicConfig = config;
            return this;
        }

        public ThreadConfig build() {
            return new ThreadConfig(mDynamicConfig);
        }
    }
}
