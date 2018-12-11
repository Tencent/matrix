/*
** 2001 September 15
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file contains C code routines that are called by the parser
** to handle UPDATE statements.
**
** $Id: update.c,v 1.123 2006/02/24 02:53:50 drh Exp $
*/
#include "sqliteInt.h"

/*
** The most recently coded instruction was an OP_Column to retrieve the
** i-th column of table pTab. This routine sets the P3 parameter of the
** OP_Column to the default value, if any.
**
** The default value of a column is specified by a DEFAULT clause in the
** column definition. This was either supplied by the user when the table
** was created, or added later to the table definition by an ALTER TABLE
** command. If the latter, then the row-records in the table btree on disk
** may not contain a value for the column and the default value, taken
** from the P3 parameter of the OP_Column instruction, is returned instead.
** If the former, then all row-records are guaranteed to include a value
** for the column and the P3 value is not required.
**
** Column definitions created by an ALTER TABLE command may only have
** literal default values specified: a number, null or a string. (If a more
** complicated default expression value was provided, it is evaluated
** when the ALTER TABLE is executed and one of the literal values written
** into the sqlite_master table.)
**
** Therefore, the P3 parameter is only required if the default value for
** the column is a literal number, string or null. The sqlite3ValueFromExpr()
** function is capable of transforming these types of expressions into
** sqlite3_value objects.
*/
/* void sqlite3ColumnDefault(Vdbe *v, Table *pTab, int i){ */
/*   if( pTab && !pTab->pSelect ){ */
/*     sqlite3_value *pValue; */
/*     u8 enc = ENC(sqlite3VdbeDb(v)); */
/*     Column *pCol = &pTab->aCol[i]; */
/*     sqlite3ValueFromExpr(pCol->pDflt, enc, pCol->affinity, &pValue); */
/*     if( pValue ){ */
/*       sqlite3VdbeChangeP3(v, -1, (const char *)pValue, P3_MEM); */
/*     }else{ */
/*       VdbeComment((v, "# %s.%s", pTab->zName, pCol->zName)); */
/*     } */
/*   } */
/* } */

Update* sqlite3UpdateNew(SrcList *pTabList, ExprList *pChanges, Expr *pWhere, int onError, Expr *pLimit, Expr *pOffset) {
    Update *pNew = NULL;
    pNew = (Update*)sqliteMalloc( sizeof(*pNew) );
    if (pNew == NULL) {
        return NULL;
    }

    pNew->pTabList = pTabList;
    pNew->pChanges = pChanges;
    pNew->pWhere = pWhere;
    pNew->onError = onError;
    pNew->pLimit = pLimit;
    pNew->pOffset = pOffset;
    return pNew;
}

void sqlite3UpdateDelete(Update* updateObj) {
    if (updateObj == NULL) { return; }

    sqlite3SrcListDelete(updateObj->pTabList);
    sqlite3ExprListDelete(updateObj->pChanges);
    sqlite3ExprDelete(updateObj->pWhere);
    sqlite3ExprDelete(updateObj->pLimit);
    sqlite3ExprDelete(updateObj->pOffset);
    sqliteFree(updateObj);
}

/*
** Process an UPDATE statement.
**
**   UPDATE OR IGNORE table_wxyz SET a=b, c=d WHERE e<5 AND f NOT NULL;
**          \_______/ \________/     \______/       \________________/
*            onError   pTabList      pChanges             pWhere
*/
void sqlite3Update(
  Parse *pParse,         /* The parser context */
  SrcList *pTabList,     /* The table in which we should change things */
  ExprList *pChanges,    /* Things to be changed */
  Expr *pWhere,          /* The WHERE clause.  May be null */
  int onError,            /* How to handle constraint errors */
  Expr *pLimit,
  Expr *pOffset
){
    Update *updateObj = sqlite3UpdateNew(pTabList, pChanges, pWhere, onError, pLimit, pOffset);

    ParsedResultItem item;
    item.sqltype = SQLTYPE_UPDATE;
    item.result.updateObj = updateObj;
    sqlite3ParsedResultArrayAppend(&pParse->parsed, &item);
}
