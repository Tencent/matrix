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

package sample.tencent.matrix.resource;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Bundle;
import android.support.annotation.Nullable;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.plugin.Plugin;
import com.tencent.matrix.resource.ResourcePlugin;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;

import sample.tencent.matrix.R;
import sample.tencent.matrix.issue.IssueFilter;

/**
 * Created by zhangshaowen on 17/6/13.
 */

public class TestLeakActivity extends Activity {
    private static final String TAG = "Matrix.TestLeakActivity";

    private static Set<Activity> testLeaks = new HashSet<>();

    private static ArrayList<Bitmap> bitmaps = new ArrayList<>();

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        SharedPreferences sharedPreferences = getSharedPreferences("memory" + MatrixUtil.getProcessName(TestLeakActivity.this), Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = sharedPreferences.edit();
        editor.clear();
        editor.commit();

        testLeaks.add(this);
        Plugin plugin = Matrix.with().getPluginByClass(ResourcePlugin.class);
        if (!plugin.isPluginStarted()) {
            MatrixLog.i(TAG, "plugin-resource start");
            plugin.start();
        }

        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inSampleSize = 2;
        bitmaps.add(BitmapFactory.decodeResource(getResources(), R.drawable.welcome_bg, options));
        MatrixLog.i(TAG, "test leak activity size: %d, bitmaps size: %d", testLeaks.size(), bitmaps.size());

        setContentView(R.layout.test_leak);

        IssueFilter.setCurrentFilter(IssueFilter.ISSUE_LEAK);
    }

    @Override
    protected void onStart() {
        super.onStart();
    }

    @Override
    protected void onRestart() {
        super.onRestart();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        MatrixLog.i(TAG, "test leak activity destroy:" + this.hashCode());

        new Thread(new Runnable() {
            @Override
            public void run() {
                Runtime.getRuntime().gc();
                Runtime.getRuntime().runFinalization();
                Runtime.getRuntime().gc();
            }
        }).start();
    }
}
