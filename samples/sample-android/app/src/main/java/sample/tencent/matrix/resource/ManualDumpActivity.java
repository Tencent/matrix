package sample.tencent.matrix.resource;

import android.app.Activity;
import android.os.Bundle;
import androidx.annotation.Nullable;
import android.util.Log;
import android.view.View;

import com.tencent.matrix.resource.config.SharePluginInfo;
import com.tencent.matrix.resource.processor.ManualDumpProcessor;

import sample.tencent.matrix.R;

/**
 * Created by Yves on 2021/3/24
 */
public class ManualDumpActivity extends Activity {
    private static final String TAG = "ManualDumpActivity";

    private String mRefString;
    private String mLeakedActivity;
    private String mLeakProcess;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_manual_dump);

        mLeakedActivity = getIntent().getStringExtra(SharePluginInfo.ISSUE_ACTIVITY_NAME);
        mRefString = getIntent().getStringExtra(SharePluginInfo.ISSUE_REF_KEY);
        mLeakProcess = getIntent().getStringExtra(SharePluginInfo.ISSUE_LEAK_PROCESS);
    }

    public void dump(View view) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                ManualDumpProcessor.ManualDumpProcessorHelper.dumpAndAnalyse(ManualDumpActivity.this, mLeakProcess, mLeakedActivity, mRefString, new ManualDumpProcessor.IResultListener() {

                    @Override
                    public void onSuccess(String hprof, String leakReference) {
                        Log.d(TAG, "onSuccess: " + hprof);
                        Log.d(TAG, "onSuccess: "+ leakReference);
                    }

                    @Override
                    public void onFailed() {

                    }
                });
            }
        }).start();
    }
}
