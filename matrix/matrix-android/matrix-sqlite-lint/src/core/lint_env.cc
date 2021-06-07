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

//
// Created by liyongjie
//

#include "lint_env.h"
#include "comm/log/logger.h"
#include "comm/lint_util.h"

namespace sqlitelint {
    extern SqlExecutionDelegate kSqlExecutionDelegate;

    //These callbacks deal with the querys'(LintEnv::GetQuery) results
    static int OnSelectTablesCallback(void *para, int n_column, char **column_value, char **column_name);
    static int OnSelectColumnsCallback(void *para, int n_column, char **column_value, char **column_name);
    static int OnSelectIndexsCallback(void *para, int n_column, char **column_value, char **column_name);
    static int OnSelectIndexInfoCallback(void *para, int n_column, char **column_value, char **column_name);
    static int OnExplainQueryPlanCallback(void *para, int n_column, char **column_value, char **column_name);

    LintEnv::LintEnv(std::string db_path) : db_path_(db_path), checked_sql_cnt_(0) {
        int pos = db_path.find_last_of('/');
        if (pos >= 0) {
            db_file_name_ = db_path.substr(pos + 1);
        } else {
            db_file_name_ = db_path;
        }
    }

    int LintEnv::GetQuery(const std::string &query_sql, const SqlExecutionCallback &callback,
                          void *para,
                          char **errMsg) {
        reserve_sql_mgr_.MarkReserve(query_sql);
        return SQLite3ExecSql(db_path_.c_str(), query_sql.c_str(), callback, para, errMsg);
    }

    const std::vector<TableInfo> LintEnv::GetTablesInfo() {
        std::unique_lock<std::mutex> lock(lints_mutex_);
        if (tables_info_.empty()) {
            CollectTablesInfo();
        }
        return tables_info_;
    }

    void LintEnv::GetTableInfo(const std::string &table_name,TableInfo& tableInfo) {
        const std::vector<TableInfo>& tables = GetTablesInfo();
        for (const TableInfo &t : tables) {
            if (CompareIgnoreCase(table_name, t.table_name_) == 0) {
                tableInfo = t;
            }
        }
    }

    int LintEnv::GetExplainQueryPlan(const std::string &sql, QueryPlan *query_plan) {
        const std::string explain_sql = "explain query plan " + sql;
        char *err_msg;
        int r = GetQuery(explain_sql, OnExplainQueryPlanCallback, query_plan, &err_msg);
        if (r != 0) {
            if (err_msg) {
                sError("LintEnv::GetExplainQueryPlan error: %s; sql: %s ", err_msg, explain_sql.c_str());
                free(err_msg);
            }
        }

        return r;
    }

    void LintEnv::AddToSqlHistory(const SqlInfo sql_info) {
        sql_history_.push_back(sql_info);
    }

    const std::vector<SqlInfo>& LintEnv::GetSqlHistory() {
        return sql_history_;
    }

    void LintEnv::ReleaseHistory(int count) {
        size_t size = sql_history_.size();
        if(count < size){
            auto it = sql_history_.begin();
            for (int i=0; i< count; i++){
                sql_history_[i].Release();
                it++;
            }
            sql_history_.erase(sql_history_.begin(),it);
        }
        sVerbose("releaseHistory %zu/%zu",sql_history_.size(),size);
    }

    void LintEnv::CheckReleaseHistory() {
        if (sql_history_.size() >= 1000){
            ReleaseHistory(200);
        }
    }

    const std::string LintEnv::GetDbPath() const {
        return db_path_;
    }

    const std::string LintEnv::GetDbFileName() const {
        return db_file_name_;
    }

    bool LintEnv::IsReserveSql(const std::string &sql) {
        return reserve_sql_mgr_.IsReserve(sql);
    }

