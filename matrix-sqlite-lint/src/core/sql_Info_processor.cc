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

#include "sql_Info_processor.h"
#include "comm/log/logger.h"

namespace sqlitelint {

    Parse *SqlInfoProcessor::ParseObj(const std::string& sql) {
        Parse *parse_obj = sqlite3ParseNew();
        char *err_msg = nullptr;

		sqlite3RunParser(parse_obj, sql.c_str(), &err_msg);

		if (sqlite3MallocFailed()) {
            parse_obj->rc = SQLITE_NOMEM;
        } else if (parse_obj->rc == SQLITE_DONE) {
            parse_obj->rc = SQLITE_OK;
        }

		if (parse_obj->rc != SQLITE_OK) {
			sError("SqlInfoProcessor::ParseObj error: %s, error_code:%d \n sql: %s", err_msg, parse_obj->rc, sql.c_str());
			sqlite3ParseClean(parse_obj);
			parse_obj = nullptr;
		}

        if (err_msg != nullptr) {
            sqliteFree(err_msg);
		}

        return parse_obj;
	}

    int SqlInfoProcessor::Process(SqlInfo *sql_info) {
        if (!sql_info){
            return -1;
        }

		Parse *parse_obj = SqlInfoProcessor::ParseObj(sql_info->sql_);
        if (!parse_obj) {
            sError("SqlInfoProcessor::ParseObj parseObj null %s", sql_info->sql_.c_str());
            return -1;
        }

        sql_info->parse_obj_ = parse_obj;
        sql_info->sql_type_ = parse_obj->parsed.array[0].sqltype;

		if (parse_obj->parsed.array[0].sqltype == SQLTYPE_SELECT &&
              parse_obj->parsed.array[0].result.selectObj) {

			ProcessSelect(parse_obj->parsed.array[0].result.selectObj);
        } else if (parse_obj->parsed.array[0].sqltype == SQLTYPE_INSERT &&
                    parse_obj->parsed.array[0].result.insertObj) {

			ProcessInsert(parse_obj->parsed.array[0].result.insertObj, false);
        } else if (parse_obj->parsed.array[0].sqltype == SQLTYPE_REPLACE &&
                  parse_obj->parsed.array[0].result.insertObj) {

            ProcessInsert(parse_obj->parsed.array[0].result.insertObj,true);
        } else if (parse_obj->parsed.array[0].sqltype == SQLTYPE_DELETE &&
                  parse_obj->parsed.array[0].result.deleteObj) {

			ProcessDelete(parse_obj->parsed.array[0].result.deleteObj);

        } else if (parse_obj->parsed.array[0].sqltype == SQLTYPE_UPDATE &&
                  parse_obj->parsed.array[0].result.updateObj) {

			ProcessUpdate(parse_obj->parsed.array[0].result.updateObj);

        } else{
            sWarn( "SqlInfoProcessor::ParseObj unknown sqlType");
            return -1;
        }
        if(select_all_count_ > 0){
            sql_info->is_select_all_ = true;
        }
        sql_info->is_prepared_statement_ = is_prepared_statement_ || !is_parameter_wildcard_;
        if(is_parameter_wildcard_ && need_gen_wildcard_sql_){
            sql_info->wildcard_sql_ = wildcard_sql_;
        }

		sVerbose( "SqlInfoProcessor::ParseObj wildcard_sql_ = %s", wildcard_sql_.c_str());

		return 0;
    }

    std::string SqlInfoProcessor::GetSql(const Select *select_obj, bool need_gen_wildcard_sql) {
        need_gen_wildcard_sql_ = need_gen_wildcard_sql;

		ProcessSelect(select_obj);
        sVerbose( "SqlInfoProcessor::GetSql, wildcard_sql_ = %s", wildcard_sql_.c_str());
		return wildcard_sql_;
    }

