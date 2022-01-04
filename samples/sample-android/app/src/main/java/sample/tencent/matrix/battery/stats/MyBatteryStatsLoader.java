package sample.tencent.matrix.battery.stats;

import android.annotation.SuppressLint;
import android.content.Context;

import com.tencent.matrix.batterycanary.BatteryCanary;
import com.tencent.matrix.batterycanary.stats.BatteryRecord;
import com.tencent.matrix.batterycanary.stats.BatteryStatsFeature;
import com.tencent.matrix.batterycanary.stats.ui.BatteryStatsAdapter;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;

import java.util.ArrayList;
import java.util.List;

import androidx.core.util.Pair;

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

    public List<Pair<Float, Long>> getCurrPowerDataSet(Context context, String date) {
        final List<Pair<Float, Long>> dataSet = new ArrayList<>();
        BatteryStatsFeature stats = BatteryCanary.getMonitorFeature(BatteryStatsFeature.class);
        if (stats != null) {
            List<BatteryRecord> batteryRecords = stats.readRecords(date, mProc);
            for (BatteryRecord item : batteryRecords) {
                if (item instanceof BatteryRecord.EventStatRecord) {
                    if (BatteryRecord.EventStatRecord.EVENT_BATTERY_STAT.equals(((BatteryRecord.EventStatRecord) item).event)) {
                        if (((BatteryRecord.EventStatRecord) item).getBoolean("battery-change", false)) {
                            long digit = ((BatteryRecord.EventStatRecord) item).getDigit("battery-pct", -1);
                            if (digit >= 0) {
                                dataSet.add(new Pair<>(digit * 1f, item.millis));
                            }
                        }
                    }
                }
            }
            if (!dataSet.isEmpty()) {
                return dataSet;
            }
        }
        @SuppressLint("RestrictedApi")
        int pct = BatteryCanaryUtil.getBatteryPercentage(context);
        dataSet.add(new Pair<>(pct * 1f, 0L));
        return dataSet;
    }
}
