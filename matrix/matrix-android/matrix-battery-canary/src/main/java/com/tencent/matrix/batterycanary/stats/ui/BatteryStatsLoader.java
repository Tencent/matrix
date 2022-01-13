package com.tencent.matrix.batterycanary.stats.ui;

import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;

import com.tencent.matrix.batterycanary.BatteryCanary;
import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.stats.BatteryRecord;
import com.tencent.matrix.batterycanary.stats.BatteryStatsFeature;
import com.tencent.matrix.batterycanary.stats.BatteryStatsFeature.BatteryRecords;
import com.tencent.matrix.batterycanary.stats.ui.BatteryStatsAdapter.Item;
import com.tencent.matrix.batterycanary.utils.Consumer;
import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;
import java.util.List;

import androidx.annotation.Nullable;

/**
 * @author Kaede
 * @since 2021/12/10
 */
public class BatteryStatsLoader {
    private static final String TAG = "Matrix.battery.loader";
    private static final int DAY_LIMIT = 7;

    protected final BatteryStatsAdapter mStatsAdapter;
    protected final int mDayLimit;
    protected final Handler mUiHandler = new Handler(Looper.getMainLooper());
    protected int mDayOffset = 0;
    protected String mProc = "";

    @Nullable
    protected Filter mFilter;

    public BatteryStatsLoader(BatteryStatsAdapter statsAdapter) {
        this(statsAdapter, DAY_LIMIT);
    }

    public BatteryStatsLoader(BatteryStatsAdapter statsAdapter, int dayLimit) {
        mStatsAdapter = statsAdapter;
        mDayLimit = dayLimit;
    }

    public BatteryStatsAdapter getAdapter() {
        return mStatsAdapter;
    }

    public List<Item> getDateSet() {
        return mStatsAdapter.getDataList();
    }

    public void setFilter(Filter filter) {
        mFilter = filter;
    }

    public void removeFilter() {
        mFilter = null;
    }

    public void reset(String proc) {
        mProc = proc;
        reset();
    }

    public void reset() {
        mDayOffset = 0;
        postClearDataSet();
    }

    public void load() {
        if (TextUtils.isEmpty(mProc)) {
            MatrixLog.w(TAG, "Call #reset first!");
            return;
        }
        load(mDayOffset);
    }

    public boolean loadMore() {
        if (Math.abs(mDayOffset) >= mDayLimit) {
            return false;
        }
        mDayOffset--;
        load(mDayOffset);
        return true;
    }

    public Item.HeaderItem getFirstHeader(int topPosition) {
        Item.HeaderItem currHeader = null;
        List<Item> dataList = mStatsAdapter.getDataList();
        for (int i = topPosition; i >= 0; i--) {
            if (topPosition < dataList.size()) {
                if (dataList.get(i) instanceof Item.HeaderItem) {
                    currHeader = (Item.HeaderItem) dataList.get(i);
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
                List<BatteryRecord> records = batteryStatsFeature.readRecords(dayOffset, mProc);
                if (mFilter != null) {
                    records = mFilter.filtering(records);
                }
                BatteryRecords batteryRecords = new BatteryRecords();
                batteryRecords.date = BatteryStatsFeature.getDateString(dayOffset);
                batteryRecords.records = records;
                add(batteryRecords);
            }
        });
    }

    public void add(BatteryRecords batteryRecords) {
        List<Item> dataList = onCreateDataSet(batteryRecords);

        // Footer
        if (Math.abs(mDayOffset) >= mDayLimit) {
            Item.HeaderItem headerItem = new Item.HeaderItem();
            headerItem.date = "END";
            dataList.add(headerItem);
            Item.NoDataItem footerItem = new Item.NoDataItem();
            footerItem.text = "Only keep last " + mDayLimit + " days' data";
            dataList.add(footerItem);
        }

        // Notify
        postUpdateDataSet(dataList);
    }

    private void postUpdateDataSet(final List<Item> dataList) {
        mUiHandler.post(new Runnable() {
            @Override
            public void run() {
                int start = mStatsAdapter.getDataList().size() - 1;
                int length = dataList.size();
                mStatsAdapter.getDataList().addAll(dataList);
                mStatsAdapter.notifyItemRangeChanged(Math.max(start, 0), length);
            }
        });
    }

    private void postClearDataSet() {
        mUiHandler.post(new Runnable() {
            @Override
            public void run() {
                int length = mStatsAdapter.getDataList().size();
                mStatsAdapter.getDataList().clear();
                mStatsAdapter.notifyItemRangeRemoved(0, length);
            }
        });
    }