	void SqlInfoProcessor::ProcessInsert(const Insert *p, bool replace) {
		if (!p)return;
		wildcard_sql_ +=  replace ? "replace into " : "insert into ";
		if (p->pTabList) {
			ProcessSrcList(p->pTabList);
		}

		if (p->pColumn) {
			wildcard_sql_ += "(";
			ProcessIdList(p->pColumn);
			wildcard_sql_ += ")";
		}
		if (p->pSetList) {
			wildcard_sql_ += " set ";
			ProcessExprList(p->pSetList, TK_SET);
		}
		if (p->pSelect) {
			wildcard_sql_ += " ";
			ProcessSelect(p->pSelect);
		}
		if (p->pValuesList) {
			wildcard_sql_ += " values";
            ProcessValuesList(p->pValuesList);
		}
	}

	//delete
	void SqlInfoProcessor::ProcessDelete(const Delete *p) {
		if (!p)return;
		wildcard_sql_ += "delete from ";
		ProcessSrcList(p->pTabList);
		if (p->pWhere) {
			wildcard_sql_ += " where ";
			ProcessExpr(p->pWhere);
		}
		if (p->pLimit) {
			wildcard_sql_ += " limit ";
			ProcessExpr(p->pLimit);
		}
		if (p->pOffset) {
			wildcard_sql_ += " offset ";
			ProcessExpr(p->pOffset);
		}
	}

	//update
	void SqlInfoProcessor::ProcessUpdate(const Update *p) {
		if (!p)return;
		wildcard_sql_ += "update ";
		ProcessSrcList(p->pTabList);
		if (p->pChanges) {
			wildcard_sql_ += " set ";
			ProcessExprList(p->pChanges, TK_SET);
		}
		if (p->pWhere) {
			wildcard_sql_ += " where ";
			ProcessExpr(p->pWhere);
		}
		if (p->pLimit) {
			wildcard_sql_ += " limit ";
			ProcessExpr(p->pLimit);
		}
		if (p->pOffset) {
			wildcard_sql_ += " offset ";
			ProcessExpr(p->pOffset);
		}
	}

	void SqlInfoProcessor::ProcessSelect(const Select *p) {
		if (!p)return;
		if (p->pPrior) {
			ProcessSelect(p->pPrior);
			if (TK_UNION == p->op) {
				wildcard_sql_ += " union ";
			}
		}
		if (p->isDistinct) {
			wildcard_sql_ += "select distinct ";
		} else {
			wildcard_sql_ += "select ";
		}
		ProcessExprList(p->pEList, 0);//column
		if (p->pSrc&&p->pSrc->nSrc) {
			wildcard_sql_ += " from ";
			ProcessSrcList(p->pSrc);
		}
		if (p->pWhere) {
			wildcard_sql_ += " where ";
			ProcessExpr(p->pWhere);
		}
		if (p->pGroupBy) {
			wildcard_sql_ += " group by ";
			ProcessExprList(p->pGroupBy, 0);
		}
		if (p->pOrderBy) {
			wildcard_sql_ += " order by ";
			ProcessExprList(p->pOrderBy, TK_ORDER);
		}
		if (p->pHaving) {
			wildcard_sql_ += " having ";
			ProcessExpr(p->pHaving);
		}
		if (p->pLimit) {
			wildcard_sql_ += " limit ";
			ProcessExpr(p->pLimit);
		}
		if (p->pOffset) {
			wildcard_sql_ += " offset ";
			ProcessExpr(p->pOffset);
		}
	}

