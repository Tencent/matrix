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
** to handle INSERT statements in SQLite.
**
** $Id: insert.c,v 1.164 2006/03/15 16:26:10 drh Exp $
*/
#include "sqliteInt.h"

/*
** Set P3 of the most recently inserted opcode to a column affinity
** string for index pIdx. A column affinity string has one character
** for each column in the table, according to the affinity of the column:
**
**  Character      Column affinity
**  ------------------------------
**  'a'            TEXT
**  'b'            NONE
**  'c'            NUMERIC
**  'd'            INTEGER
**  'e'            REAL
*/
/* void sqlite3IndexAffinityStr(Vdbe *v, Index *pIdx){ */
/*   if( !pIdx->zColAff ){ */
/*     /1* The first time a column affinity string for a particular index is */
/*     ** required, it is allocated and populated here. It is then stored as */
/*     ** a member of the Index structure for subsequent use. */
/*     ** */
/*     ** The column affinity string will eventually be deleted by */
/*     ** sqliteDeleteIndex() when the Index structure itself is cleaned */
/*     ** up. */
/*     *1/ */
/*     int n; */
/*     Table *pTab = pIdx->pTable; */
/*     pIdx->zColAff = (char *)sqliteMalloc(pIdx->nColumn+1); */
/*     if( !pIdx->zColAff ){ */
/*       return; */
/*     } */
/*     for(n=0; n<pIdx->nColumn; n++){ */
/*       pIdx->zColAff[n] = pTab->aCol[pIdx->aiColumn[n]].affinity; */
/*     } */
/*     pIdx->zColAff[pIdx->nColumn] = '\0'; */
/*   } */

/*   sqlite3VdbeChangeP3(v, -1, pIdx->zColAff, 0); */
/* } */

/*
** Set P3 of the most recently inserted opcode to a column affinity
** string for table pTab. A column affinity string has one character
** for each column indexed by the index, according to the affinity of the
** column:
**
**  Character      Column affinity
**  ------------------------------
**  'a'            TEXT
**  'b'            NONE
**  'c'            NUMERIC
**  'd'            INTEGER
**  'e'            REAL
*/
/* void sqlite3TableAffinityStr(Vdbe *v, Table *pTab){ */
/*   /1* The first time a column affinity string for a particular table */
/*   ** is required, it is allocated and populated here. It is then */
/*   ** stored as a member of the Table structure for subsequent use. */
/*   ** */
/*   ** The column affinity string will eventually be deleted by */
/*   ** sqlite3DeleteTable() when the Table structure itself is cleaned up. */
/*   *1/ */
/*   if( !pTab->zColAff ){ */
/*     char *zColAff; */
/*     int i; */

/*     zColAff = (char *)sqliteMalloc(pTab->nCol+1); */
/*     if( !zColAff ){ */
/*       return; */
/*     } */

/*     for(i=0; i<pTab->nCol; i++){ */
/*       zColAff[i] = pTab->aCol[i].affinity; */
/*     } */
/*     zColAff[pTab->nCol] = '\0'; */

/*     pTab->zColAff = zColAff; */
/*   } */

/*   sqlite3VdbeChangeP3(v, -1, pTab->zColAff, 0); */
/* } */

/*
** Return non-zero if SELECT statement p opens the table with rootpage
** iTab in database iDb.  This is used to see if a statement of the form
** "INSERT INTO <iDb, iTab> SELECT ..." can run without using temporary
** table for the results of the SELECT.
**
** No checking is done for sub-selects that are part of expressions.
*/
static int selectReadsTable(Select *p, Schema *pSchema, int iTab){
  int i;
  struct SrcList_item *pItem;
  if( p->pSrc==0 ) return 0;
  for(i=0, pItem=p->pSrc->a; i<p->pSrc->nSrc; i++, pItem++){
    if( pItem->pSelect ){
      if( selectReadsTable(pItem->pSelect, pSchema, iTab) ) return 1;
    }else{
      if( pItem->pTab->pSchema==pSchema && pItem->pTab->tnum==iTab ) return 1;
    }
  }
  return 0;
}

