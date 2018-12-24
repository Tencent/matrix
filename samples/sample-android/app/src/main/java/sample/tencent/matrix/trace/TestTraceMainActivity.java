/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package sample.tencent.matrix.trace;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Intent;
import android.os.Bundle;
import android.os.Debug;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.plugin.Plugin;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.util.MatrixLog;

import java.util.Random;

import sample.tencent.matrix.R;
import sample.tencent.matrix.issue.IssueFilter;
import sample.tencent.matrix.issue.IssuesMap;

public class TestTraceMainActivity extends Activity {
    private static String TAG = "Matrix.TestTraceMainActivity";

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.test_trace);
        IssueFilter.setCurrentFilter(IssueFilter.ISSUE_TRACE);

        Plugin plugin = Matrix.with().getPluginByClass(TracePlugin.class);
        if(!plugin.isPluginStarted()) {
            MatrixLog.i(TAG, "plugin-trace start");
            plugin.start();
        }
        if(!Matrix.with().getPluginByClass(TracePlugin.class).getEvilMethodTracer().isCreated()) {
            Matrix.with().getPluginByClass(TracePlugin.class).getEvilMethodTracer().onCreate();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Plugin plugin = Matrix.with().getPluginByClass(TracePlugin.class);
        if(plugin.isPluginStarted()) {
            MatrixLog.i(TAG, "plugin-trace stop");
            plugin.stop();
        }
    }

    public void testEnter(View view) {
        Intent intent = new Intent(this, TestEnterActivity.class);
        startActivity(intent);
    }

    public void testFps(View view) {
        Intent intent = new Intent(this, TestFpsActivity.class);
        startActivity(intent);
        overridePendingTransition(R.anim.slide_right_in, 0);
    }

    public void testJank(View view) {
        for (int i = 0; i < 30; i++) {
            tryHeavyMethod();
        }
    }

    public void testANR(View view) {
        try {
            Thread.sleep(5200);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    private void tryHeavyMethod() {
        Debug.getMemoryInfo(new Debug.MemoryInfo());
    }

    private void testMuch(int count) {
        if (count % 2 == 0) {
            System.nanoTime();
        }
    }

}
