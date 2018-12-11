package sample.tencent.matrix.trace;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import com.tencent.matrix.util.MatrixLog;

import static sample.tencent.matrix.MatrixApplication.getContext;

public class StartUpBroadcastReceiver extends BroadcastReceiver {
    private static final String TAG = "Matrix.StartUpBroadcastReceiver";

    @Override
    public void onReceive(Context context, Intent intent) {
        MatrixLog.i(TAG, "[onReceive]");
        getContext().startActivity(new Intent(getContext(), TestOtherProcessActivity.class));
    }
}
