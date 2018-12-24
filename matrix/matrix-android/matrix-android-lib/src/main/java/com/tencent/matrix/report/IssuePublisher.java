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

package com.tencent.matrix.report;

import java.util.HashSet;

/**
 * Manage the logic for publish the issue to the listener.
 * Only one listener is accepted.
 *
 * @author liyongjie
 *         Created by liyongjie on 2017/6/6.
 */

public class IssuePublisher {

    private final OnIssueDetectListener    mIssueListener;
    private final HashSet<String> mPublishedMap;

    public interface OnIssueDetectListener {
        void onDetectIssue(Issue issue);
    }

    public IssuePublisher(OnIssueDetectListener issueDetectListener) {
        mPublishedMap = new HashSet<>();
        this.mIssueListener = issueDetectListener;
    }

    protected void publishIssue(Issue issue) {
        if (mIssueListener == null) {
            throw new RuntimeException("publish issue, but issue listener is null");
        }
        if (issue != null) {
            mIssueListener.onDetectIssue(issue);
        }
    }

    protected boolean isPublished(String key) {
        if (key == null) {
            return false;
        }

        return mPublishedMap.contains(key);
    }

    protected void markPublished(String key) {
        if (key == null) {
            return;
        }

        mPublishedMap.add(key);
    }

    protected void unMarkPublished(String key) {
        if (key == null) {
            return;
        }
        mPublishedMap.remove(key);
    }
}
