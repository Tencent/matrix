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

package sample.tencent.matrix.issue;

import android.support.annotation.StringDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

public class IssueFilter {

    @StringDef({ISSUE_IO, ISSUE_LEAK, ISSUE_TRACE, ISSUE_SQLITELINT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface FILTER {
    }

    public static final String ISSUE_IO = "ISSUE_IO";
    public static final String ISSUE_LEAK = "ISSUR_LEAK";
    public static final String ISSUE_TRACE = "ISSUR_TRACE";
    public static final String ISSUE_SQLITELINT = "ISSUR_SQLITELINT";

    @FILTER
    private static String CURRENT_FILTER = ISSUE_IO;


    public static void setCurrentFilter(@FILTER String filter) {
        CURRENT_FILTER = filter;
    }

    @FILTER
    public static String getCurrentFilter() {
        return CURRENT_FILTER;
    }
}
