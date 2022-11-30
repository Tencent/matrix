package sample.tencent.matrix.resource;

import android.app.Activity;
import android.os.Bundle;

import androidx.annotation.Nullable;

import android.widget.TextView;

import com.tencent.matrix.resource.config.SharePluginInfo;
import com.tencent.matrix.resource.processor.ManualDumpProcessor;

import sample.tencent.matrix.R;

/**
 * Created by Yves on 2021/3/24
 */
public class ManualDumpActivity extends Activity {
    private static final String TAG = "ManualDumpActivity";

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_manual_dump);

        ((TextView) findViewById(R.id.leak_activity))
                .setText(getIntent().getStringExtra(SharePluginInfo.ISSUE_ACTIVITY_NAME));
        ((TextView) findViewById(R.id.leak_process))
                .setText(getIntent().getStringExtra(SharePluginInfo.ISSUE_LEAK_PROCESS));

//        final ManualDumpProcessor.ManualDumpData data =
//                getIntent().getParcelableExtra(SharePluginInfo.ISSUE_DUMP_DATA);
//        if (data != null) {
//            ((TextView) findViewById(R.id.reference_chain))
//                    .setText(data.refChain);
//        } else {
//            ((TextView) findViewById(R.id.reference_chain))
//                    .setText(R.string.empty_reference_chain);
//        }
    }
}
