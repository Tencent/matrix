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

import com.tencent.matrix.report.Issue;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;

public class IssuesMap {

    private static final ConcurrentHashMap<String, List<Issue>> issues = new ConcurrentHashMap<>();
    private static final ArrayList<Issue> allIssues = new ArrayList<>();

    public static void put(@IssueFilter.FILTER String filter, Issue issue) {
        List<Issue> list = issues.get(filter);
        if (list == null) {
            list = new ArrayList<>();
        }
        list.add(0, issue);
        issues.put(filter, list);

        synchronized (allIssues) {
            allIssues.add(issue);
        }
    }

    public static List<Issue> get(@IssueFilter.FILTER String filter) {
        return issues.get(filter);
    }

    public static int getCount() {
        List list = issues.get(IssueFilter.getCurrentFilter());
        return null == list ? 0 : list.size();
    }

    public static void clear() {
        issues.clear();
    }

    public static Issue getIssueReverse(int index) {
        synchronized (allIssues) {
            if (allIssues.size() <= index) {
                return null;
            }

            return allIssues.get(allIssues.size() - index - 1);
        }
    }

    public static int amountOfIssues() {
        synchronized (allIssues) {
            return allIssues.size();
        }
    }

}
