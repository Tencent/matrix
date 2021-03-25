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

package com.tencent.sqlitelint.behaviour.alert;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import androidx.appcompat.widget.Toolbar;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import com.tencent.sqlitelint.R;

/**
 * @author liyongjie
 *         Created by liyongjie on 17/1/23.
 */

public abstract class SQLiteLintBaseActivity extends Activity {
    private Toolbar mToolBar;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        onCreateView();
    }

    protected void onCreateView() {
        setContentView(R.layout.activity_sqlitelint_base);
        FrameLayout contentLayout = (FrameLayout) findViewById(R.id.content);
        LayoutInflater layoutInflater = LayoutInflater.from(this);
        int layoutId = getLayoutId();
        assert layoutId != 0;
        layoutInflater.inflate(layoutId, contentLayout);

        mToolBar = (Toolbar) findViewById(R.id.toolbar);
        mToolBar.setNavigationOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                onBackBtnClick();
            }
        });
        Drawable drawable = mToolBar.getLogo();
        if (drawable != null) {
            drawable.setVisible(false, true);
        }
    }

    protected void setTitle(String title) {
        mToolBar.setTitle(title);
    }

    protected void onBackBtnClick() {
        finish();
    }

    protected abstract int getLayoutId();
}
