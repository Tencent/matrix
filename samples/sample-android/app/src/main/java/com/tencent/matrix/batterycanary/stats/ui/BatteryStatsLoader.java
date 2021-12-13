package com.tencent.matrix.batterycanary.stats.ui;

import com.tencent.matrix.batterycanary.BatteryCanary;
import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.stats.BatteryRecord;
import com.tencent.matrix.batterycanary.stats.BatteryStatsFeature;
import com.tencent.matrix.batterycanary.utils.Consumer;

import java.util.ArrayList;
import java.util.List;

/**
 * @author Kaede
 * @since 2021/12/10
 */
public class BatteryStatsLoader {
    private static final int DAY_LIMIT = 7;

    final BatteryStatsAdapter mStatsAdapter;
    int mDayOffset = 0;

    public BatteryStatsLoader(BatteryStatsAdapter statsAdapter) {
        mStatsAdapter = statsAdapter;
    }

    public List<BatteryStatsAdapter.Item> getDateSet() {
        return mStatsAdapter.dataList;
    }

    public void load() {
        load(mDayOffset);
    }

    public boolean loadMore() {
        if (Math.abs(mDayOffset) >= DAY_LIMIT) {
            return false;
        }
        mDayOffset--;
        load(mDayOffset);
        return true;
    }

    public BatteryStatsAdapter.HeaderItem getFirstHeader(int topPosition) {
        BatteryStatsAdapter.HeaderItem currHeader = null;
        List<BatteryStatsAdapter.Item> dataList = mStatsAdapter.getDataList();
        for (int i = topPosition; i >= 0; i--) {
            if (topPosition < dataList.size()) {
                if (dataList.get(i) instanceof BatteryStatsAdapter.HeaderItem) {
                    currHeader = (BatteryStatsAdapter.HeaderItem) dataList.get(i);
                    break;
                }
            }
        }
        return currHeader;
    }

    protected void load(final int dayOffset) {
        BatteryCanary.getMonitorFeature(BatteryStatsFeature.class, new Consumer<BatteryStatsFeature>() {
            @Override
            public void accept(BatteryStatsFeature batteryStatsFeature) {
                List<BatteryRecord> records = batteryStatsFeature.readRecords(dayOffset);
                BatteryStatsFeature.BatteryRecords batteryRecords = new BatteryStatsFeature.BatteryRecords();
                batteryRecords.date = BatteryStatsFeature.getDateString(dayOffset);
                batteryRecords.records = records;
                add(batteryRecords);
            }
        });
    }

    public void add(BatteryStatsFeature.BatteryRecords batteryRecords) {
        List<BatteryStatsAdapter.Item> dataList = onCreateDataItem(batteryRecords);

        // Footer
        if (Math.abs(mDayOffset) >= DAY_LIMIT) {
            BatteryStatsAdapter.HeaderItem headerItem = new BatteryStatsAdapter.HeaderItem();
            headerItem.date = "END";
            dataList.add(headerItem);
            BatteryStatsAdapter.NoDataItem footerItem = new BatteryStatsAdapter.NoDataItem();
            footerItem.text = "Only keep last " + DAY_LIMIT + " days' data";
            dataList.add(footerItem);
        }

        int position = mStatsAdapter.getDataList().size() - 1;
        mStatsAdapter.getDataList().addAll(dataList);
        mStatsAdapter.notifyItemRangeChanged(Math.max(position, 0), dataList.size());
    }

