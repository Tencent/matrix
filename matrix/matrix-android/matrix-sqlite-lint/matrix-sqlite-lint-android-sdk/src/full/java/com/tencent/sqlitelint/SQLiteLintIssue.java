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

package com.tencent.sqlitelint;

import android.content.Context;
import android.os.Parcel;
import android.os.Parcelable;
import android.support.annotation.NonNull;

/**
 * @author liyongjie
 *         Created by liyongjie on 16/10/12.
 */

public class SQLiteLintIssue implements Parcelable {
    public static final int PASS = 0;
    public static final int TIPS = 1;
    public static final int SUGGESTION = 2;
    public static final int WARNING = 3;
    public static final int ERROR = 4;

    @NonNull
    public static String getLevelText(int level, @NonNull Context context) {
        String levelText = "";
        switch (level) {
            case TIPS:
                levelText = context.getString(R.string.diagnosis_level_tips);
                break;
            case SUGGESTION:
                levelText = context.getString(R.string.diagnosis_level_suggestion);
                break;
            case WARNING:
                levelText = context.getString(R.string.diagnosis_level_warning);
                break;
            case ERROR:
                levelText = context.getString(R.string.diagnosis_level_error);
                break;
            default:
                levelText = context.getString(R.string.diagnosis_level_suggestion);
                break;
        }
        return levelText;
    }

    public String id;
    public String dbPath;
    public int level;
    /**
     * typedef enum {
         kExplainQueryScanTable = 1,
         kExplainQueryUseTempTree,
         kExplainQueryTipsForLargerIndex,
         kAvoidAutoIncrement,
         kAvoidSelectAllChecker,
         kWithoutRowIdBetter,
         kPreparedStatementBetter,
         kRedundantIndex,
     } IssueType;
     */
    public int type;
    public String sql;
    public String table;
    public String desc;
    public String detail;
    public String advice;
    public long createTime;
    public String extInfo;
    public long sqlTimeCost;
    public boolean isNew;
    public boolean isInMainThread;

    public SQLiteLintIssue() {
    }

    public SQLiteLintIssue(String id, String dbPath, int level, int type, String sql, String table,
                           String desc, String detail, String advice, long createTime, String extInfo,
                           long sqlTimeCost, boolean isInMainThread) {
        this.id = id;
        this.dbPath = dbPath;
        this.level = level;
        this.type = type;
        this.sql = sql;
        this.table = table;
        this.desc = desc;
        this.detail = detail;
        this.advice = advice;
        this.createTime = createTime;
        this.extInfo = extInfo;
        this.sqlTimeCost = sqlTimeCost;
        this.isInMainThread = isInMainThread;
    }

    @Override
    public boolean equals(Object o) {
        if (!(o instanceof SQLiteLintIssue)) {
            return false;
        }

        SQLiteLintIssue d = (SQLiteLintIssue) o;
        return d.id.equals(id);
    }

    @Override
    public int hashCode() {
        return id.hashCode();
    }

    protected SQLiteLintIssue(Parcel in) {
        id = in.readString();
        dbPath = in.readString();
        level = in.readInt();
        type = in.readInt();
        sql = in.readString();
        table = in.readString();
        desc = in.readString();
        detail = in.readString();
        advice = in.readString();
        createTime = in.readLong();
        extInfo = in.readString();
        sqlTimeCost = in.readLong();
        isInMainThread = in.readInt() == 1;
    }

    public static final Creator<SQLiteLintIssue> CREATOR = new Creator<SQLiteLintIssue>() {
        @Override
        public SQLiteLintIssue createFromParcel(Parcel in) {
            return new SQLiteLintIssue(in);
        }

        @Override
        public SQLiteLintIssue[] newArray(int size) {
            return new SQLiteLintIssue[size];
        }
    };

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeString(id);
        dest.writeString(dbPath);
        dest.writeInt(level);
        dest.writeInt(type);
        dest.writeString(sql);
        dest.writeString(table);
        dest.writeString(desc);
        dest.writeString(detail);
        dest.writeString(advice);
        dest.writeLong(createTime);
        dest.writeString(extInfo);
        dest.writeLong(sqlTimeCost);
        dest.writeInt(isInMainThread ? 1 : 0);
    }
}
