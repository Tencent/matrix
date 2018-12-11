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

package sample.tencent.matrix.battery;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import com.tencent.matrix.util.MatrixLog;

/**
 * Created by lolo on 2017/11/15.
 */
public class TestReceiver {
    public static class TestBroadcastReceiver extends BroadcastReceiver {
        private static final String TAG = "Matrix.TestBroadcastReceiver";

        @Override
        public void onReceive(Context context, Intent intent) {
            MatrixLog.d(TAG, "TestBroadcastReceiver onReceive time:%d", System.currentTimeMillis());

        }
    }

    public static class TestBroadcastReceiver2 extends BroadcastReceiver {
        private static final String TAG = "Matrix.TestBroadcastReceiver2";

        @Override
        public void onReceive(Context context, Intent intent) {
            MatrixLog.d(TAG, "TestBroadcastReceiver2 onReceive time:%d", System.currentTimeMillis());

        }
    }

    public static class TestBroadcastReceiver3 extends BroadcastReceiver {
        private static final String TAG = "Matrix.TestBroadcastReceiver3";

        @Override
        public void onReceive(Context context, Intent intent) {
            MatrixLog.d(TAG, "TestBroadcastReceiver3 onReceive time:%d", System.currentTimeMillis());

        }
    }

    public static class TestBroadcastReceiver4 extends BroadcastReceiver {
        private static final String TAG = "Matrix.TestBroadcastReceiver4";

        @Override
        public void onReceive(Context context, Intent intent) {
            MatrixLog.d(TAG, "TestBroadcastReceiver4 onReceive time:%d", System.currentTimeMillis());

        }
    }
}