Insert* sqlite3InsertNew(SrcList *pTabList, ExprList *pSetList, ValuesList *pValuesList, Select *pSelect, IdList *pColumn, int onError) {
    Insert *pNew = NULL;
    pNew = (Insert*)sqliteMalloc(sizeof(*pNew));
    if (pNew == NULL) {
        return NULL;
    }

    pNew->pTabList = pTabList;
    pNew->pValuesList = pValuesList;
    pNew->pSelect = pSelect;
    pNew->pColumn = pColumn;
    pNew->onError = onError;
    pNew->pSetList = pSetList;
    return pNew;
}

void sqlite3InsertDelete(Insert* insertObj) {
    if (insertObj) {
        sqlite3SrcListDelete(insertObj->pTabList);
        sqlite3ExprListDelete(insertObj->pSetList);
        sqlite3ValuesListDelete(insertObj->pValuesList);
        sqlite3SelectDelete(insertObj->pSelect);
        sqlite3IdListDelete(insertObj->pColumn);
        sqliteFree(insertObj);
    }
}

/*
** This routine is call to handle SQL of the following forms:
**
**    insert into TABLE (IDLIST) values(EXPRLIST)
**    insert into TABLE (IDLIST) select
**
** The IDLIST following the table name is always optional.  If omitted,
** then a list of all columns for the table is substituted.  The IDLIST
** appears in the pColumn parameter.  pColumn is NULL if IDLIST is omitted.
**
** The pList parameter holds EXPRLIST in the first form of the INSERT
** statement above, and pSelect is NULL.  For the second form, pList is
** NULL and pSelect is a pointer to the select statement used to generate
** data for the insert.
**
** The code generated follows one of three templates.  For a simple
** select with data coming from a VALUES clause, the code executes
** once straight down through.  The template looks like this:
**
**         open write cursor to <table> and its indices
**         puts VALUES clause expressions onto the stack
**         write the resulting record into <table>
**         cleanup
**
** If the statement is of the form
**
**   INSERT INTO <table> SELECT ...
**
** And the SELECT clause does not read from <table> at any time, then
** the generated code follows this template:
**
**         goto B
**      A: setup for the SELECT
**         loop over the tables in the SELECT
**           gosub C
**         end loop
**         cleanup after the SELECT
**         goto D
**      B: open write cursor to <table> and its indices
**         goto A
**      C: insert the select result into <table>
**         return
**      D: cleanup
**
** The third template is used if the insert statement takes its
** values from a SELECT but the data is being inserted into a table
** that is also read as part of the SELECT.  In the third form,
** we have to use a intermediate table to store the results of
** the select.  The template is like this:
**
**         goto B
**      A: setup for the SELECT
**         loop over the tables in the SELECT
**           gosub C
**         end loop
**         cleanup after the SELECT
**         goto D
**      C: insert the select result into the intermediate table
**         return
**      B: open a cursor to an intermediate table
**         goto A
**      D: open write cursor to <table> and its indices
**         loop over the intermediate table
**           transfer values form intermediate table into <table>
**         end the loop
**         cleanup
*/
void sqlite3Insert(
  Parse *pParse,        /* Parser context */
  SrcList *pTabList,    /* Name of table into which we are inserting */
  ExprList *pSetList,      /* List of values to be inserted, e.g. INSERT INTO test SET id =1; */
  ValuesList *pValuesList, /* List of values*/
  Select *pSelect,      /* A SELECT statement to use as the data source */
  IdList *pColumn,      /* Column names corresponding to IDLIST. */
  int onError           /* How to handle constraint errors */
){
    Insert *insertObj = sqlite3InsertNew(pTabList, pSetList, pValuesList, pSelect, pColumn, onError);
    if (insertObj == NULL) {
        sqlite3ErrorMsg(pParse, "sqlite3InsertNew return NULL, may the malloc failed!");
    }
    ParsedResultItem item;
    item.sqltype = onError == OE_Replace ? SQLTYPE_REPLACE : SQLTYPE_INSERT;
    item.result.insertObj = insertObj;
    sqlite3ParsedResultArrayAppend(&pParse->parsed, &item);
}