    void LintEnv::CollectTablesInfo() {
        auto DealWithGetQuery = [](int r, char *err_msg, const std::string &query_sql) {
            if (r == 0) {
                return true;
            }
            if (err_msg) {
                sError("LintEnv::CollectTablesInfo error : %s; sql : %s", err_msg,
                       query_sql.c_str());
                free(err_msg);
            }
            return false;
        };

        char *err_msg = nullptr;
        int r = GetQuery(kSelectTablesSql, OnSelectTablesCallback, &tables_info_, &err_msg);
        if (!DealWithGetQuery(r, err_msg, kSelectTablesSql)) {
            return;
        }

        std::string select_columns_sql;
        std::string select_index_sql;
        std::string select_index_info_sql;
        for (TableInfo &table_info : tables_info_) {
            select_columns_sql = "PRAGMA table_info(" + table_info.table_name_ + ")";
            r = GetQuery(select_columns_sql, OnSelectColumnsCallback, &table_info, &err_msg);
            if (!DealWithGetQuery(r, err_msg, select_columns_sql)) {
                return;
            }

            select_index_sql = "PRAGMA index_list(" + table_info.table_name_ + ")";

            r = GetQuery(select_index_sql, OnSelectIndexsCallback, &table_info, &err_msg);
            if (!DealWithGetQuery(r, err_msg, select_index_sql)) {
                return;
            }

            for (IndexInfo &index_info : table_info.indexs_) {
                select_index_info_sql = "PRAGMA index_info(" + index_info.index_name_ + ")";
                r = GetQuery(select_index_info_sql, OnSelectIndexInfoCallback, &index_info,
                             &err_msg);
                if (!DealWithGetQuery(r, err_msg, select_index_info_sql)) {
                    return;
                }
            }
        }
    }

    int LintEnv::SQLite3ExecSql(const char *db_path, const char *sql,
                                const SqlExecutionCallback &callback, void *para, char **errmsg) {
        if (kSqlExecutionDelegate) {
            return kSqlExecutionDelegate(db_path, sql, callback, para, errmsg);
        } else {
            sError("LintEnv::SQLite3ExecSql kSqlExecutionDelegate not set!!!");
            return -1;
        }
    }

    void LintEnv::SetWhiteList(const std::map<std::string, std::set<std::string>> &white_list) {
        white_list_mgr_.SetWhiteList(white_list);
    }

    bool LintEnv::IsInWhiteList(const std::string& checker_name, const std::string &target) const {
        return white_list_mgr_.IsInWhiteList(checker_name, target);
    }

    void LintEnv::IncSqlCnt() {
        checked_sql_cnt_ ++;
    }

    int LintEnv::GetSqlCnt() {
        return checked_sql_cnt_;
    }

    int OnSelectTablesCallback(void *para, int n_column, char **column_value, char **column_name) {
        if (para == nullptr) {
            sError("OnSelectTablesCallback para is null");
            return -1;
        }

        TableInfo table_info;
        table_info.table_name_ = (column_value[0] == nullptr ? "" : column_value[0]);
        if (ReserveSqlManager::IsReservedTable(table_info.table_name_)) {
            return 0;
        }

        table_info.create_sql_ = (column_value[1] == nullptr ? "" : column_value[1]);
        std::vector<TableInfo> *infoList = reinterpret_cast<std::vector<TableInfo> *>(para);
        infoList->push_back(table_info);
        return 0;
    }

    int OnSelectColumnsCallback(void *para, int n_column, char **column_value, char **column_name) {
        if (para == nullptr) {
            sError("OnSelectColumnsCallback para is null");
            return -1;
        }

        TableInfo *table_info = reinterpret_cast<TableInfo *>(para);
        ColumnInfo column_info;
        int columns_assigned = 0;
        for (int i = 0; i < n_column; i++) {
            if (strcmp("name", column_name[i]) == 0) {
                column_info.name_ = (nullptr != column_value[i] ? column_value[i] : "");
                columns_assigned++;
            } else if (strcmp("type", column_name[i]) == 0) {
                column_info.type_ = (nullptr != column_value[i] ? column_value[i] : "");
                columns_assigned++;
            } else if (strcmp("pk", column_name[i]) == 0) {
                column_info.is_primary_key_ = column_value[i][0] != '0';
                columns_assigned++;
            }

            if (columns_assigned == 3) {
                break;
            }
        }

        table_info->columns_.push_back(column_info);
        return 0;
    }

