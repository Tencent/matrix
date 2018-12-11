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
** in order to generate code for DELETE FROM statements.
**
** $Id: delete.c,v 1.122 2006/02/24 02:53:50 drh Exp $
*/
#include "sqliteInt.h"

/*
** Look up every table that is named in pSrc.  If any table is not found,
** add an error message to pParse->zErrMsg and return NULL.  If all tables
** are found, return a pointer to the last table.
*/
/* Table *sqlite3SrcListLookup(Parse *pParse, SrcList *pSrc){ */
/*   Table *pTab = 0; */
/*   int i; */
/*   struct SrcList_item *pItem; */
/*   for(i=0, pItem=pSrc->a; i<pSrc->nSrc; i++, pItem++){ */
/*     pTab = sqlite3LocateTable(pParse, pItem->zName, pItem->zDatabase); */
/*     sqlite3DeleteTable(pParse->db, pItem->pTab); */
/*     pItem->pTab = pTab; */
/*     if( pTab ){ */
/*       pTab->nRef++; */
/*     } */
/*   } */
/*   return pTab; */
/* } */

/*
** Check to make sure the given table is writable.  If it is not
** writable, generate an error message and return 1.  If it is
** writable return 0;
*/
/* int sqlite3IsReadOnly(Parse *pParse, Table *pTab, int viewOk){ */
/*   if( pTab->readOnly && (pParse->db->flags & SQLITE_WriteSchema)==0 */
/*         && pParse->nested==0 ){ */
/*     sqlite3ErrorMsg(pParse, "table %s may not be modified", pTab->zName); */
/*     return 1; */
/*   } */
/* #ifndef SQLITE_OMIT_VIEW */
/*   if( !viewOk && pTab->pSelect ){ */
/*     sqlite3ErrorMsg(pParse,"cannot modify %s because it is a view",pTab->zName); */
/*     return 1; */
/*   } */
/* #endif */
/*   return 0; */
/* } */

/*
** Generate code that will open a table for reading.
*/
/* void sqlite3OpenTable( */
/*   Parse *p,       /1* Generate code into this VDBE *1/ */
/*   int iCur,       /1* The cursor number of the table *1/ */
/*   int iDb,        /1* The database index in sqlite3.aDb[] *1/ */
/*   Table *pTab,    /1* The table to be opened *1/ */
/*   int opcode      /1* OP_OpenRead or OP_OpenWrite *1/ */
/* ){ */
/*   Vdbe *v = sqlite3GetVdbe(p); */
/*   assert( opcode==OP_OpenWrite || opcode==OP_OpenRead ); */
/*   sqlite3TableLock(p, iDb, pTab->tnum, (opcode==OP_OpenWrite), pTab->zName); */
/*   sqlite3VdbeAddOp(v, OP_Integer, iDb, 0); */
/*   VdbeComment((v, "# %s", pTab->zName)); */
/*   sqlite3VdbeAddOp(v, opcode, iCur, pTab->tnum); */
/*   sqlite3VdbeAddOp(v, OP_SetNumColumns, iCur, pTab->nCol); */
/* } */


Delete* sqlite3DeleteNew(SrcList *pTabList, Expr *pWhere, Expr *pLimit, Expr *pOffset) {
    Delete* pNew = NULL;
    pNew = (Delete*) sqliteMalloc(sizeof(*pNew));
    if (pNew == NULL) {
        return NULL;
    }

    pNew->pTabList = pTabList;
    pNew->pWhere = pWhere;
    pNew->pLimit = pLimit;
    pNew->pOffset = pOffset;
    return pNew;
}

void sqlite3DeleteFree(Delete* deleteObj) {
    if (deleteObj == NULL) { return; }

    sqlite3SrcListDelete(deleteObj->pTabList);
    sqlite3ExprDelete(deleteObj->pWhere);
    sqlite3ExprDelete(deleteObj->pLimit);
    sqlite3ExprDelete(deleteObj->pOffset);
    sqliteFree(deleteObj);
}

/*
** Generate code for a DELETE FROM statement.
**
**     DELETE FROM table_wxyz WHERE a<5 AND b NOT NULL;
**                 \________/       \________________/
**                  pTabList              pWhere
*/
void sqlite3DeleteFrom(
  Parse *pParse,         /* The parser context */
  SrcList *pTabList,     /* The table from which we should delete things */
  Expr *pWhere,           /* The WHERE clause.  May be null */
  Expr *pLimit,
  Expr *pOffset
){
    Delete* deleteObj = sqlite3DeleteNew(pTabList, pWhere, pLimit, pOffset);
    if (deleteObj == NULL) {
        sqlite3ErrorMsg(pParse, "sqlite3DeleteNew return NULL, may the malloc failed!");
    }

    ParsedResultItem item;
    item.sqltype = SQLTYPE_DELETE;
    item.result.deleteObj = deleteObj;
    sqlite3ParsedResultArrayAppend(&pParse->parsed, &item);
}