/*
** Generate code to do a constraint check prior to an INSERT or an UPDATE.
**
** When this routine is called, the stack contains (from bottom to top)
** the following values:
**
**    1.  The rowid of the row to be updated before the update.  This
**        value is omitted unless we are doing an UPDATE that involves a
**        change to the record number.
**
**    2.  The rowid of the row after the update.
**
**    3.  The data in the first column of the entry after the update.
**
**    i.  Data from middle columns...
**
**    N.  The data in the last column of the entry after the update.
**
** The old rowid shown as entry (1) above is omitted unless both isUpdate
** and rowidChng are 1.  isUpdate is true for UPDATEs and false for
** INSERTs and rowidChng is true if the record number is being changed.
**
** The code generated by this routine pushes additional entries onto
** the stack which are the keys for new index entries for the new record.
** The order of index keys is the same as the order of the indices on
** the pTable->pIndex list.  A key is only created for index i if
** aIdxUsed!=0 and aIdxUsed[i]!=0.
**
** This routine also generates code to check constraints.  NOT NULL,
** CHECK, and UNIQUE constraints are all checked.  If a constraint fails,
** then the appropriate action is performed.  There are five possible
** actions: ROLLBACK, ABORT, FAIL, REPLACE, and IGNORE.
**
**  Constraint type  Action       What Happens
**  ---------------  ----------   ----------------------------------------
**  any              ROLLBACK     The current transaction is rolled back and
**                                sqlite3_exec() returns immediately with a
**                                return code of SQLITE_CONSTRAINT.
**
**  any              ABORT        Back out changes from the current command
**                                only (do not do a complete rollback) then
**                                cause sqlite3_exec() to return immediately
**                                with SQLITE_CONSTRAINT.
**
**  any              FAIL         Sqlite_exec() returns immediately with a
**                                return code of SQLITE_CONSTRAINT.  The
**                                transaction is not rolled back and any
**                                prior changes are retained.
**
**  any              IGNORE       The record number and data is popped from
**                                the stack and there is an immediate jump
**                                to label ignoreDest.
**
**  NOT NULL         REPLACE      The NULL value is replace by the default
**                                value for that column.  If the default value
**                                is NULL, the action is the same as ABORT.
**
**  UNIQUE           REPLACE      The other row that conflicts with the row
**                                being inserted is removed.
**
**  CHECK            REPLACE      Illegal.  The results in an exception.
**
** Which action to take is determined by the overrideError parameter.
** Or if overrideError==OE_Default, then the pParse->onError parameter
** is used.  Or if pParse->onError==OE_Default then the onError value
** for the constraint is used.
**
** The calling routine must open a read/write cursor for pTab with
** cursor number "base".  All indices of pTab must also have open
** read/write cursors with cursor number base+i for the i-th cursor.
** Except, if there is no possibility of a REPLACE action then
** cursors do not need to be open for indices where aIdxUsed[i]==0.
**
** If the isUpdate flag is true, it means that the "base" cursor is
** initially pointing to an entry that is being updated.  The isUpdate
** flag causes extra code to be generated so that the "base" cursor
** is still pointing at the same entry after the routine returns.
** Without the isUpdate flag, the "base" cursor might be moved.
*/
/* void sqlite3GenerateConstraintChecks( */
/*   Parse *pParse,      /1* The parser context *1/ */
/*   Table *pTab,        /1* the table into which we are inserting *1/ */
/*   int base,           /1* Index of a read/write cursor pointing at pTab *1/ */
/*   char *aIdxUsed,     /1* Which indices are used.  NULL means all are used *1/ */
/*   int rowidChng,      /1* True if the record number will change *1/ */
/*   int isUpdate,       /1* True for UPDATE, False for INSERT *1/ */
/*   int overrideError,  /1* Override onError to this if not OE_Default *1/ */
/*   int ignoreDest      /1* Jump to this label on an OE_Ignore resolution *1/ */
/* ){ */
/*   int i; */
/*   Vdbe *v; */
/*   int nCol; */
/*   int onError; */
/*   int addr; */
/*   int extra; */
/*   int iCur; */
/*   Index *pIdx; */
/*   int seenReplace = 0; */
/*   int jumpInst1=0, jumpInst2; */
/*   int hasTwoRowids = (isUpdate && rowidChng); */