	void SqlInfoProcessor::ProcessExprList(const ExprList *p, int op) {
		if (!p)return;

		bool need_escape = false;

		for (int i = 0; i < p->nExpr; i++) {
			if (p->a[i].zName && TK_SET == op) {
				wildcard_sql_ += p->a[i].zName;
				wildcard_sql_ += " = ";
			} else if (TK_CASE == op) {
                wildcard_sql_ += (i % 2 == 0) ? " when " : " then ";
            }

			ProcessExpr(p->a[i].pExpr);
			if (p->a[i].zName && TK_SET != op) {
                if (p->a[i].pExpr && p->a[i].pExpr->op == TK_CASE) {
                    wildcard_sql_ += " end ";
                } else {
                    wildcard_sql_ += " as ";
                }
				wildcard_sql_ += p->a[i].zName;
			}
			if (i < (p->nExpr - 1)) {
				if (TK_BETWEEN == op) {
					wildcard_sql_ += " and ";
				} else if (op == TK_LIKE_KW) {
					if (p->nExpr >= 3) {
						need_escape = true;
					}
					op = 0;
                    wildcard_sql_ += " " + like_ + " ";
				} else if (op == TK_ORDER){
                    if (p->a[i].sortOrder){
                        wildcard_sql_ += " desc,";
                    } else {//ignore asc
                        wildcard_sql_ += ",";
                    }
                } else if (TK_CASE != op){
					if (need_escape) {
						wildcard_sql_ += " escape ";
					} else {
						wildcard_sql_ += ",";
					}
				}
			} else if (i == (p->nExpr - 1)){
                if (op == TK_ORDER){
                    if (p->a[i].sortOrder){
                        wildcard_sql_ += " desc";//ignore asc
                    }
                }
            }
		}
	}

	void SqlInfoProcessor::ProcessExpr(const Expr *p) {
		if (!p)return;

		ProcessExpr(p->pLeft);
		ProcessToken(p);
		if ((p->op == TK_AND || p->op == TK_OR) && p->pRight && (p->pRight->op == TK_AND || p->pRight->op == TK_OR)) {
			wildcard_sql_ += "(";
		}
		ProcessExpr(p->pRight);
		if (p->pSelect) {
			if (TK_IN != p->op) wildcard_sql_ += "(";
			ProcessSelect(p->pSelect);
			if (TK_IN != p->op) wildcard_sql_ += ")";
		}
		ProcessExprList(p->pList, p->op);

		switch (p->op) {
		case TK_IN:
		case TK_EXISTS:
			wildcard_sql_ += ")";
			break;
		case TK_AND:
		case TK_OR:
			if (p->pRight && (p->pRight->op == TK_AND || p->pRight->op == TK_OR)) {
				wildcard_sql_ += ")";
			}
			break;
		case TK_FUNCTION:
			if (p->pList || p->pLeft || p->pRight) {
				wildcard_sql_ += ")";
			} else {
				wildcard_sql_ += "*)";
			}
			break;
		}
	}

	void SqlInfoProcessor::ProcessSrcList(const SrcList *p) {
		if (!p)return;
        bool zName_consumed = false;
		for (int i = 0; i < p->nSrc; i++) {
			if (p->a[i].zDatabase) {
				wildcard_sql_ += p->a[i].zDatabase;
				wildcard_sql_ += ".";
			}
			if (!zName_consumed && p->a[i].zName) {
				wildcard_sql_ += p->a[i].zName;
			}
            zName_consumed = false;
            if (p->a[i].pSelect) {
                wildcard_sql_ += " (";
				ProcessSelect(p->a[i].pSelect);
                wildcard_sql_ += ") ";
            }
			if (p->a[i].zAlias) {
				wildcard_sql_ += " as ";
				wildcard_sql_ += p->a[i].zAlias;
			}

			if (p->a[i].jointype && (p->a[i].pUsing || p->a[i].pOn)) {
				if (JT_LEFT & p->a[i].jointype) {
					wildcard_sql_ += " left";
				}
				if (JT_RIGHT & p->a[i].jointype) {
					wildcard_sql_ += " right";
				}
				if (JT_FULL & p->a[i].jointype) {
					wildcard_sql_ += " full";
				}
				if (JT_NATURAL & p->a[i].jointype) {
					wildcard_sql_ += " natural";
				}
				if (JT_OUTER & p->a[i].jointype) {
					wildcard_sql_ += " outer";
				}

				wildcard_sql_ += " join ";
				if (i < p->nSrc - 1 && p->a[i + 1].zName) {
					wildcard_sql_ += p->a[i + 1].zName;
                    zName_consumed = true;
				}
			} else if (i < (p->nSrc - 1)) {
				wildcard_sql_ += ",";
			}

			if (p->a[i].pUsing) {
				wildcard_sql_ += " using (";
				ProcessIdList(p->a[i].pUsing);
				wildcard_sql_ += ") ";
			}

			if (p->a[i].pOn) {
				wildcard_sql_ += " on (";
				ProcessExpr(p->a[i].pOn);
				wildcard_sql_ += ") ";
			}
		}
	}