/*
** This routine generates VDBE code that causes a single row of a
** single table to be deleted.
**
** The VDBE must be in a particular state when this routine is called.
** These are the requirements:
**
**   1.  A read/write cursor pointing to pTab, the table containing the row
**       to be deleted, must be opened as cursor number "base".
**
**   2.  Read/write cursors for all indices of pTab must be open as
**       cursor number base+i for the i-th index.
**
**   3.  The record number of the row to be deleted must be on the top
**       of the stack.
**
** This routine pops the top of the stack to remove the record number
** and then generates code to remove both the table record and all index
** entries that point to that record.
*/
/* void sqlite3GenerateRowDelete( */
/*   sqlite3 *db,       /1* The database containing the index *1/ */
/*   Vdbe *v,           /1* Generate code into this VDBE *1/ */
/*   Table *pTab,       /1* Table containing the row to be deleted *1/ */
/*   int iCur,          /1* Cursor number for the table *1/ */
/*   int count          /1* Increment the row change counter *1/ */
/* ){ */
/*   int addr; */
/*   addr = sqlite3VdbeAddOp(v, OP_NotExists, iCur, 0); */
/*   sqlite3GenerateRowIndexDelete(v, pTab, iCur, 0); */
/*   sqlite3VdbeAddOp(v, OP_Delete, iCur, (count?OPFLAG_NCHANGE:0)); */
/*   if( count ){ */
/*     sqlite3VdbeChangeP3(v, -1, pTab->zName, P3_STATIC); */
/*   } */
/*   sqlite3VdbeJumpHere(v, addr); */
/* } */

/*
** This routine generates VDBE code that causes the deletion of all
** index entries associated with a single row of a single table.
**
** The VDBE must be in a particular state when this routine is called.
** These are the requirements:
**
**   1.  A read/write cursor pointing to pTab, the table containing the row
**       to be deleted, must be opened as cursor number "iCur".
**
**   2.  Read/write cursors for all indices of pTab must be open as
**       cursor number iCur+i for the i-th index.
**
**   3.  The "iCur" cursor must be pointing to the row that is to be
**       deleted.
*/
/* void sqlite3GenerateRowIndexDelete( */
/*   Vdbe *v,           /1* Generate code into this VDBE *1/ */
/*   Table *pTab,       /1* Table containing the row to be deleted *1/ */
/*   int iCur,          /1* Cursor number for the table *1/ */
/*   char *aIdxUsed     /1* Only delete if aIdxUsed!=0 && aIdxUsed[i]!=0 *1/ */
/* ){ */
/*   int i; */
/*   Index *pIdx; */

/*   for(i=1, pIdx=pTab->pIndex; pIdx; i++, pIdx=pIdx->pNext){ */
/*     if( aIdxUsed!=0 && aIdxUsed[i-1]==0 ) continue; */
/*     sqlite3GenerateIndexKey(v, pIdx, iCur); */
/*     sqlite3VdbeAddOp(v, OP_IdxDelete, iCur+i, 0); */
/*   } */
/* } */

/*
** Generate code that will assemble an index key and put it on the top
** of the tack.  The key with be for index pIdx which is an index on pTab.
** iCur is the index of a cursor open on the pTab table and pointing to
** the entry that needs indexing.
*/
/* void sqlite3GenerateIndexKey( */
/*   Vdbe *v,           /1* Generate code into this VDBE *1/ */
/*   Index *pIdx,       /1* The index for which to generate a key *1/ */
/*   int iCur           /1* Cursor number for the pIdx->pTable table *1/ */
/* ){ */
/*   int j; */
/*   Table *pTab = pIdx->pTable; */

/*   sqlite3VdbeAddOp(v, OP_Rowid, iCur, 0); */
/*   for(j=0; j<pIdx->nColumn; j++){ */
/*     int idx = pIdx->aiColumn[j]; */
/*     if( idx==pTab->iPKey ){ */
/*       sqlite3VdbeAddOp(v, OP_Dup, j, 0); */
/*     }else{ */
/*       sqlite3VdbeAddOp(v, OP_Column, iCur, idx); */
/*       sqlite3ColumnDefault(v, pTab, idx); */
/*     } */
/*   } */
/*   sqlite3VdbeAddOp(v, OP_MakeIdxRec, pIdx->nColumn, 0); */
/*   sqlite3IndexAffinityStr(v, pIdx); */
/* } */
