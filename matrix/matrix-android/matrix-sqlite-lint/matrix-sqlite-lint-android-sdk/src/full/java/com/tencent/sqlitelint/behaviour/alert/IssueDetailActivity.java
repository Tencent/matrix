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

package com.tencent.sqlitelint.behaviour.alert;

import android.os.Bundle;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.tencent.sqlitelint.SQLiteLintIssue;
import com.tencent.sqlitelint.R;
import com.tencent.sqlitelint.util.SLog;
import com.tencent.sqlitelint.util.SQLiteLintUtil;

/**
 * @author liyongjie
 *         Created by liyongjie on 17/1/24.
 */

public class IssueDetailActivity extends SQLiteLintBaseActivity {
    private static final String TAG = "MicroMsg.IssueDetailActivity";
    public static final String KEY_ISSUE = "issue";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        initView((SQLiteLintIssue) getIntent().getParcelableExtra(KEY_ISSUE));
    }

    private void initView(final SQLiteLintIssue issue) {
        if (issue == null) {
            return;
        }

        setTitle(getString(R.string.diagnosis_detail_title));
        TextView timeTv = (TextView) findViewById(R.id.time_tv);
        TextView diagnosisLevelTv = (TextView) findViewById(R.id.diagnosis_level_tv);
        timeTv.setText(SQLiteLintUtil.formatTime(SQLiteLintUtil.YYYY_MM_DD_HH_mm, issue.createTime));
        diagnosisLevelTv.setText(SQLiteLintIssue.getLevelText(issue.level, getBaseContext()));


        if (!SQLiteLintUtil.isNullOrNil(issue.desc)) {
            LinearLayout descLayout = (LinearLayout) findViewById(R.id.desc_layout);
            TextView descTv = (TextView) findViewById(R.id.desc_tv);
            descTv.setText(issue.desc);
            descLayout.setVisibility(View.VISIBLE);

            descLayout.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    SLog.v(TAG, issue.desc.replace("%", "###"));
                }
            });
        }

        if (!SQLiteLintUtil.isNullOrNil(issue.detail)) {
            LinearLayout detailLayout = (LinearLayout) findViewById(R.id.detail_layout);
            TextView detailTv = (TextView) findViewById(R.id.detail_tv);
            detailTv.setText(issue.detail);
            detailLayout.setVisibility(View.VISIBLE);

            detailTv.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    SLog.v(TAG, issue.detail.replace("%", "###"));
                }
            });
        }

        if (!SQLiteLintUtil.isNullOrNil(issue.advice)) {
            LinearLayout adviceLayout = (LinearLayout) findViewById(R.id.advice_layout);
            TextView adviceTv = (TextView) findViewById(R.id.advice_tv);
            adviceTv.setText(issue.advice);
            adviceLayout.setVisibility(View.VISIBLE);
        }

        if (!SQLiteLintUtil.isNullOrNil(issue.extInfo)) {
            LinearLayout adviceLayout = (LinearLayout) findViewById(R.id.ext_info_layout);
            TextView extInfoTv = (TextView) findViewById(R.id.ext_info_tv);
            extInfoTv.setText(getString(R.string.diagnosis_ext_info_title, issue.extInfo));
            adviceLayout.setVisibility(View.VISIBLE);
        }
    }

    @Override
    protected int getLayoutId() {
        return R.layout.activity_diagnosis_detail;
    }
}