    int OnSelectIndexsCallback(void *para, int n_column, char **column_value, char **column_name) {
        if (para == nullptr) {
            sError("OnSelectIndexsCallback para is null");
            return -1;
        }

        IndexInfo index_info;
        int columns_assigned = 0;
        for (int i = 0; i < n_column; i++) {
            if (strcmp("name", column_name[i]) == 0) {
                index_info.index_name_ = (nullptr != column_value[i] ? column_value[i] : "");
                columns_assigned++;
            } else if (strcmp("seq", column_name[i]) == 0) {
                index_info.seq_ = atoi(column_value[i]);
                columns_assigned++;
            } else if (strcmp("unique", column_name[i]) == 0) {
                index_info.is_unique_ = column_value[i][0] != '0';
                columns_assigned++;
            }

            if (columns_assigned == 3) {
                break;
            }
        }

        TableInfo* table_info = reinterpret_cast<TableInfo *>(para);
        sVerbose("OnSelectIndexsCallback index : %s", index_info.index_name_.c_str());

        table_info->indexs_.push_back(index_info);
        return 0;
    }

    int OnSelectIndexInfoCallback(void* para, int n_column, char** column_value, char** column_name) {
        if (para == nullptr) {
            sError("OnSelectIndexsCallback para is null");
            return -1;
        }

        IndexElement index_element;
        int columns_assigned = 0;
        for (int i = 0; i < n_column; i++) {
            if (strcmp("seqno", column_name[i]) == 0) {
                index_element.pos_ = atoi(column_value[i]);
                columns_assigned ++;
            }
            else if (strcmp("cid", column_name[i]) == 0) {
                index_element.column_index_ = atoi(column_value[i]);
                columns_assigned ++;
            }
            else if (strcmp("name", column_name[i]) == 0) {
                index_element.column_name_ = (nullptr != column_value[i] ? column_value[i] : "");
                columns_assigned ++;
            }
            if (columns_assigned == 3) {
                break;
            }
        }

        IndexInfo* index_info = reinterpret_cast<IndexInfo*>(para);
        if(index_info) {
            index_info->AddIndexElement(index_element);
        } else{
            sError("onCollectIndexInfoCallback indexInfo is null");
        }
        return 0;
    }

    int OnExplainQueryPlanCallback(void *para, int n_column, char **column_value, char **column_name) {
        if (para == nullptr) {
            sError("OnExplainQueryPlanCallback para is null");
            return -1;
        }

        Record record( (nullptr != column_value[3] ? column_value[3] : "")
                , atoi(column_value[0]), atoi(column_value[1]), atoi(column_value[2]));

        QueryPlan *plan = reinterpret_cast<QueryPlan*>(para);
        if(plan) {
            plan->plans_.push_back(record);
        } else{
            sError("onExplainSqlCallback plan null");
        }

        return 0;
    }

    bool ReserveSqlManager::IsReservedTable(const std::string &table_name) {
        static std::set<std::string> kReservedTables = {"sqlite_master", "sqlite_sequence", "android_metadata"};
        return kReservedTables.find(table_name) != kReservedTables.end();
    }

    void ReserveSqlManager::MarkReserve(const std::string& sql) {
        reserve_sql_map_.insert(std::pair<std::string, int64_t>(sql, GetSysTimeMillisecond()));
    }

    bool ReserveSqlManager::IsReserve(const std::string &sql) {
        if (reserve_sql_map_.find(sql) == reserve_sql_map_.end()) {
            return false;
        }

        int64_t reserve_query_time = reserve_sql_map_[sql];
        if (GetSysTimeMillisecond() - reserve_query_time > kMarkReserveIntervalThreshold) {
            sDebug("ReserveSqlManager::isReserve mark reserve expired, sql:%s", sql.c_str());
            reserve_sql_map_.erase(sql);
            return false;
        }

        return true;
    }
}
