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

//SqlInfoProcessor.h
// Created by wuzhiwen on 2017/2/28.
//

#ifndef SQLITELINT_SQLINFOPROCESSOR_H
#define SQLITELINT_SQLINFOPROCESSOR_H

#include <string>
#include "lint_info.h"

namespace sqlitelint {

    class SqlInfoProcessor {
    public:
        int Process(SqlInfo *sql_info);

        std::string GetSql(const Select *select_obj, bool need_gen_wildcard_sql);

    private:
        Parse *ParseObj(const std::string& sql);

        //select
        void ProcessSelect(const Select *p);
        void ProcessExprList(const ExprList *p, int op);
        void ProcessExpr(const Expr *p);
        void ProcessSrcList(const SrcList *p);
        void ProcessToken(const Expr *p);
        void ProcessIdList(const IdList *p);

		//insert
		void ProcessInsert(const Insert *p, bool replace);
		void ProcessValuesList(const ValuesList *p);

		//delete
		void ProcessDelete(const Delete *p);

		//update
		void ProcessUpdate(const Update *p);

		// eg.
		// sql: select * from t where a = 3
		// if need_gen_wildcard_sql_ true
        // then wildcard_sql_ will be : select * from t where a = ?
		std::string wildcard_sql_;
        bool need_gen_wildcard_sql_ = true;

        // is the sql being prepared statement
		bool is_prepared_statement_ = false;

        bool is_parameter_wildcard_ = false;

		std::string like_;
		int select_all_count_ = 0;//select *
    };
}

#endif //SQLITELINT_SQLINFOPROCESSOR_H