	void SqlInfoProcessor::ProcessValuesList(const ValuesList *p) {
		if (!p)return;
		for (int i = 0; i < p->nValues; i++) {
			wildcard_sql_ += "(";
			ProcessExprList(p->a[i], 0);
			if (i < p->nValues - 1) {
				wildcard_sql_ += "),";
			} else {
				wildcard_sql_ += ")";
			}
		}
	}

	void SqlInfoProcessor::ProcessIdList(const IdList *p) {
		if (!p)return;
		for (int i = 0; i < p->nId; i++) {
			wildcard_sql_ += p->a[i].zName;
			if (i < p->nId - 1) {
				wildcard_sql_ += ",";
			}
		}
	}

	void SqlInfoProcessor::ProcessToken(const Expr *p) {
        if (!p)return;
        const Token& t = p->token;
        int op = p->op;

		if (!t.n) {
			switch (op) {
			case TK_IN:
                if (p->pParent && p->pParent->op == TK_NOT) {
					wildcard_sql_ += " not in(";
				} else {
					wildcard_sql_ += " in(";
				}
				break;
			case TK_ALL:
				wildcard_sql_ += "*";
                select_all_count_++;
				break;
			case TK_DOT:
				wildcard_sql_ += ".";
				break;
			case TK_EQ:
				wildcard_sql_ += " = ";
				break;
			case TK_NE:
				wildcard_sql_ += " != ";
				break;
			case TK_GT:
				wildcard_sql_ += " > ";
				break;
			case TK_LE:
				wildcard_sql_ += " <= ";
				break;
			case TK_LT:
				wildcard_sql_ += " < ";
				break;
			case TK_GE:
				wildcard_sql_ += " >= ";
				break;
			case TK_AND:
				wildcard_sql_ += " and ";
				break;
			case TK_OR:
				wildcard_sql_ += " or ";
				break;
			case TK_BITAND:
				wildcard_sql_ += " & ";
				break;
			case TK_BITOR:
				wildcard_sql_ += " | ";
				break;
			case TK_BITNOT:
				wildcard_sql_ += " ~ ";
				break;
			case TK_LSHIFT:
				wildcard_sql_ += " << ";
				break;
			case TK_RSHIFT:
				wildcard_sql_ += " >> ";
				break;
			case TK_BETWEEN:
				wildcard_sql_ += " between ";
				break;
			case TK_EXISTS:
                if (p->pParent && p->pParent->op == TK_NOT) {
					wildcard_sql_ += " not exists(";
				} else {
					wildcard_sql_ += " exists(";
				}
				break;
			default:
				break;
			}
			return;
		}
		char text[t.n + 1];
		strncpy(text, (const char *)t.z, t.n);
		text[t.n] = 0;
		switch (op) {
            case TK_STRING:
            case TK_FLOAT:
            case TK_INTEGER:
            case TK_BLOB:
                if (need_gen_wildcard_sql_) {
                    wildcard_sql_ += '?';
                } else{
                    wildcard_sql_ += text;
                }
                is_parameter_wildcard_ = true;
			break;
		case TK_LIKE_KW:
            if (p->pParent && p->pParent->op == TK_NOT) {
                like_ = std::string("not ") + text;
            } else {
                like_ = text;
            }
			break;
		case TK_FUNCTION:
			wildcard_sql_ += text;
			wildcard_sql_ += "(";
			break;
		case TK_VARIABLE:
			wildcard_sql_ += text;
            is_prepared_statement_ = true;
			break;
		default:
            if (op == TK_ID && p->pParent && p->pParent->op == TK_CASE) {
                wildcard_sql_ += " case ";
            }
			wildcard_sql_ += text;
			break;
		}
	}

}
