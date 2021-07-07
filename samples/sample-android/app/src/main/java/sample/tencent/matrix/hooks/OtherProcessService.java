package sample.tencent.matrix.hooks;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

import androidx.annotation.Nullable;

import com.tencent.matrix.backtrace.WeChatBacktrace;

public class OtherProcessService extends Service {

    private final static String TAG = "Matrix.OtherProcess";

    boolean mInit = false;

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {

        Log.e(TAG, "Service started");

        if (mInit) {
            return null;
        }

        mInit = true;

        WeChatBacktrace.instance().configure(getApplicationContext()).commit();

        return null;
    }

    public void onDestroy() {
        Log.e(TAG, "Service onDestroy");
    }
}
