package sample.tencent.matrix.battery.stats;

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
}
