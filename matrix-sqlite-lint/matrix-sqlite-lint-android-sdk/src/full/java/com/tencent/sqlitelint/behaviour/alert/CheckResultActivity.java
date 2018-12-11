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

import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.ListView;
import android.widget.TextView;

import com.tencent.sqlitelint.SQLiteLintIssue;
import com.tencent.sqlitelint.R;
import com.tencent.sqlitelint.behaviour.persistence.IssueStorage;
import com.tencent.sqlitelint.util.SLog;
import com.tencent.sqlitelint.util.SQLiteLintUtil;

import java.util.List;

/**
 * @author liyongjie
 */
public class CheckResultActivity extends SQLiteLintBaseActivity {

    private static final String TAG = "MpApp.CheckResultActivity";

    public static final String KEY_DB_LABEL = "db_label";

    private String mDbLabel;
    private List<SQLiteLintIssue> mCheckResultList;
    private CheckResultListAdapter mAdapter;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mDbLabel = getIntent().getStringExtra(KEY_DB_LABEL);
        initView();
        refreshData();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        refreshData();
    }

    private void refreshData() {
        List<SQLiteLintIssue> issueList = IssueStorage.getIssueListByDb(mDbLabel);
        if (mCheckResultList == null) {
            mCheckResultList = issueList;
        } else {
            mCheckResultList.clear();
            mCheckResultList.addAll(issueList);
        }
        SLog.d(TAG, "refreshData size %d", mCheckResultList.size());
        mAdapter.notifyDataSetChanged();
    }

    private void initView() {
        String dbName = SQLiteLintUtil.extractDbName(mDbLabel);
        setTitle(getString(R.string.check_result_title, dbName));

        ListView listView = (ListView) findViewById(R.id.list);
        mAdapter = new CheckResultListAdapter();
        listView.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                SQLiteLintIssue issue = (SQLiteLintIssue) parent.getItemAtPosition(position);
                Intent intent = new Intent();
                intent.putExtra(IssueDetailActivity.KEY_ISSUE, issue);
                intent.setClass(getBaseContext(), IssueDetailActivity.class);
                startActivity(intent);
            }
        });

        listView.setAdapter(mAdapter);
        mAdapter.notifyDataSetChanged();
    }

    @Override
    protected int getLayoutId() {
        return R.layout.activity_check_result;
    }

    class CheckResultListAdapter extends BaseAdapter {
        private final LayoutInflater mInflater;

        CheckResultListAdapter() {
            mInflater = LayoutInflater.from(CheckResultActivity.this);
        }

        @Override
        public int getCount() {
            if (mCheckResultList == null) {
                return 0;
            }
            return mCheckResultList.size();
        }

        @Override
        public SQLiteLintIssue getItem(int position) {
            return mCheckResultList.get(position);
        }

        @Override
        public long getItemId(int position) {
            return 0;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            ViewHolder viewHolder;
            if (convertView == null) {
                convertView = mInflater.inflate(R.layout.view_check_result_item, parent, false);
                viewHolder = new ViewHolder();
                viewHolder.checkResultTv = (TextView) convertView.findViewById(R.id.result_tv);
                viewHolder.diagnosisLevelTv = (TextView) convertView.findViewById(R.id.diagnosis_level_tv);
                viewHolder.timeTv = (TextView) convertView.findViewById(R.id.time_tv);
                convertView.setTag(viewHolder);
            } else {
                viewHolder = (ViewHolder) convertView.getTag();
            }
            SQLiteLintIssue issue = getItem(position);
            viewHolder.checkResultTv.setText(String.format("%d、%s", position + 1, issue.desc));
            viewHolder.timeTv.setText(SQLiteLintUtil.formatTime(SQLiteLintUtil.YYYY_MM_DD_HH_mm, issue.createTime));
            viewHolder.diagnosisLevelTv.setText(SQLiteLintIssue.getLevelText(issue.level, getBaseContext()));
            return convertView;
        }
    }

    static class ViewHolder {
        public TextView checkResultTv;
        public TextView diagnosisLevelTv;
        public TextView timeTv;
    }
}
