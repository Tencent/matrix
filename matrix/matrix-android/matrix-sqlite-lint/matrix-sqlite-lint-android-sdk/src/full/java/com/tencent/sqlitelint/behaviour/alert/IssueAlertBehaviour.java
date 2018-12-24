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

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;

import com.tencent.sqlitelint.R;
import com.tencent.sqlitelint.SQLiteLintIssue;
import com.tencent.sqlitelint.behaviour.BaseBehaviour;
import com.tencent.sqlitelint.behaviour.persistence.IssueStorage;
import com.tencent.sqlitelint.util.SLog;

import java.util.List;

/**
 * The issues will be popped up when published.
 * There's UIs for the management of the issues.
 * And issues are saved locally.
 *
 * @author liyongjie
 *         Created by liyongjie on 2017/6/15.
 */

public class IssueAlertBehaviour extends BaseBehaviour {
    private static final String TAG = "Matrix.IssueAlertBehaviour";

    private final Context mContext;
    private final String mConcernedDbPath;

    private long mLastInsertRowId;

    //TODO non-static better?
    private static Handler sMainHandler;
    static {
        sMainHandler = new Handler(Looper.getMainLooper());
    }

    public IssueAlertBehaviour(Context context, String dbPath) {
        mContext = context;
        mConcernedDbPath = dbPath;
        createShortCut(context);
    }

    @Override
    public void onPublish(List<SQLiteLintIssue> publishedIssues) {
        if (publishedIssues == null || publishedIssues.isEmpty()) {
            return;
        }

        long currentInsertRowId = IssueStorage.getLastRowId();
        if (currentInsertRowId == mLastInsertRowId) {
            SLog.v(TAG, "no new issue");
            return;
        }

        mLastInsertRowId = currentInsertRowId;

        sMainHandler.post(new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent();
                intent.setClass(mContext, CheckResultActivity.class);
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                intent.putExtra(CheckResultActivity.KEY_DB_LABEL, mConcernedDbPath);
                mContext.startActivity(intent);
            }
        });
    }

    private static final String NAME = "SQLiteLint";

    private static void createShortCut(Context context) {
        final ContentResolver cr = context.getContentResolver();
        final Uri contentUri = Uri.parse("content://com.android.launcher2.settings/favorites?notify=true");
        Cursor c = cr.query(contentUri, new String[]{"title", "iconResource"}, "title=?",
                new String[]{NAME}, null);
        if (c != null) {
            int count = c.getCount();
            c.close();
            if (count > 0) {
                return;
            }
        }

        Intent shortcut = new Intent("com.android.launcher.action.INSTALL_SHORTCUT");

        shortcut.putExtra(Intent.EXTRA_SHORTCUT_NAME, NAME);
        shortcut.putExtra("duplicate", false); //不允许重复创建
        Intent shortcutIntent = new Intent(Intent.ACTION_MAIN);
        shortcutIntent.setClassName(context, CheckedDatabaseListActivity.class.getName());
        shortcut.putExtra(Intent.EXTRA_SHORTCUT_INTENT, shortcutIntent);

        //快捷方式的图标
        Intent.ShortcutIconResource iconRes = Intent.ShortcutIconResource.fromContext(context, R.drawable.sqlite_lint_icon);
        shortcut.putExtra(Intent.EXTRA_SHORTCUT_ICON_RESOURCE, iconRes);
        context.sendBroadcast(shortcut);
    }
}