    protected List<BatteryStatsAdapter.Item> onCreateDataItem(BatteryStatsFeature.BatteryRecords batteryRecords) {
        List<BatteryStatsAdapter.Item> dataList = new ArrayList<>(batteryRecords.records.size() + 1);

        // Records
        if (batteryRecords.records.isEmpty()) {
            // N0 DATA
            BatteryStatsAdapter.NoDataItem item = new BatteryStatsAdapter.NoDataItem();
            dataList.add(0, item);

        } else {
            // Convert records to list items
            for (BatteryRecord item : batteryRecords.records) {
                if (item instanceof BatteryRecord.ProcStatRecord) {
                    BatteryStatsAdapter.EventLevel1Item eventItem = new BatteryStatsAdapter.EventLevel1Item(item);
                    eventItem.text = (((BatteryRecord.ProcStatRecord) item).procStat == BatteryRecord.ProcStatRecord.STAT_PROC_LAUNCH ? "PROCESS INIT" : "PROCESS_ID_" + ((BatteryRecord.ProcStatRecord) item).procStat)
                            + " (pid " + ((BatteryRecord.ProcStatRecord) item).pid + "）";
                    dataList.add(0, eventItem);
                    continue;
                }

                if (item instanceof BatteryRecord.AppStatRecord) {
                    BatteryStatsAdapter.EventLevel1Item eventItem = new BatteryStatsAdapter.EventLevel1Item(item);
                    switch (((BatteryRecord.AppStatRecord) item).appStat) {
                        case AppStats.APP_STAT_FOREGROUND:
                            eventItem.text = "App 切换到前台";
                            break;
                        case AppStats.APP_STAT_BACKGROUND:
                            eventItem.text = "App 切换到后台";
                            break;
                        case AppStats.APP_STAT_FOREGROUND_SERVICE:
                            eventItem.text = "App 切换到后台 (有前台服务)";
                            break;
                        default:
                            eventItem.text = "App 状态变化: " + ((BatteryRecord.AppStatRecord) item).appStat;
                            break;
                    }
                    dataList.add(0, eventItem);
                    continue;
                }

                if (item instanceof BatteryRecord.DevStatRecord) {
                    BatteryStatsAdapter.EventLevel2Item eventItem = new BatteryStatsAdapter.EventLevel2Item(item);
                    switch (((BatteryRecord.DevStatRecord) item).devStat) {
                        case AppStats.DEV_STAT_CHARGING:
                            eventItem.text = "CHARGE_ON";
                            break;
                        case AppStats.DEV_STAT_UN_CHARGING:
                            eventItem.text = "CHARGE_OFF";
                            break;
                        case AppStats.DEV_STAT_SAVE_POWER_MODE:
                            eventItem.text = "PowerSave ON";
                            break;
                        case AppStats.DEV_STAT_SCREEN_ON:
                            eventItem.text = "SCREEN_ON";
                            break;
                        case AppStats.DEV_STAT_SCREEN_OFF:
                            eventItem.text = "SCREEN_OFF";
                            break;
                        default:
                            eventItem.text = "设备状态变化: " + ((BatteryRecord.DevStatRecord) item).devStat;
                            break;
                    }
                    dataList.add(0, eventItem);
                    continue;
                }

                if (item instanceof BatteryRecord.SceneStatRecord) {
                    BatteryStatsAdapter.EventLevel2Item eventItem = new BatteryStatsAdapter.EventLevel2Item(item);
                    eventItem.text = "UI: " + ((BatteryRecord.SceneStatRecord) item).scene;
                    dataList.add(0, eventItem);
                    continue;
                }

                if (item instanceof BatteryRecord.ReportRecord) {
                    BatteryStatsAdapter.EventDumpItem dumpItem = new BatteryStatsAdapter.EventDumpItem((BatteryRecord.ReportRecord) item);
                    dataList.add(0, dumpItem);
                    continue;
                }

                if (item instanceof BatteryRecord.EventStatRecord) {
                    BatteryStatsAdapter.EventLevel2Item eventItem = new BatteryStatsAdapter.EventLevel2Item(item);
                    eventItem.text = "EVENT: " + ((BatteryRecord.EventStatRecord) item).event;
                    dataList.add(0, eventItem);
                    continue;
                }

                BatteryStatsAdapter.EventLevel2Item eventItem = new BatteryStatsAdapter.EventLevel2Item(item);
                eventItem.text = "Unknown: " + item.getClass().getName();
                dataList.add(0, eventItem);
            }
        }

        // Date
        BatteryStatsAdapter.HeaderItem headerItem = new BatteryStatsAdapter.HeaderItem();
        headerItem.date = batteryRecords.date;
        dataList.add(0, headerItem);

        return dataList;
    }
}