    protected List<Item> onCreateDataSet(BatteryRecords batteryRecords) {
        List<Item> dataList = new ArrayList<>(batteryRecords.records.size() + 1);

        // Records
        if (batteryRecords.records.isEmpty()) {
            // N0 DATA
            Item.NoDataItem item = new Item.NoDataItem();
            item.text = "NO DATA";
            dataList.add(0, item);

        } else {
            // Convert records to list items
            for (BatteryRecord record : batteryRecords.records) {
                Item item = onCreateDataItem(record);
                dataList.add(0, item);
            }
        }

        // Date
        Item.HeaderItem headerItem = new Item.HeaderItem();
        headerItem.date = batteryRecords.date;
        dataList.add(0, headerItem);

        return dataList;
    }

    protected Item onCreateDataItem(BatteryRecord record) {
        if (record instanceof BatteryRecord.ProcStatRecord) {
            Item.EventLevel1Item item = new Item.EventLevel1Item(record);
            String title;
            switch (((BatteryRecord.ProcStatRecord) record).procStat) {
                case BatteryRecord.ProcStatRecord.STAT_PROC_LAUNCH:
                    title = "PROCESS_INIT";
                    break;
                case BatteryRecord.ProcStatRecord.STAT_PROC_OFF:
                    title = "PROCESS_QUIT";
                    break;
                default:
                    title = "PROCESS_ID_" + ((BatteryRecord.ProcStatRecord) record).procStat;
                    break;
            }
            item.text = title + " (pid " + ((BatteryRecord.ProcStatRecord) record).pid + "）";
            return item;
        }

        if (record instanceof BatteryRecord.AppStatRecord) {
            Item.EventLevel1Item item = new Item.EventLevel1Item(record);
            switch (((BatteryRecord.AppStatRecord) record).appStat) {
                case AppStats.APP_STAT_FOREGROUND:
                    item.text = "App 切换到前台";
                    break;
                case AppStats.APP_STAT_BACKGROUND:
                    item.text = "App 切换到后台";
                    break;
                case AppStats.APP_STAT_FOREGROUND_SERVICE:
                    item.text = "App 切换到后台 (有前台服务)";
                    break;
                default:
                    item.text = "App 状态变化: " + ((BatteryRecord.AppStatRecord) record).appStat;
                    break;
            }
            return item;
        }

        if (record instanceof BatteryRecord.DevStatRecord) {
            Item.EventLevel2Item item = new Item.EventLevel2Item(record);
            switch (((BatteryRecord.DevStatRecord) record).devStat) {
                case AppStats.DEV_STAT_CHARGING:
                    item.text = "CHARGE_ON";
                    break;
                case AppStats.DEV_STAT_UN_CHARGING:
                    item.text = "CHARGE_OFF";
                    break;
                case AppStats.DEV_STAT_DOZE_MODE_ON:
                    item.text = "低电耗模式(Doze) ON";
                    break;
                case AppStats.DEV_STAT_DOZE_MODE_OFF:
                    item.text = "低电耗模式(Doze) OFF";
                    break;
                case AppStats.DEV_STAT_SAVE_POWER_MODE_ON:
                    item.text = "待机模式(Standby) ON";
                    break;
                case AppStats.DEV_STAT_SAVE_POWER_MODE_OFF:
                    item.text = "待机模式(Standby) OFF";
                    break;
                case AppStats.DEV_STAT_SCREEN_ON:
                    item.text = "SCREEN_ON";
                    break;
                case AppStats.DEV_STAT_SCREEN_OFF:
                    item.text = "SCREEN_OFF";
                    break;
                default:
                    item.text = "设备状态变化: " + ((BatteryRecord.DevStatRecord) record).devStat;
                    break;
            }
            return item;
        }

        if (record instanceof BatteryRecord.SceneStatRecord) {
            Item.EventLevel2Item item = new Item.EventLevel2Item(record);
            item.text = "UI: " + ((BatteryRecord.SceneStatRecord) record).scene;
            return item;
        }

        if (record instanceof BatteryRecord.ReportRecord) {
            return new Item.EventDumpItem((BatteryRecord.ReportRecord) record);
        }

        if (record instanceof BatteryRecord.EventStatRecord) {
            if (BatteryRecord.EventStatRecord.EVENT_BATTERY_STAT.equals(((BatteryRecord.EventStatRecord) record).event)) {
                return new Item.EventBatteryItem((BatteryRecord.EventStatRecord) record);
            }
            return new Item.EventSimpleItem((BatteryRecord.EventStatRecord) record);
        }

        Item.EventLevel2Item item = new Item.EventLevel2Item(record);
        item.text = "Unknown: " + record.getClass().getName();
        return item;
    }

    public interface Filter {
        List<BatteryRecord> filtering(List<BatteryRecord> input);
    }
}
