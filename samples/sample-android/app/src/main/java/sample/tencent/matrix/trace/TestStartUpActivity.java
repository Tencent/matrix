package sample.tencent.matrix.trace;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.Nullable;

import com.tencent.matrix.util.MatrixLog;

import static sample.tencent.matrix.MatrixApplication.getContext;

public class TestStartUpActivity extends Activity {

    private static final String TAG = "TestStartUpActivity";
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
//        sendBroadcast(new Intent(this, StartUpBroadcastReceiver.class));
        startService(new Intent(this, StartUpService.class));
//        callByContentResolver();
    }

    public void callByContentResolver() {
        ContentResolver contentResolver = getContext().getContentResolver();
        Uri uri = Uri.parse("content://sample.tencent.matrix.trace.StartUpContentProvider/user");
        Bundle bundle = contentResolver.call(uri, "method", null, null);
        String returnCall = bundle.getString("returnCall");
        MatrixLog.i(TAG, "[callByContentResolver] returnCall:%s", returnCall);
    }
}
