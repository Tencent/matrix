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

package com.tencent.matrix.trace.util;

import android.view.View;
import android.view.ViewGroup;

/**
 * Created by caichongyang on 2017/7/10.
 */

public class ViewUtil {

    public static ViewInfo dumpViewInfo(View view) {
        ViewInfo info = new ViewInfo();
        traversalViewTree(info, 0, view);
        return info;
    }

    private static void traversalViewTree(ViewInfo info, int deep, View view) {

        if (view == null) {
            return;
        }

        deep++;

        if (deep > info.mViewDeep) {
            info.mViewDeep = deep;
        }

        if (!(view instanceof ViewGroup)) {
            return;
        }

        ViewGroup grp = (ViewGroup) view;
        final int n = grp.getChildCount();

        if (n <= 0) {
            return;
        }

//        info.mViewCount += N;

        for (int i = 0; i < n; i++) {
            View v = grp.getChildAt(i);
            if (null != v && v.getVisibility() == View.GONE) {
                continue;
            }
            info.mViewCount++;
            traversalViewTree(info, deep, v);
        }

    }

    public static class ViewInfo {
        public int mViewCount = 0;
        public int mViewDeep = 0;
        public String mActivityName = "";

        @Override
        public String toString() {
            return "ViewCount:" + mViewCount + "," + "ViewDeep:" + mViewDeep + "," + "mActivityName:" + mActivityName;
        }
    }

}
