package com.tencent.matrix.batterycanary.stats.ui;

import android.os.Bundle;
import android.os.Process;
import android.os.SystemClock;
import android.util.ArrayMap;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.PopupMenu;
import android.widget.TextView;
import android.widget.Toast;

import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.stats.BatteryRecord;
import com.tencent.matrix.batterycanary.stats.BatteryStatsFeature;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import sample.tencent.matrix.R;

import static sample.tencent.matrix.MatrixApplication.getContext;

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

        procTv.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(final View v) {
                List<String> procs = Arrays.asList("main", "appbrand", "tools", "toolsmp", "push");
                PopupMenu menu = new PopupMenu(v.getContext(), procTv);
                for (String item : procs) {
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

        RecyclerView recyclerView = findViewById(R.id.rv_battery_stats);
        final LinearLayoutManager layoutManager = new LinearLayoutManager(this);
        recyclerView.setLayoutManager(layoutManager);
        BatteryStatsAdapter adapter = new BatteryStatsAdapter();
        recyclerView.setAdapter(adapter);
        mStatsLoader = new BatteryStatsLoader(adapter);
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
        updateHeader(0);

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

    private void loadMockingData() {
        List<BatteryRecord> records = new ArrayList<>();
        BatteryRecord.ProcStatRecord procStatRecord = new BatteryRecord.ProcStatRecord();
        procStatRecord.pid = Process.myPid();
        records.add(procStatRecord);

        BatteryRecord.AppStatRecord appStat = new BatteryRecord.AppStatRecord();
        appStat.appStat = AppStats.APP_STAT_FOREGROUND;
        records.add(appStat);

        BatteryRecord.SceneStatRecord sceneStatRecord = new BatteryRecord.SceneStatRecord();
        sceneStatRecord.scene = "Activity 1";
        records.add(sceneStatRecord);
        sceneStatRecord = new BatteryRecord.SceneStatRecord();
        sceneStatRecord.scene = "Activity 2";
        records.add(sceneStatRecord);
        sceneStatRecord = new BatteryRecord.SceneStatRecord();
        sceneStatRecord.scene = "Activity 3";
        records.add(sceneStatRecord);

        appStat = new BatteryRecord.AppStatRecord();
        appStat.appStat = AppStats.APP_STAT_BACKGROUND;
        records.add(appStat);

        BatteryRecord.DevStatRecord devStatRecord = new BatteryRecord.DevStatRecord();
        devStatRecord.devStat = AppStats.DEV_STAT_CHARGING;
        records.add(devStatRecord);
        devStatRecord = new BatteryRecord.DevStatRecord();
        devStatRecord.devStat = AppStats.DEV_STAT_UN_CHARGING;
        records.add(devStatRecord);

        appStat = new BatteryRecord.AppStatRecord();
        appStat.appStat = AppStats.APP_STAT_FOREGROUND;
        records.add(appStat);

        BatteryRecord.ReportRecord reportRecord = new BatteryRecord.ReportRecord();
        reportRecord.threadInfoList = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
            BatteryRecord.ReportRecord.ThreadInfo threadInfo = new BatteryRecord.ReportRecord.ThreadInfo();
            threadInfo.stat = "R" + i;
            threadInfo.tid = 10000 + i;
            threadInfo.name = "ThreadName_" + i;
            threadInfo.jiffies = SystemClock.currentThreadTimeMillis() / 10;
            reportRecord.threadInfoList.add(0, threadInfo);
        }
        reportRecord.entryList = new ArrayList<>();
        for (int i = 0; i < 4; i++) {
            BatteryRecord.ReportRecord.EntryInfo entryInfo = new BatteryRecord.ReportRecord.EntryInfo();
            entryInfo.name = "Entry Name " + i;
            entryInfo.entries = new ArrayMap<>();
            entryInfo.entries.put("Key 1", "Value 1");
            entryInfo.entries.put("Key 2", "Value 2");
            entryInfo.entries.put("Key 3", "Value 3");
            reportRecord.entryList.add(entryInfo);
        }
        records.add(reportRecord);

        BatteryStatsFeature.BatteryRecords batteryRecords = new BatteryStatsFeature.BatteryRecords();
        batteryRecords.date = BatteryStatsFeature.getDateString(0);
        batteryRecords.records = records;
        mStatsLoader.add(batteryRecords, true);
    }


}