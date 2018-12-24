/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
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

package com.tencent.matrix.iocanary.core;

/**
 * Created by liyongjie on 2017/12/8.
 */

public final class IOIssue {
    public final int type;
    public final String path;
    public final long fileSize;
    public final int opCnt;
    public final long bufferSize;
    public final long opCostTime;
    public final int opType;
    public final long opSize;
    public final String threadName;
    public final String stack;

    public final int repeatReadCnt;

    public IOIssue(int type, String path, long fileSize, int opCnt, long bufferSize, long opCostTime,
                   int opType, long opSize, String threadName, String stack, int repeatReadCnt) {
        this.type = type;
        this.path = path;
        this.fileSize = fileSize;
        this.opCnt = opCnt;
        this.bufferSize = bufferSize;
        this.opCostTime = opCostTime;
        this.opType = opType;
        this.opSize = opSize;
        this.threadName = threadName;
        this.stack = stack;
        this.repeatReadCnt = repeatReadCnt;
    }
}
