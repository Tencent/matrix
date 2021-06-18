package com.tencent.matrix.hook;

import androidx.annotation.Nullable;

/**
 * Created by Yves on 2020-03-18
 */
public abstract class AbsHook {
    private Status mStatus = Status.UNCOMMIT;

    public enum Status {
        UNCOMMIT,
        COMMIT_SUCCESS,
        COMMIT_FAIL_ON_LOAD_LIB,
        COMMIT_FAIL_ON_CONFIGURE,
        COMMIT_FAIL_ON_HOOK
    }

    void setStatus(Status status) {
        mStatus = status;
    }

    public Status getStatus() {
        return mStatus;
    }

    @Nullable
    protected abstract String getNativeLibraryName();

    protected abstract boolean onConfigure();

    protected abstract boolean onHook(boolean enableDebug);
}