/*   v = sqlite3GetVdbe(pParse); */
/*   assert( v!=0 ); */
/*   assert( pTab->pSelect==0 );  /1* This table is not a VIEW *1/ */
/*   nCol = pTab->nCol; */

/*   /1* Test all NOT NULL constraints. */
/*   *1/ */
/*   for(i=0; i<nCol; i++){ */
/*     if( i==pTab->iPKey ){ */
/*       continue; */
/*     } */
/*     onError = pTab->aCol[i].notNull; */
/*     if( onError==OE_None ) continue; */
/*     if( overrideError!=OE_Default ){ */
/*       onError = overrideError; */
/*     }else if( onError==OE_Default ){ */
/*       onError = OE_Abort; */
/*     } */
/*     if( onError==OE_Replace && pTab->aCol[i].pDflt==0 ){ */
/*       onError = OE_Abort; */
/*     } */
/*     sqlite3VdbeAddOp(v, OP_Dup, nCol-1-i, 1); */
/*     addr = sqlite3VdbeAddOp(v, OP_NotNull, 1, 0); */
/*     assert( onError==OE_Rollback || onError==OE_Abort || onError==OE_Fail */
/*         || onError==OE_Ignore || onError==OE_Replace ); */
/*     switch( onError ){ */
/*       case OE_Rollback: */
/*       case OE_Abort: */
/*       case OE_Fail: { */
/*         char *zMsg = 0; */
/*         sqlite3VdbeAddOp(v, OP_Halt, SQLITE_CONSTRAINT, onError); */
/*         sqlite3SetString(&zMsg, pTab->zName, ".", pTab->aCol[i].zName, */
/*                         " may not be NULL", (char*)0); */
/*         sqlite3VdbeChangeP3(v, -1, zMsg, P3_DYNAMIC); */
/*         break; */
/*       } */
/*       case OE_Ignore: { */
/*         sqlite3VdbeAddOp(v, OP_Pop, nCol+1+hasTwoRowids, 0); */
/*         sqlite3VdbeAddOp(v, OP_Goto, 0, ignoreDest); */
/*         break; */
/*       } */
/*       case OE_Replace: { */
/*         sqlite3ExprCode(pParse, pTab->aCol[i].pDflt); */
/*         sqlite3VdbeAddOp(v, OP_Push, nCol-i, 0); */
/*         break; */
/*       } */
/*     } */
/*     sqlite3VdbeJumpHere(v, addr); */
/*   } */

/*   /1* Test all CHECK constraints */
/*   *1/ */
/* #ifndef SQLITE_OMIT_CHECK */
/*   if( pTab->pCheck && (pParse->db->flags & SQLITE_IgnoreChecks)==0 ){ */
/*     int allOk = sqlite3VdbeMakeLabel(v); */
/*     assert( pParse->ckOffset==0 ); */
/*     pParse->ckOffset = nCol; */
/*     sqlite3ExprIfTrue(pParse, pTab->pCheck, allOk, 1); */
/*     assert( pParse->ckOffset==nCol ); */
/*     pParse->ckOffset = 0; */
/*     onError = overrideError!=OE_Default ? overrideError : OE_Abort; */
/*     if( onError==OE_Ignore || onError==OE_Replace ){ */
/*       sqlite3VdbeAddOp(v, OP_Pop, nCol+1+hasTwoRowids, 0); */
/*       sqlite3VdbeAddOp(v, OP_Goto, 0, ignoreDest); */
/*     }else{ */
/*       sqlite3VdbeAddOp(v, OP_Halt, SQLITE_CONSTRAINT, onError); */
/*     } */
/*     sqlite3VdbeResolveLabel(v, allOk); */
/*   } */
/* #endif /1* !defined(SQLITE_OMIT_CHECK) *1/ */

