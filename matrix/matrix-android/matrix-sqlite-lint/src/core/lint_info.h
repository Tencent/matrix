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

#ifndef SQLiteLint_TableInfo_H
#define SQLiteLint_TableInfo_H

#include <string>
#include <vector>
#include "lemon/sqliteInt.h"
#include "sqlite_lint.h"

namespace sqlitelint {

	class ColumnInfo {
	public:
		std::string name_;
		std::string type_;
		bool is_primary_key_;
	};

	class IndexElement {
	public:
		int pos_;
		int column_index_;
		std::string column_name_;
	};

	class IndexInfo {
	public:
		void AddIndexElement(IndexElement index_element);

		int seq_;
		std::string index_name_;
        std::vector<IndexElement> index_elements_;
		bool is_unique_;
	};
	
	class TableInfo {
	public:
		std::string table_name_;
		std::string create_sql_;

        std::vector<ColumnInfo> columns_;
        std::vector<IndexInfo> indexs_;
	};

    //TODO 自定义移动构造函数
	class SqlInfo {
	public:
		SqlInfo();
		~SqlInfo();
		void Release();

        void CopyWithoutParse(SqlInfo &info);

		std::string sql_;
		SqlType sql_type_ = SQLTYPE_UNKNOWN;
		// eg.
		// sql: select * from t where a = 3
		// then wildcard_sql_ will be : select * from t where a = ?
		// mostly used as a unique key
		std::string wildcard_sql_;

        // is the sql being prepared statement
        bool is_prepared_statement_ = true;

		// is select * contained
		bool is_select_all_ = false;

		int64_t execution_time_;

		Parse *parse_obj_ = nullptr;

		std::string ext_info_;

		long time_cost_ = 0;

		bool is_in_main_thread_ = false;
	};

	class Record {
	public:
		//EQP short for explain query plan
        constexpr static const char* const kEQPCompoundPrefix = "COMPOUND SUBQUERIES";
        constexpr static const char* const kEQPOneLoopScanPrefix = "SCAN";
        constexpr static const char* const kEQPOneLoopSearchPrefix = "SEARCH";
        constexpr static const char* const kEQPOneLoopScanSubqueryPrefix = "SCAN SUBQUERY";
        constexpr static const char* const kEQPOneLoopSearchSubqueryPrefix = "SEARCH SUBQUERY";
        constexpr static const char* const kEQPOneLoopScanTablePrefix  = "SCAN TABLE";
        constexpr static const char* const kEQPOneLoopSearchTablePrefix  = "SEARCH TABLE";
        constexpr static const char* const kEQPUseTempTreePrefix = "USE TEMP B-TREE";
        constexpr static const char* const kEQPExecuteScalarPrefix = "EXECUTE"; //TODO include execute LIST

		const static Record kEmpty;

		Record(const std::string& detail, const int select_id
                , const int order, const int from);
		~Record();

        bool isCompoundExplainRecord() const ;
        bool isOneLoopScanTableExplainRecord() const ;
        bool isOneLoopSearchTableExplainRecord() const ;
        bool isOneLoopSubQueryExplainRecord() const ;
        bool isUseTempTreeExplainRecord() const ;
		bool isExecuteScalarExplainRecord() const ;

		int select_id_;
		int order_;
		int from_;
		std::string detail_;
	};


	class QueryPlan {
	public:
		std::string sql_;
		std::vector<Record> plans_;
	};

}
#endif //end of SQLiteLint_TableInfo_H
