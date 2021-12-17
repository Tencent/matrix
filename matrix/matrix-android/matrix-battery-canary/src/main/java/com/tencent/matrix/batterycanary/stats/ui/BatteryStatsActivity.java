package com.tencent.matrix.batterycanary.stats.ui;

import android.os.Bundle;
import android.view.MenuItem;
import android.view.View;
import android.widget.PopupMenu;
import android.widget.TextView;

import com.tencent.matrix.batterycanary.BatteryCanary;
import com.tencent.matrix.batterycanary.R;
import com.tencent.matrix.batterycanary.stats.BatteryStatsFeature;
import com.tencent.matrix.batterycanary.utils.Consumer;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

public class BatteryStatsActivity extends AppCompatActivity {

    @NonNull
    private BatteryStatsLoader mStatsLoader;
    private BatteryStatsAdapter.HeaderItem mCurrHeader;
    private boolean mEnd;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_battery_stats);

        Toolbar toolbar = findViewById(R.id.toolbar);
        toolbar.setTitle("电量统计报告");
        setSupportActionBar(toolbar);

        final TextView procTv = findViewById(R.id.tv_proc);
        String proc = com.tencent.matrix.batterycanary.stats.BatteryRecorder.MMKVRecorder.getProcNameSuffix();
        procTv.setText(":" + proc);

        BatteryCanary.getMonitorFeature(BatteryStatsFeature.class, new Consumer<BatteryStatsFeature>() {
            @Override
            public void accept(final BatteryStatsFeature batteryStatsFeature) {
                procTv.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(final View v) {
                        PopupMenu menu = new PopupMenu(v.getContext(), procTv);
                        menu.getMenu().add("Process :main");
                        for (String item : batteryStatsFeature.getProcSet()) {
                            if ("main".equals(item)) {
                                continue;
                            }
                            menu.getMenu().add("Process :" + item);
                        }
                        menu.setOnMenuItemClickListener(new PopupMenu.OnMenuItemClickListener() {
                            @Override
                            public boolean onMenuItemClick(MenuItem item) {
                                String title = item.getTitle().toString();
                                if (title.contains(":")) {
                                    String proc = title.substring(title.lastIndexOf(":") + 1);
                                    procTv.setText(":" + proc);
                                    mStatsLoader.reset(proc);
                                    mStatsLoader.load();
                                    updateHeader(0);
                                }
                                return false;
                            }
                        });
                        menu.show();
                    }
                });
            }
        });

        RecyclerView recyclerView = findViewById(R.id.rv_battery_stats);
        final LinearLayoutManager layoutManager = new LinearLayoutManager(this);
        recyclerView.setLayoutManager(layoutManager);
        BatteryStatsAdapter adapter = new BatteryStatsAdapter();
        recyclerView.setAdapter(adapter);
        adapter.registerAdapterDataObserver(new RecyclerView.AdapterDataObserver() {
            @Override
            public void onChanged() {
                // update header
                int topPosition = layoutManager.findFirstVisibleItemPosition();
                updateHeader(topPosition);
            }
        });
        mStatsLoader = new BatteryStatsLoader(adapter);
        // mStatsLoader.setLoadListener(new BatteryStatsLoader.OnLoadListener() {
        //     @Override
        //     public void onLoadFinish(BatteryStatsAdapter adapter) {
        //
        //     }
        // });
        recyclerView.addOnScrollListener(new RecyclerView.OnScrollListener() {
            @Override
            public void onScrolled(@NonNull RecyclerView recyclerView, int dx, int dy) {
                super.onScrolled(recyclerView, dx, dy);
                // update header
                int topPosition = layoutManager.findFirstVisibleItemPosition();
                updateHeader(topPosition);

                // load more
                if (layoutManager.findLastVisibleItemPosition() == mStatsLoader.mStatsAdapter.getDataList().size() - 1) {
                    if (!mStatsLoader.loadMore()) {
                        if (!mEnd) {
                            mEnd = true;
                        }
                    }
                }
            }
        });

        // load today's data
        mStatsLoader.reset(proc);
        mStatsLoader.load();

        // load mocking data
        // loadMockingData();
    }

    private void updateHeader(final int topPosition) {
        BatteryStatsAdapter.HeaderItem currHeader = mStatsLoader.getFirstHeader(topPosition);
        if (currHeader != null) {
            if (mCurrHeader == null || mCurrHeader != currHeader) {
                mCurrHeader = currHeader;
                View headerView = findViewById(R.id.header_pinned);
                headerView.setVisibility(View.VISIBLE);
                TextView tv = headerView.findViewById(R.id.tv_title);
                tv.setText(currHeader.date);
            }
        }
    }
}