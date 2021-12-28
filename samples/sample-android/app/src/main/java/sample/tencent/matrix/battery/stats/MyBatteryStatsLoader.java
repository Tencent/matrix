package sample.tencent.matrix.battery.stats;

import com.tencent.matrix.batterycanary.stats.BatteryRecord;
import com.tencent.matrix.batterycanary.stats.ui.BatteryStatsAdapter;

/**
 * @author Kaede
 * @since 2021/12/10
 */
public class MyBatteryStatsLoader extends com.tencent.matrix.batterycanary.stats.ui.BatteryStatsLoader {
    private static final String TAG = "Matrix.battery.MyBatteryStatsLoader";

    public MyBatteryStatsLoader(BatteryStatsAdapter statsAdapter) {
        super(statsAdapter);
    }

    @Override
    protected BatteryStatsAdapter.Item onCreateDataItem(BatteryRecord record) {
        if (record instanceof BatteryRecord.EventStatRecord) {
            if ("Custom Event".equals(((BatteryRecord.EventStatRecord) record).event)) {
                BatteryStatsAdapter.Item.EventSimpleItem eventItem = new BatteryStatsAdapter.Item.EventSimpleItem((BatteryRecord.EventStatRecord) record);
                eventItem.event = "自定义事件";
                return eventItem;
            }
        }
        return super.onCreateDataItem(record);
    }
}
