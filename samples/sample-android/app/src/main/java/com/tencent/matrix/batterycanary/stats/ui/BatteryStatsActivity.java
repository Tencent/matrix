package com.tencent.matrix.batterycanary.stats.ui;

import android.os.Bundle;
import android.os.Process;
import android.os.SystemClock;
import android.util.ArrayMap;
import android.view.View;

import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.stats.BatteryRecord;
import com.tencent.matrix.batterycanary.stats.BatteryStatsFeature;

import java.util.ArrayList;
import java.util.List;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import sample.tencent.matrix.R;

public class BatteryStatsActivity extends AppCompatActivity {

    @NonNull
    private BatteryStatsLoader mStatsLoader;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_battery_stats);

        RecyclerView recyclerView = findViewById(R.id.rv_battery_stats);
        View pinnedHeader = findViewById(R.id.header_pinned);

        recyclerView.setLayoutManager(new LinearLayoutManager(this));
        BatteryStatsAdapter adapter = new BatteryStatsAdapter();
        recyclerView.setAdapter(adapter);
        mStatsLoader = new BatteryStatsLoader(adapter);

        // load today's data
        mStatsLoader.load(0);

        // load mocking data
        loadMockingData();
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
        mStatsLoader.add(batteryRecords);
    }


}