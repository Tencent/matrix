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

package sample.tencent.matrix.listener;

import android.app.Application;
import android.content.Context;
import android.content.Intent;

import com.tencent.matrix.plugin.DefaultPluginListener;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.util.MatrixLog;

import java.lang.ref.SoftReference;

import sample.tencent.matrix.issue.IssueFilter;
import sample.tencent.matrix.issue.IssuesListActivity;
import sample.tencent.matrix.issue.IssuesMap;

/**
 * Created by zhangshaowen on 17/6/15.
 */

public class TestPluginListener extends DefaultPluginListener {

    public static final String TAG = "TestPluginListener";

    public SoftReference<Context> softReference;

    public TestPluginListener(Context context) {
        super(context);
        softReference = new SoftReference<>(context);
    }

    @Override
    public void onReportIssue(Issue issue) {
        super.onReportIssue(issue);
        MatrixLog.e(TAG, issue.toString());

        IssuesMap.put(IssueFilter.getCurrentFilter(), issue);
        jumpToIssueActivity();
    }

    public void jumpToIssueActivity() {
        Context context = softReference.get();
        Intent intent = new Intent(context, IssuesListActivity.class);

        if (context instanceof Application) {
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        }

        context.startActivity(intent);

    }

}