/*   /1* If we have an INTEGER PRIMARY KEY, make sure the primary key */
/*   ** of the new record does not previously exist.  Except, if this */
/*   ** is an UPDATE and the primary key is not changing, that is OK. */
/*   *1/ */
/*   if( rowidChng ){ */
/*     onError = pTab->keyConf; */
/*     if( overrideError!=OE_Default ){ */
/*       onError = overrideError; */
/*     }else if( onError==OE_Default ){ */
/*       onError = OE_Abort; */
/*     } */

/*     if( isUpdate ){ */
/*       sqlite3VdbeAddOp(v, OP_Dup, nCol+1, 1); */
/*       sqlite3VdbeAddOp(v, OP_Dup, nCol+1, 1); */
/*       jumpInst1 = sqlite3VdbeAddOp(v, OP_Eq, 0, 0); */
/*     } */
/*     sqlite3VdbeAddOp(v, OP_Dup, nCol, 1); */
/*     jumpInst2 = sqlite3VdbeAddOp(v, OP_NotExists, base, 0); */
/*     switch( onError ){ */
/*       default: { */
/*         onError = OE_Abort; */
/*         /1* Fall thru into the next case *1/ */
/*       } */
/*       case OE_Rollback: */
/*       case OE_Abort: */
/*       case OE_Fail: { */
/*         sqlite3VdbeOp3(v, OP_Halt, SQLITE_CONSTRAINT, onError, */
/*                          "PRIMARY KEY must be unique", P3_STATIC); */
/*         break; */
/*       } */
/*       case OE_Replace: { */
/*         sqlite3GenerateRowIndexDelete(v, pTab, base, 0); */
/*         if( isUpdate ){ */
/*           sqlite3VdbeAddOp(v, OP_Dup, nCol+hasTwoRowids, 1); */
/*           sqlite3VdbeAddOp(v, OP_MoveGe, base, 0); */
/*         } */
/*         seenReplace = 1; */
/*         break; */
/*       } */
/*       case OE_Ignore: { */
/*         assert( seenReplace==0 ); */
/*         sqlite3VdbeAddOp(v, OP_Pop, nCol+1+hasTwoRowids, 0); */
/*         sqlite3VdbeAddOp(v, OP_Goto, 0, ignoreDest); */
/*         break; */
/*       } */
/*     } */
/*     sqlite3VdbeJumpHere(v, jumpInst2); */
/*     if( isUpdate ){ */
/*       sqlite3VdbeJumpHere(v, jumpInst1); */
/*       sqlite3VdbeAddOp(v, OP_Dup, nCol+1, 1); */
/*       sqlite3VdbeAddOp(v, OP_MoveGe, base, 0); */
/*     } */
/*   } */

/*   /1* Test all UNIQUE constraints by creating entries for each UNIQUE */
/*   ** index and making sure that duplicate entries do not already exist. */
/*   ** Add the new records to the indices as we go. */
/*   *1/ */
/*   extra = -1; */
/*   for(iCur=0, pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext, iCur++){ */
/*     if( aIdxUsed && aIdxUsed[iCur]==0 ) continue;  /1* Skip unused indices *1/ */
/*     extra++; */

/*     /1* Create a key for accessing the index entry *1/ */
/*     sqlite3VdbeAddOp(v, OP_Dup, nCol+extra, 1); */
/*     for(i=0; i<pIdx->nColumn; i++){ */
/*       int idx = pIdx->aiColumn[i]; */
/*       if( idx==pTab->iPKey ){ */
/*         sqlite3VdbeAddOp(v, OP_Dup, i+extra+nCol+1, 1); */
/*       }else{ */
/*         sqlite3VdbeAddOp(v, OP_Dup, i+extra+nCol-idx, 1); */
/*       } */
/*     } */
/*     jumpInst1 = sqlite3VdbeAddOp(v, OP_MakeIdxRec, pIdx->nColumn, 0); */
/*     sqlite3IndexAffinityStr(v, pIdx); */

