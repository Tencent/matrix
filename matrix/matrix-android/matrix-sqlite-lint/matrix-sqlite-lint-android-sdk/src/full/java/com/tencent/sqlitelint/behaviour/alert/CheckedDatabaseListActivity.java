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

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.ListView;
import android.widget.TextView;

import com.tencent.sqlitelint.R;
import com.tencent.sqlitelint.behaviour.persistence.IssueStorage;
import com.tencent.sqlitelint.behaviour.persistence.SQLiteLintDbHelper;
import com.tencent.sqlitelint.util.SLog;
import com.tencent.sqlitelint.util.SQLiteLintUtil;

import java.util.List;

/**
 * @author liyongjie
 */
public class CheckedDatabaseListActivity extends SQLiteLintBaseActivity {
    private static final String TAG = "SQLiteLint.CheckedDatabaseListActivity";

    private ListView mListView;
    private CheckedDatabaseListAdapter mListAdapter;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        SQLiteLintDbHelper.INSTANCE.initialize(this);
        initView();
    }

    @Override
    protected void onResume() {
        super.onResume();
        refreshView();
    }

    @Override
    protected int getLayoutId() {
        return R.layout.activity_checked_database_list;
    }

    private void initView() {
        setTitle(getString(R.string.checked_database_list_title));
        mListView = (ListView) findViewById(R.id.list);
        mListAdapter = new CheckedDatabaseListAdapter(this);
        mListView.setAdapter(mListAdapter);
        mListView.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                String dbPath = (String) parent.getItemAtPosition(position);
                if (SQLiteLintUtil.isNullOrNil(dbPath)) {
                    return;
                }

                Intent intent = new Intent();
                intent.setClass(CheckedDatabaseListActivity.this, CheckResultActivity.class);
                intent.putExtra(CheckResultActivity.KEY_DB_LABEL, dbPath);
                startActivity(intent);
            }
        });
    }

    private void refreshView() {
        List<String> defectiveDbList = IssueStorage.getDbPathList();
        SLog.i(TAG, "refreshView defectiveDbList is %d", defectiveDbList.size());
        mListAdapter.setData(defectiveDbList);
    }

    private static class CheckedDatabaseListAdapter extends BaseAdapter {
        private final LayoutInflater mInflater;
        private List<String> mDefectiveDbList;

        CheckedDatabaseListAdapter(Context context) {
            mInflater = LayoutInflater.from(context);
        }

        public void setData(List<String> defectiveDbList) {
            mDefectiveDbList = defectiveDbList;
            notifyDataSetChanged();
        }

        @Override
        public int getCount() {
            if (mDefectiveDbList == null) {
                return 0;
            }
            return mDefectiveDbList.size();
        }

        @Override
        public String getItem(int position) {
            return mDefectiveDbList.get(position);
        }

        @Override
        public long getItemId(int position) {
            return 0;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            ViewHolder viewHolder;
            if (convertView == null) {
                convertView = mInflater.inflate(R.layout.view_checked_database_item, parent, false);
                viewHolder = new ViewHolder();
                viewHolder.dbPathTv = (TextView) convertView.findViewById(R.id.db_path);
                convertView.setTag(viewHolder);
            } else {
                viewHolder = (ViewHolder) convertView.getTag();
            }
            String dbPath = getItem(position);
            viewHolder.dbPathTv.setText(dbPath);
            return convertView;
        }
    }

    static class ViewHolder {
        public TextView dbPathTv;
    }
}
