/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.tencent.matrix.hook;

import androidx.annotation.NonNull;

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

    @NonNull
    protected abstract String getNativeLibraryName();

    protected abstract boolean onConfigure();

    protected abstract boolean onHook(boolean enableDebug);
}