/*     /1* Find out what action to take in case there is an indexing conflict *1/ */
/*     onError = pIdx->onError; */
/*     if( onError==OE_None ) continue;  /1* pIdx is not a UNIQUE index *1/ */
/*     if( overrideError!=OE_Default ){ */
/*       onError = overrideError; */
/*     }else if( onError==OE_Default ){ */
/*       onError = OE_Abort; */
/*     } */
/*     if( seenReplace ){ */
/*       if( onError==OE_Ignore ) onError = OE_Replace; */
/*       else if( onError==OE_Fail ) onError = OE_Abort; */
/*     } */


/*     /1* Check to see if the new index entry will be unique *1/ */
/*     sqlite3VdbeAddOp(v, OP_Dup, extra+nCol+1+hasTwoRowids, 1); */
/*     jumpInst2 = sqlite3VdbeAddOp(v, OP_IsUnique, base+iCur+1, 0); */

/*     /1* Generate code that executes if the new index entry is not unique *1/ */
/*     assert( onError==OE_Rollback || onError==OE_Abort || onError==OE_Fail */
/*         || onError==OE_Ignore || onError==OE_Replace ); */
/*     switch( onError ){ */
/*       case OE_Rollback: */
/*       case OE_Abort: */
/*       case OE_Fail: { */
/*         int j, n1, n2; */
/*         char zErrMsg[200]; */
/*         strcpy(zErrMsg, pIdx->nColumn>1 ? "columns " : "column "); */
/*         n1 = strlen(zErrMsg); */
/*         for(j=0; j<pIdx->nColumn && n1<sizeof(zErrMsg)-30; j++){ */
/*           char *zCol = pTab->aCol[pIdx->aiColumn[j]].zName; */
/*           n2 = strlen(zCol); */
/*           if( j>0 ){ */
/*             strcpy(&zErrMsg[n1], ", "); */
/*             n1 += 2; */
/*           } */
/*           if( n1+n2>sizeof(zErrMsg)-30 ){ */
/*             strcpy(&zErrMsg[n1], "..."); */
/*             n1 += 3; */
/*             break; */
/*           }else{ */
/*             strcpy(&zErrMsg[n1], zCol); */
/*             n1 += n2; */
/*           } */
/*         } */
/*         strcpy(&zErrMsg[n1], */
/*             pIdx->nColumn>1 ? " are not unique" : " is not unique"); */
/*         sqlite3VdbeOp3(v, OP_Halt, SQLITE_CONSTRAINT, onError, zErrMsg, 0); */
/*         break; */
/*       } */
/*       case OE_Ignore: { */
/*         assert( seenReplace==0 ); */
/*         sqlite3VdbeAddOp(v, OP_Pop, nCol+extra+3+hasTwoRowids, 0); */
/*         sqlite3VdbeAddOp(v, OP_Goto, 0, ignoreDest); */
/*         break; */
/*       } */
/*       case OE_Replace: { */
/*         sqlite3GenerateRowDelete(pParse->db, v, pTab, base, 0); */
/*         if( isUpdate ){ */
/*           sqlite3VdbeAddOp(v, OP_Dup, nCol+extra+1+hasTwoRowids, 1); */
/*           sqlite3VdbeAddOp(v, OP_MoveGe, base, 0); */
/*         } */
/*         seenReplace = 1; */
/*         break; */
/*       } */
/*     } */
/* #if NULL_DISTINCT_FOR_UNIQUE */
/*     sqlite3VdbeJumpHere(v, jumpInst1); */
/* #endif */
/*     sqlite3VdbeJumpHere(v, jumpInst2); */
/*   } */
/* } */

