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

package com.tencent.matrix.iocanary.config;

/**
 * Created by zhangshaowen on 17/7/13.
 */

public class SharePluginInfo {
    public static final String TAG_PLUGIN = "io";

    public static final class IssueType {
        public static final int ISSUE_UNKNOWN                       = 0x0;

        /**
         *  enum IssueType {
         *     kIssueMainThreadIO = 1,
         *     kIssueSmallBuffer,
         *     kIssueRepeatRead
         *  };
         */
        public static final int ISSUE_IO_CLOSABLE_LEAK              = 0x4;
        public static final int ISSUE_NETWORK_IO_IN_MAIN_THREAD     = 0x5;
        public static final int ISSUE_IO_CURSOR_LEAK                = 0x6;
    }

    public static final String ISSUE_FILE_PATH            = "path";
    public static final String ISSUE_FILE_SIZE            = "size";
    public static final String ISSUE_FILE_COST_TIME       = "cost";
    public static final String ISSUE_FILE_STACK           = "stack";
    public static final String ISSUE_FILE_OP_TIMES        = "op";
    public static final String ISSUE_FILE_BUFFER          = "buffer";
    public static final String ISSUE_FILE_THREAD          = "thread";
    public static final String ISSUE_FILE_READ_WRITE_TYPE = "opType";
    public static final String ISSUE_FILE_OP_SIZE         = "opSize";

    public static final String ISSUE_FILE_REPEAT_COUNT = "repeat";
}
