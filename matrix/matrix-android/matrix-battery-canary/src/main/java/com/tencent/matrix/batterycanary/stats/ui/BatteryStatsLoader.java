package com.tencent.matrix.batterycanary.stats.ui;

import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;

import com.tencent.matrix.batterycanary.BatteryCanary;
import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.stats.BatteryRecord;
import com.tencent.matrix.batterycanary.stats.BatteryStatsFeature;
import com.tencent.matrix.batterycanary.stats.BatteryStatsFeature.BatteryRecords;
import com.tencent.matrix.batterycanary.utils.Consumer;
import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;
import java.util.List;

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

    public List<BatteryStatsAdapter.Item> getDateSet() {
        return mStatsAdapter.getDataList();
    }

    public void reset(String proc) {
        mProc = proc;
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

    public BatteryStatsAdapter.Item.HeaderItem getFirstHeader(int topPosition) {
        BatteryStatsAdapter.Item.HeaderItem currHeader = null;
        List<BatteryStatsAdapter.Item> dataList = mStatsAdapter.getDataList();
        for (int i = topPosition; i >= 0; i--) {
            if (topPosition < dataList.size()) {
                if (dataList.get(i) instanceof BatteryStatsAdapter.Item.HeaderItem) {
                    currHeader = (BatteryStatsAdapter.Item.HeaderItem) dataList.get(i);
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
                BatteryRecords batteryRecords = new BatteryRecords();
                batteryRecords.date = BatteryStatsFeature.getDateString(dayOffset);
                batteryRecords.records = records;
                add(batteryRecords);
            }
        });
    }

    public void add(BatteryRecords batteryRecords) {
        List<BatteryStatsAdapter.Item> dataList = onCreateDataSet(batteryRecords);

        // Footer
        if (Math.abs(mDayOffset) >= mDayLimit) {
            BatteryStatsAdapter.Item.HeaderItem headerItem = new BatteryStatsAdapter.Item.HeaderItem();
            headerItem.date = "END";
            dataList.add(headerItem);
            BatteryStatsAdapter.Item.NoDataItem footerItem = new BatteryStatsAdapter.Item.NoDataItem();
            footerItem.text = "Only keep last " + mDayLimit + " days' data";
            dataList.add(footerItem);
        }

        // Notify
        postUpdateDataSet(dataList);
    }

    private void postUpdateDataSet(final List<BatteryStatsAdapter.Item> dataList) {
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

    protected List<BatteryStatsAdapter.Item> onCreateDataSet(BatteryRecords batteryRecords) {
        List<BatteryStatsAdapter.Item> dataList = new ArrayList<>(batteryRecords.records.size() + 1);

        // Records
        if (batteryRecords.records.isEmpty()) {
            // N0 DATA
            BatteryStatsAdapter.Item.NoDataItem item = new BatteryStatsAdapter.Item.NoDataItem();
            item.text = "NO DATA";
            dataList.add(0, item);

        } else {
            // Convert records to list items
            for (BatteryRecord record : batteryRecords.records) {
                BatteryStatsAdapter.Item item = onCreateDataItem(record);
                dataList.add(0, item);
            }
        }

        // Date
        BatteryStatsAdapter.Item.HeaderItem headerItem = new BatteryStatsAdapter.Item.HeaderItem();
        headerItem.date = batteryRecords.date;
        dataList.add(0, headerItem);

        return dataList;
    }

    protected BatteryStatsAdapter.Item onCreateDataItem(BatteryRecord record) {
        if (record instanceof BatteryRecord.ProcStatRecord) {
            BatteryStatsAdapter.Item.EventLevel1Item eventItem = new BatteryStatsAdapter.Item.EventLevel1Item(record);
            String title;
            switch (((BatteryRecord.ProcStatRecord) record).procStat) {
                case BatteryRecord.ProcStatRecord.STAT_PROC_LAUNCH:
                    title = "PROCESS INIT";
                    break;
                case BatteryRecord.ProcStatRecord.STAT_PROC_OFF:
                    title = "PROCESS QUIT";
                    break;
                default:
                    title = "PROCESS_ID_" + ((BatteryRecord.ProcStatRecord) record).procStat;
                    break;
            }
            eventItem.text = title + " (pid " + ((BatteryRecord.ProcStatRecord) record).pid + "）";
            return eventItem;
        }

        if (record instanceof BatteryRecord.AppStatRecord) {
            BatteryStatsAdapter.Item.EventLevel1Item eventItem = new BatteryStatsAdapter.Item.EventLevel1Item(record);
            switch (((BatteryRecord.AppStatRecord) record).appStat) {
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
                    eventItem.text = "App 状态变化: " + ((BatteryRecord.AppStatRecord) record).appStat;
                    break;
            }
            return eventItem;
        }

        if (record instanceof BatteryRecord.DevStatRecord) {
            BatteryStatsAdapter.Item.EventLevel2Item eventItem = new BatteryStatsAdapter.Item.EventLevel2Item(record);
            switch (((BatteryRecord.DevStatRecord) record).devStat) {
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
                    eventItem.text = "设备状态变化: " + ((BatteryRecord.DevStatRecord) record).devStat;
                    break;
            }
            return eventItem;
        }

        if (record instanceof BatteryRecord.SceneStatRecord) {
            BatteryStatsAdapter.Item.EventLevel2Item eventItem = new BatteryStatsAdapter.Item.EventLevel2Item(record);
            eventItem.text = "UI: " + ((BatteryRecord.SceneStatRecord) record).scene;
            return eventItem;
        }

        if (record instanceof BatteryRecord.ReportRecord) {
            return new BatteryStatsAdapter.Item.EventDumpItem((BatteryRecord.ReportRecord) record);
        }

        if (record instanceof BatteryRecord.EventStatRecord) {
            BatteryStatsAdapter.Item.EventSimpleItem eventItem = new BatteryStatsAdapter.Item.EventSimpleItem((BatteryRecord.EventStatRecord) record);
            eventItem.event = ((BatteryRecord.EventStatRecord) record).event;
            return eventItem;
        }

        BatteryStatsAdapter.Item.EventLevel2Item eventItem = new BatteryStatsAdapter.Item.EventLevel2Item(record);
        eventItem.text = "Unknown: " + record.getClass().getName();
        return eventItem;
    }
}
