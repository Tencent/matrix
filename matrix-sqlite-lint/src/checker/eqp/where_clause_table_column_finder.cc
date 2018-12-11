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
// Created by wuzhiwen on 2017/3/1.
//

#include "where_clause_table_column_finder.h"
#include <algorithm>
#include "comm/log/logger.h"


namespace sqlitelint {
	WhereClauseTableColumnFinder::WhereClauseTableColumnFinder(const Expr * target_where, const std::string& target_table, const std::string& target_table_alias
			, const std::vector<ColumnInfo> &target_table_columns) {
		target_table_ = target_table;
        target_table_alias_ = target_table_alias;
        for (unsigned int i = 0; i < target_table_columns.size(); i++) {
            target_table_columns_.insert(target_table_columns[i].name_);
        }

		VisitExpr(target_where);
	}

	WhereClauseTableColumnFinder::~WhereClauseTableColumnFinder() {
	}

	bool WhereClauseTableColumnFinder::HasTargetTableColumnInWhereClause() const {
		sDebug("HasTargetTableColumnInWhereClause targetTable=%s targetTableAlias=%s %d", target_table_.c_str(), target_table_alias_.c_str(), has_target_table_column_in_where_clause_);
		return has_target_table_column_in_where_clause_;
	}

	bool WhereClauseTableColumnFinder::IsTargetTableHasConstantExpression() const {
		sDebug("IsTargetTableHasConstantExpression targetTable=%s targetTableAlias=%s %d", target_table_.c_str(), target_table_alias_.c_str(), is_target_table_has_constant_expression_);
		return is_target_table_has_constant_expression_;
	}

	const std::vector<std::string>&  WhereClauseTableColumnFinder::GetTargetTableColumnsInWhereClause() const {
		return target_table_columns_in_where_clause_;
	}

	void WhereClauseTableColumnFinder::VisitExpr(const Expr *p) {
		if (!p)return;
		if (p->op == TK_DOT) {
			std::string table, column;
			if (p->pLeft) {
				VisitToken(p->pLeft->token, &table);
			}
			if (p->pRight) {
				VisitToken(p->pRight->token, &column);
			}
			Process(table, column);
		} else if (p->op == TK_ID) {
			std::string table, column;
			VisitToken(p->token, &column);
			Process(table, column);
		} else {
            if (p->op == TK_STRING || p->op == TK_FLOAT || p->op == TK_INTEGER || p->op == TK_BLOB) {
                ProcessConstantExpr(p);
            }
			VisitExpr(p->pLeft);
			VisitExpr(p->pRight);
		}

		if (p->pSelect) {
			VisitSelect(p->pSelect);
		}
		VisitExprList(p->pList);
	}

    void WhereClauseTableColumnFinder::ProcessConstantExpr(const Expr *p) {
        if (!p)return;
        const Expr *parent = p->pParent;
        if (!parent)return;
        if (parent->pLeft) {
            if (parent->pLeft->op == TK_DOT) {
                std::string table;
                if (parent->pLeft->pLeft) {//dot's left
                    VisitToken(parent->pLeft->pLeft->token, &table);
                }
                if (table == target_table_) {
                    is_target_table_has_constant_expression_ = true;
                    return;
                }
                if (table == target_table_alias_) {
                    is_target_table_has_constant_expression_ = true;
                    return;
                }
            } else if (parent->pLeft->op == TK_ID){
                std::string column;
                VisitToken(parent->pLeft->token, &column);
                if (target_table_columns_.find(column) != target_table_columns_.end()) {
                    is_target_table_has_constant_expression_ = true;
                }
            }
            sVerbose("ProcessConstantExpr=%d" ,is_target_table_has_constant_expression_);
        }
    }

	void WhereClauseTableColumnFinder::VisitSelect(const Select *p) {
		if (!p)return;
		if (p->pPrior) {
			VisitSelect(p->pPrior);
		}
		VisitExprList(p->pEList);
		if (p->pSrc&&p->pSrc->nSrc) {
			VisitSrcList(p->pSrc);
		}
		if (p->pWhere) {
			VisitExpr(p->pWhere);
		}
		if (p->pGroupBy) {
			VisitExprList(p->pGroupBy);
		}
		if (p->pOrderBy) {
			VisitExprList(p->pOrderBy);
		}
		if (p->pHaving) {
			VisitExpr(p->pHaving);
		}
		if (p->pLimit) {
			VisitExpr(p->pLimit);
		}
		if (p->pOffset) {
			VisitExpr(p->pOffset);
		}
	}

	void WhereClauseTableColumnFinder::VisitExprList(const ExprList *p) {
		if (!p)return;

		for (int i = 0; i < p->nExpr; i++) {
			VisitExpr(p->a[i].pExpr);
		}
	}

	//from
	void WhereClauseTableColumnFinder::VisitSrcList(const SrcList *p) {
		if (!p)return;
		for (int i = 0; i < p->nSrc; i++) {
			VisitSelect(p->a[i].pSelect);

			if (p->a[i].pOn) {
				VisitExpr(p->a[i].pOn);
			}
		}
	}
	
	 void WhereClauseTableColumnFinder::VisitToken(const Token& p, std::string* r) {
		if (!p.n || !r) {
			return;
		}

		char text[p.n + 1];
		strncpy(text, (const char *)p.z, p.n);
		text[p.n] = 0;
		 *r = std::string(text);
	}

	void WhereClauseTableColumnFinder::Process(const std::string &table, const std::string &column) {
		sInfo("Process table=%s, column =%s", table.c_str(), column.c_str());
		if (table.length() > 0) {
			if (table == target_table_) {
				has_target_table_column_in_where_clause_ = true;
				std::vector<std::string>::iterator it = find(target_table_columns_in_where_clause_.begin(), target_table_columns_in_where_clause_.end(), column);
				if (it == target_table_columns_in_where_clause_.end()) {
					target_table_columns_in_where_clause_.push_back(column);
				}
				return;
			}
			if (table == target_table_alias_) {
				has_target_table_column_in_where_clause_ = true;
				std::vector<std::string>::iterator it =find(target_table_columns_in_where_clause_.begin(), target_table_columns_in_where_clause_.end(), column);
				if (it == target_table_columns_in_where_clause_.end()) {
					target_table_columns_in_where_clause_.push_back(column);
				}
				return;
			}
		} else {
			sInfo("visit column no explicit table");
			if (target_table_columns_.find(column) != target_table_columns_.end()) {
				has_target_table_column_in_where_clause_ = true;
				std::vector<std::string>::iterator it = find(target_table_columns_in_where_clause_.begin(), target_table_columns_in_where_clause_.end(), column);
				if (it == target_table_columns_in_where_clause_.end()) {
					target_table_columns_in_where_clause_.push_back(column);
				}
			}
		}
	}
}
