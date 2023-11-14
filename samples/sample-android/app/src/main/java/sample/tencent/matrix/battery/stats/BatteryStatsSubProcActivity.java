package sample.tencent.matrix.battery.stats;

import android.os.Bundle;
import android.os.Process;
import android.os.SystemClock;
import android.util.ArrayMap;
import android.view.MenuItem;
import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.PopupMenu;
import android.widget.TextView;

import com.tencent.matrix.batterycanary.BatteryCanary;
import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.stats.BatteryRecord;
import com.tencent.matrix.batterycanary.stats.BatteryStatsFeature;
import com.tencent.matrix.batterycanary.stats.ui.BatteryStatsAdapter.Item.HeaderItem;
import com.tencent.matrix.batterycanary.stats.ui.BatteryStatsLoader;
import com.tencent.matrix.batterycanary.utils.Consumer;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.core.util.Pair;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import sample.tencent.matrix.R;
import sample.tencent.matrix.battery.BatteryCanaryInitHelper;
import sample.tencent.matrix.battery.stats.chart.SimpleLineChart;

public class BatteryStatsSubProcActivity extends AppCompatActivity {
    private static final String TAG = "Matrix.BatteryStatsSubProcActivity";

    @NonNull
    private MyBatteryStatsLoader mStatsLoader;
    @NonNull
    SimpleLineChart mChart;
    @Nullable
    private HeaderItem mCurrHeader;
    private boolean mEnd;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        String proc = com.tencent.matrix.batterycanary.stats.BatteryRecorder.MMKVRecorder.getProcNameSuffix();
        initStatsView(proc);
    }

    private void initStatsView(String proc) {
        setContentView(R.layout.activity_battery_stats_sub);
        Toolbar toolbar = findViewById(R.id.toolbar);
        toolbar.setTitle("电量统计报告");
        setSupportActionBar(toolbar);

        BatteryCanaryInitHelper.startBatteryMonitor(this);

        final TextView procTv = findViewById(R.id.tv_proc);
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

        // Chart
        mChart = findViewById(R.id.chart);
        CheckBox powerCheckBox = findViewById(R.id.cb_power);
        powerCheckBox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                if (isChecked) {
                    mStatsLoader.setFilter(new BatteryStatsLoader.Filter() {
                        @Override
                        public List<BatteryRecord> filtering(List<BatteryRecord> input) {
                            List<BatteryRecord> filtered = new ArrayList<>();
                            for (BatteryRecord item : input) {
                                if (item instanceof BatteryRecord.EventStatRecord) {
                                    if (BatteryRecord.EventStatRecord.EVENT_BATTERY_STAT.equals(((BatteryRecord.EventStatRecord) item).event)) {
                                        filtered.add(item);
                                    }
                                }
                            }
                            return filtered;
                        }
                    });
                } else {
                    mStatsLoader.removeFilter();
                }

                mStatsLoader.reset();
                mStatsLoader.load();
                updateHeader(0);
            }
        });

        // List
        RecyclerView recyclerView = findViewById(R.id.rv_battery_stats);
        final LinearLayoutManager layoutManager = new LinearLayoutManager(this);
        recyclerView.setLayoutManager(layoutManager);
        MyBatteryStatsAdapter adapter = new MyBatteryStatsAdapter();
        recyclerView.setAdapter(adapter);
        adapter.registerAdapterDataObserver(new RecyclerView.AdapterDataObserver() {
            @Override
            public void onChanged() {
                // update header
                int topPosition = layoutManager.findFirstVisibleItemPosition();
                updateHeader(topPosition);
            }
        });
        mStatsLoader = new MyBatteryStatsLoader(adapter);
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
                if (layoutManager.findLastVisibleItemPosition() == mStatsLoader.getDateSet().size() - 1) {
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
        HeaderItem currHeader = mStatsLoader.getFirstHeader(topPosition);
        if (currHeader != null) {
            if (mCurrHeader == null || mCurrHeader != currHeader) {
                mCurrHeader = currHeader;
                View headerView = findViewById(R.id.header_pinned);
                headerView.setVisibility(View.VISIBLE);
                TextView tv = headerView.findViewById(R.id.tv_title);
                tv.setText(currHeader.date);

                List<Pair<Float, Long>> dataSet = mStatsLoader.getCurrPowerDataSet(getApplicationContext(), currHeader.date);
                // mChart.setData(dataSet);
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

        Map<String, Object> extras = new HashMap<>();
        extras.put("String", "String");
        extras.put("Digit", 10086L);
        extras.put("Boolean", true);
        BatteryRecord.EventStatRecord eventStatRecord = new BatteryRecord.EventStatRecord();
        eventStatRecord.event = "Custom Event";
        eventStatRecord.id = 100;
        eventStatRecord.extras = extras;
        records.add(eventStatRecord);

        BatteryStatsFeature.BatteryRecords batteryRecords = new BatteryStatsFeature.BatteryRecords();
        batteryRecords.date = BatteryStatsFeature.getDateString(0);
        batteryRecords.records = records;
        mStatsLoader.add(batteryRecords);
    }
}