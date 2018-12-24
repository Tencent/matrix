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

import android.content.Context;
import android.graphics.Color;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import android.widget.Toast;

import com.tencent.matrix.report.Issue;

import java.lang.ref.WeakReference;
import java.text.SimpleDateFormat;
import java.util.Date;

import sample.tencent.matrix.R;

public class IssuesListActivity extends AppCompatActivity {

    private final static int toastCount = 3;
    private static int currToastCount = 0;
    private RecyclerView recyclerView;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_issue_list);

        setTitle(IssueFilter.getCurrentFilter());
        initRecyclerView();

        if (currToastCount < toastCount) {
            currToastCount++;
            Toast.makeText(this, "click view to hide or show issue detail", Toast.LENGTH_LONG).show();
        }


    }

    public void initRecyclerView() {
        recyclerView = (RecyclerView) findViewById(R.id.recycler_view);
        recyclerView.setLayoutManager(new LinearLayoutManager(this));
        recyclerView.setAdapter(new Adapter(this));
    }

    public static class Adapter extends RecyclerView.Adapter<ViewHolder> {

        WeakReference<Context> context;

        public Adapter(Context context) {
            this.context = new WeakReference<>(context);
        }

        @Override
        public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            View view = LayoutInflater.from(context.get()).inflate(R.layout.item_issue_list, parent, false);

            return new ViewHolder(view);
        }

        @Override
        public void onBindViewHolder(final ViewHolder holder, int position) {

            holder.bindPosition(position);
            final Issue issue = IssuesMap.get(IssueFilter.getCurrentFilter()).get(position);
            holder.bind(issue);

            holder.itemView.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {

                    if (!holder.isShow)
                        holder.showIssue(issue);
                    else
                        holder.hideIssue();
                }
            });
        }

        @Override
        public int getItemCount() {
            return IssuesMap.getCount();
        }
    }


    public static class ViewHolder extends RecyclerView.ViewHolder {

        TextView tvTime, tvTag, tvKey, tvType, tvContent, tvIndex;

        public int position;

        private boolean isShow = true;

        public ViewHolder(View itemView) {
            super(itemView);
            tvTime = (TextView) itemView.findViewById(R.id.item_time);
            tvTag = (TextView) itemView.findViewById(R.id.item_tag);
            tvKey = (TextView) itemView.findViewById(R.id.item_key);
            tvType = (TextView) itemView.findViewById(R.id.item_type);
            tvContent = (TextView) itemView.findViewById(R.id.item_content);
            tvIndex = (TextView) itemView.findViewById(R.id.item_index);
        }

        public void bindPosition(int position) {
            this.position = position;
        }

        public void bind(Issue issue) {

            SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy年MM月dd日 HH:mm:ss:SSS");
            Date date = new Date(Long.parseLong(issue.getContent().optString("time")));
            tvTime.setText("IssueTime -> " + simpleDateFormat.format(date));

            if (TextUtils.isEmpty(issue.getTag())) tvTag.setVisibility(View.GONE);
            else tvTag.setText("TAG -> " + issue.getTag());

            if (TextUtils.isEmpty(issue.getKey())) tvKey.setVisibility(View.GONE);
            else tvKey.setText("KEY -> " + issue.getKey());

            if (issue.getType() == null) tvType.setVisibility(View.GONE);
            else tvType.setText("TYPE -> " + issue.getType());

            tvIndex.setText((IssuesMap.getCount() - position) + "");
            tvIndex.setTextColor(getColor(position));
            if (isShow)
                showIssue(issue);
            else
                hideIssue();
        }

        public void showIssue(Issue issue) {
            tvContent.setText(ParseIssueUtil.parseIssue(issue, true));
            tvContent.setVisibility(View.VISIBLE);
            isShow = true;
        }

        public void hideIssue() {
            tvContent.setVisibility(View.GONE);
            isShow = false;
        }

        public int getColor(int index) {
            switch (index) {
                case 0:
                    return Color.RED;
                case 1:
                    return Color.GREEN;
                case 2:
                    return Color.BLUE;
                default:
                    return Color.GRAY;
            }
        }
    }
}