/*
** This routine generates code to finish the INSERT or UPDATE operation
** that was started by a prior call to sqlite3GenerateConstraintChecks.
** The stack must contain keys for all active indices followed by data
** and the rowid for the new entry.  This routine creates the new
** entries in all indices and in the main table.
**
** The arguments to this routine should be the same as the first six
** arguments to sqlite3GenerateConstraintChecks.
*/
/* void sqlite3CompleteInsertion( */
/*   Parse *pParse,      /1* The parser context *1/ */
/*   Table *pTab,        /1* the table into which we are inserting *1/ */
/*   int base,           /1* Index of a read/write cursor pointing at pTab *1/ */
/*   char *aIdxUsed,     /1* Which indices are used.  NULL means all are used *1/ */
/*   int rowidChng,      /1* True if the record number will change *1/ */
/*   int isUpdate,       /1* True for UPDATE, False for INSERT *1/ */
/*   int newIdx          /1* Index of NEW table for triggers.  -1 if none *1/ */
/* ){ */
/*   int i; */
/*   Vdbe *v; */
/*   int nIdx; */
/*   Index *pIdx; */
/*   int pik_flags; */

/*   v = sqlite3GetVdbe(pParse); */
/*   assert( v!=0 ); */
/*   assert( pTab->pSelect==0 );  /1* This table is not a VIEW *1/ */
/*   for(nIdx=0, pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext, nIdx++){} */
/*   for(i=nIdx-1; i>=0; i--){ */
/*     if( aIdxUsed && aIdxUsed[i]==0 ) continue; */
/*     sqlite3VdbeAddOp(v, OP_IdxInsert, base+i+1, 0); */
/*   } */
/*   sqlite3VdbeAddOp(v, OP_MakeRecord, pTab->nCol, 0); */
/*   sqlite3TableAffinityStr(v, pTab); */
/* #ifndef SQLITE_OMIT_TRIGGER */
/*   if( newIdx>=0 ){ */
/*     sqlite3VdbeAddOp(v, OP_Dup, 1, 0); */
/*     sqlite3VdbeAddOp(v, OP_Dup, 1, 0); */
/*     sqlite3VdbeAddOp(v, OP_Insert, newIdx, 0); */
/*   } */
/* #endif */
/*   if( pParse->nested ){ */
/*     pik_flags = 0; */
/*   }else{ */
/*     pik_flags = OPFLAG_NCHANGE; */
/*     pik_flags |= (isUpdate?OPFLAG_ISUPDATE:OPFLAG_LASTROWID); */
/*   } */
/*   sqlite3VdbeAddOp(v, OP_Insert, base, pik_flags); */
/*   if( !pParse->nested ){ */
/*     sqlite3VdbeChangeP3(v, -1, pTab->zName, P3_STATIC); */
/*   } */

/*   if( isUpdate && rowidChng ){ */
/*     sqlite3VdbeAddOp(v, OP_Pop, 1, 0); */
/*   } */
/* } */

/*
** Generate code that will open cursors for a table and for all
** indices of that table.  The "base" parameter is the cursor number used
** for the table.  Indices are opened on subsequent cursors.
*/
/* void sqlite3OpenTableAndIndices( */
/*   Parse *pParse,   /1* Parsing context *1/ */
/*   Table *pTab,     /1* Table to be opened *1/ */
/*   int base,        /1* Cursor number assigned to the table *1/ */
/*   int op           /1* OP_OpenRead or OP_OpenWrite *1/ */
/* ){ */
/*   int i; */
/*   int iDb = sqlite3SchemaToIndex(pParse->db, pTab->pSchema); */
/*   Index *pIdx; */
/*   Vdbe *v = sqlite3GetVdbe(pParse); */
/*   assert( v!=0 ); */
/*   sqlite3OpenTable(pParse, base, iDb, pTab, op); */
/*   for(i=1, pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext, i++){ */
/*     KeyInfo *pKey = sqlite3IndexKeyinfo(pParse, pIdx); */
/*     assert( pIdx->pSchema==pTab->pSchema ); */
/*     sqlite3VdbeAddOp(v, OP_Integer, iDb, 0); */
/*     VdbeComment((v, "# %s", pIdx->zName)); */
/*     sqlite3VdbeOp3(v, op, i+base, pIdx->tnum, (char*)pKey, P3_KEYINFO_HANDOFF); */
/*   } */
/*   if( pParse->nTab<=base+i ){ */
/*     pParse->nTab = base+i; */
/*   } */
/* } */
